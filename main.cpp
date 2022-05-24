#include <iostream>
#include "numa.h"
#include <unistd.h>
#include "stdlib.h"
#include <sys/time.h>

using namespace std;

/*******************FIFO queue realize*******************/
// 不浪费存储空间
// 设置一个变量size，记录队列当中存储元素的个数
// 队尾指针指向 队尾元素的后一个位置（下一个应该插入的位置）

#define all_threads 2  //总线程数
#define io_threads 1  // IO线程数
#define n_threads_one_IO (int)(all_threads / io_threads)
#define txt 2201  //文件中int类型数据的个数
#define outputPath(category, tid, format) #category#tid#format  //定义输出文件的路径
#define batchsize 200  //IO线程每次读取的数据大小
#define MaxSize 500
typedef int ElemType;
typedef struct{
    ElemType data[MaxSize];  //存放队列元素
    int front;  //队头指针
    int rear;  //队尾指针
    int size;	//队列当前长度
}SeqQueue;

// 初始化队列
void InitQueue(SeqQueue& q)
{
    q.front = q.rear = 0;
    q.size = 0;		//队列当前长度为0
}

/***************pthread使用***************/


void gen_data(){
    int i;
    int *d = (int *)malloc(txt * sizeof(int));

    FILE *fp = fopen("../test.txt", "w");
    for(i = 0; i < txt; i++){
        fprintf(fp, "%d\n", i);
    }
    fclose(fp);
}

void read_data(){
    int i;
    int d;
    FILE *fp = fopen("../0.txt", "r");
    for(i = 0; i < txt; i++){
        fscanf(fp, "%d\n", &d);
        if(d != i)
            fprintf(stderr, "error!%d\n", i);
    }
    fclose(fp);
}

struct share_data{
    pthread_mutex_t mutex;
    pthread_cond_t cv;
};

struct thread_data{
    int tid, flag, deal_batch_count;  //人为给定的线程号和线程类别：0 - IO线程， 1 - 计算线程， 线程处理批次
    FILE *fin, *fout;
    int not_end, send_singal;
    SeqQueue input, output;
    thread_data *IO_data;
    share_data *shared;
};



void *io_thread_func(void *data){
    thread_data *my_data = (thread_data *)data;
    int d, i;
    while(!feof(my_data->fin)) {
        fseek(my_data->fin, (io_threads - 1) * batchsize, SEEK_CUR);
        for (i = 0; i < batchsize; i++) {
            if(fscanf(my_data->fin, "%d\n", &d) == -1) {
                break;
            }
            my_data->input.data[my_data->input.rear++ % MaxSize] = d;
        }

        my_data->input.size += i;
        my_data->send_singal = 1;
        if(!my_data->input.front)
            pthread_cond_signal(&my_data->shared->cv);

        pthread_mutex_lock(&my_data->shared->mutex);
        if(my_data->output.size){
            for(i = my_data->output.front; i < my_data->output.front + my_data->output.size; i++){
                fprintf(my_data->fout, "%d\n", my_data->output.data[i % MaxSize]);
            }
            my_data->output.front += my_data->output.size;
            my_data->output.size = 0;
            pthread_cond_wait(&my_data->shared->cv, &my_data->shared->mutex);
        } else pthread_cond_wait(&my_data->shared->cv, &my_data->shared->mutex);
        pthread_mutex_unlock(&my_data->shared->mutex);
    }
    my_data->not_end = 0;
    pthread_mutex_lock(&my_data->shared->mutex);
    pthread_cond_wait(&my_data->shared->cv, &my_data->shared->mutex);
    if(my_data->output.size){
        for(i = my_data->output.front; i < my_data->output.rear - 1; i++){
            fprintf(my_data->fout, "%d\n", my_data->output.data[i % MaxSize]);
        }
        my_data->output.front += my_data->output.size;
        my_data->output.size = 0;
    }
    pthread_mutex_unlock(&my_data->shared->mutex);
}

void *cal_thread_func(void *data){
    thread_data *my_data = (thread_data *)data;
    int data_id = (int)(my_data->tid / n_threads_one_IO);

    pthread_mutex_lock(&my_data->shared->mutex);
    if(!my_data->IO_data[data_id].input.rear) {
        pthread_cond_wait(&my_data->shared->cv, &my_data->shared->mutex);
    }
    pthread_mutex_unlock(&my_data->shared->mutex);

    int i = 0, j = 0;
    while (j < my_data->IO_data[data_id].input.rear) {
        i = __sync_fetch_and_add(&my_data->IO_data[data_id].output.rear, 1);
        j = __sync_fetch_and_add(&my_data->IO_data[data_id].input.front, 1);
        if (my_data->IO_data[data_id].send_singal && my_data->IO_data[data_id].not_end && my_data->tid % n_threads_one_IO == 1 && my_data->IO_data[data_id].input.size - i < 10) {
            my_data->IO_data[data_id].output.size = i - my_data->IO_data[data_id].output.front;  //队尾 - 队头 = 等待写入文件的数据个数
//            fprintf(stderr, "%d\t%d\n", my_data->IO_data[data_id].input.size, my_data->IO_data[data_id].input.rear);
            my_data->IO_data[data_id].deal_batch_count++;
            my_data->IO_data[data_id].send_singal = 0;
            pthread_cond_signal(&my_data->shared->cv);
        }

        my_data->IO_data[data_id].output.data[i % MaxSize] = my_data->IO_data[data_id].input.data[j % MaxSize];
        usleep(500);
//        sleep(1);
    }
    pthread_cond_signal(&my_data->shared->cv);
}

int main() {
    /*************generate data**************/
//    gen_data();
//    read_data();

    /*************define variable**************/
    int i;

    thread_data *data = (thread_data *)malloc(io_threads * sizeof(thread_data));
    share_data *shared = (share_data *)malloc(sizeof(share_data));
    pthread_mutex_init(&shared->mutex, 0);  //初始化互斥锁
    pthread_cond_init(&shared->cv, 0);  //初始化条件变量

    char outpath[20];
    /*************create thread**************/
    for(i = 0; i < all_threads; i++){
        data[i].tid = i;
        data[i].shared = shared;
        if(i % n_threads_one_IO) {
            data[i].flag = 1;
            data[i].IO_data = &data[(int)(i / n_threads_one_IO)];
        }
        else {
            data[i].flag = 0;
            InitQueue(data[i].input);
            InitQueue(data[i].output);
            data[i].output.size = 0;
            data[i].deal_batch_count = 0;
            data[i].fin = fopen("../test.txt", "r");
            sprintf(outpath, "%s%d%s", "../", i, ".txt");
            data[i].fout = fopen(outpath, "w");
            data[i].not_end = 1;
            data[i].send_singal = 1;
        }
    }

    pthread_t *tid = (pthread_t *)malloc(all_threads * sizeof(pthread_t));
    for(i = 0; i < all_threads; i++) pthread_create(&tid[i], 0, data[i].flag ? cal_thread_func : io_thread_func, &data[i]);

    /*************thread join**************/
    for(i = 0; i < all_threads; i++) pthread_join(tid[i], 0);

    for(i = 0; i < io_threads; i++){
        fclose(data[i * n_threads_one_IO].fin);
        fclose(data[i * n_threads_one_IO].fout);
    }
    free(data);
    free(shared);
}