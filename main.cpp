#include <iostream>
#include "numa.h"
#include <unistd.h>
#include "stdlib.h"
#include <sys/time.h>

using namespace std;

#define all_threads 15  //总线程数
#define group_threads 6
#define io_threads (int)((all_threads + group_threads - 1) / group_threads)  // IO线程数
#define txt 200  //文件中int类型数据的个数
#define batchsize 30  //IO线程每次读取的数据大小
#define MaxSize 70

extern int threshold = 2 * group_threads;

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

struct share_data{
    pthread_mutex_t mutex;
    pthread_cond_t cv;
};

struct thread_data{
    int tid, flag, deal_batch_count, last_output;  //人为给定的线程号和线程类别：0 - IO线程， 1 - 计算线程， 线程处理批次
    FILE *fin, *fout;
    int not_end, send_singal;
    SeqQueue input, output;
    thread_data *IO_data;
    share_data *shared;
};

void *io_thread_func(void *data) {
    thread_data *my_data = (thread_data *) data;

    pthread_mutex_lock(&my_data->shared->mutex);
    pthread_cond_wait(&my_data->shared->cv, &my_data->shared->mutex);
    pthread_mutex_unlock(&my_data->shared->mutex);

    int d, i;
    while (!feof(my_data->fin)) {
        for (i = 0; i < (io_threads - 1) * batchsize; i++) {
            if (fscanf(my_data->fin, "%d\n", &d) == -1) {
                break;
            }
        }
        for (i = 0; i < batchsize; i++) {
            if (fscanf(my_data->fin, "%d\n", &d) == -1) {
                break;
            }
            my_data->input.data[my_data->input.rear++ % MaxSize] = d;
        }
        my_data->input.size += i;
        my_data->send_singal = 1;

        for(i = 0; i < my_data->output.size; i++){
            fprintf(my_data->fout, "%d\n", my_data->output.data[my_data->output.front++ % MaxSize]);
        }
        my_data->output.size = 0;
        pthread_mutex_lock(&my_data->shared->mutex);
        pthread_cond_wait(&my_data->shared->cv, &my_data->shared->mutex);
        pthread_mutex_unlock(&my_data->shared->mutex);
    }
    my_data->not_end = 0;
    pthread_mutex_lock(&my_data->shared->mutex);
    pthread_cond_wait(&my_data->shared->cv, &my_data->shared->mutex);
    if(my_data->output.size){
        for(i = my_data->output.front; i < my_data->output.rear - my_data->last_output + 1; i++){
            fprintf(my_data->fout, "%d\n", my_data->output.data[i % MaxSize]);
        }
    }
    pthread_mutex_unlock(&my_data->shared->mutex);
}

void *cal_thread_func(void *data){
    thread_data *my_data = (thread_data *)data;
    int data_id = (int)(my_data->tid / group_threads);

    int i, j = 0;
//    fprintf(stderr, "tid = %d, input.rear = %d, io_id = %d\n", my_data->tid, my_data->IO_data->input.rear, data_id);
    while (j < my_data->IO_data->input.rear) {
        i = __sync_fetch_and_add(&my_data->IO_data->output.rear, 1);
        j = __sync_fetch_and_add(&my_data->IO_data->input.front, 1);
        my_data->IO_data->output.data[i % MaxSize] = my_data->IO_data->input.data[i % MaxSize];
        if (my_data->IO_data->send_singal && my_data->IO_data->not_end && (my_data->IO_data->input.size - j < threshold)) {
            my_data->IO_data->output.size = i - my_data->IO_data->output.front;  //队尾 - 队头 = 等待写入文件的数据个数
            my_data->IO_data->deal_batch_count++;
            my_data->IO_data->send_singal = 0;
            pthread_cond_signal(&my_data->IO_data->shared->cv);
        }
//        usleep(500);
        sleep(1);
    }
    if(i == j)
    {
        my_data->IO_data->last_output = data_id < (io_threads - 1) ? group_threads : (all_threads % group_threads);
        pthread_cond_signal(&my_data->IO_data->shared->cv);
    }
}

int main() {
    /*************define variable**************/
    int i, j;

    thread_data *data = (thread_data *)malloc(all_threads * sizeof(thread_data));
    share_data **shared = (share_data **)malloc(io_threads * sizeof(share_data*));

    char outpath[20];
    int d, io_id;
    /*************create thread**************/
    for(i = 0; i < all_threads; i++){
        io_id = (int)(i / group_threads);
        data[i].tid = i;
        if(i % group_threads) {
            data[i].flag = 1;
            data[i].IO_data = &data[io_id * group_threads];
        }
        else {
            shared[io_id] = (share_data *)malloc(sizeof(share_data));
            data[i].shared = shared[io_id];
            pthread_mutex_init(&data[i].shared->mutex, 0);  //初始化互斥锁
            pthread_cond_init(&data[i].shared->cv, 0);  //初始化条件变量
            data[i].flag = 0;
            InitQueue(data[i].input);
            InitQueue(data[i].output);
            data[i].output.size = 0;
            data[i].deal_batch_count = 0;
            data[i].fin = fopen("../test.txt", "r");
            for(j = 0; j < io_id * batchsize; j++){
                if(fscanf(data[i].fin, "%d\n", &d) == -1) {
                    data[i].not_end = 0;
                    break;
                }
            }
            for (j = 0; j < batchsize; j++) {
                if(fscanf(data[i].fin, "%d\n", &d) == -1) {
                    data[i].not_end = 0;
                    break;
                }
                data[i].input.data[data[i].input.rear++ % MaxSize] = d;
            }
            data[i].input.size += j;
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
        fclose(data[i * group_threads].fin);
        fclose(data[i * group_threads].fout);
    }
    free(data);
    for(i = 0; i < io_threads; i++){
        free(shared[i]);
    }
    free(shared);
}