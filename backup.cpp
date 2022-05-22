//
// Created by root on 2022/5/22.
//
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

// 判断队列是否为空
bool QueueEmpty(SeqQueue& q)
{
    if (q.size==0)	//队空条件
        return true;
    else
        return false;
}

// 入队
bool EnQueue(SeqQueue& q, ElemType x)
{
    if (q.size==MaxSize)
        return false;		//队列满则报错

    q.data[q.rear] = x;		//将x插入队尾
    q.rear = (q.rear + 1) % MaxSize;    //队尾指针后移
    q.size++;
    return true;
}

// 出队
bool DeQueue(SeqQueue& q, ElemType& x)
{
    if (q.size==0)
        return false;	//队空则报错

    x = q.data[q.front];
    q.front = (q.front + 1) % MaxSize; //队头指针后移
    q.size--;
    return true;
}

// 获取队头元素
bool GetHead(SeqQueue& q, ElemType& x)
{
    if (q.size==0)
        return false;	//队空则报错

    x = q.data[q.front];
    return true;
}

// 队列中元素的个数
int QueueNum(SeqQueue& q)
{
    return q.size;
}

/***************pthread使用***************/
#define all_threads 10  //总线程数
#define io_threads 2  // IO线程数
#define n_threads_one_IO (int)(all_threads / io_threads)
#define txt 10000  //文件中int类型数据的个数
#define outputPath(category, tid, format) #category#tid#format  //定义输出文件的路径
#define batchsize 200  //IO线程每次读取的数据大小

void gen_data(){
    int i;
    int *d = (int *)malloc(txt * sizeof(int));

    for(i = 0; i < txt; i++){
        d[i] = i;
    }
    FILE *fp = fopen("../test.txt", "w");
    fwrite(d, sizeof(int), txt, fp);
    fclose(fp);
}

void read_data(){
    int i;
    int *d = (int *)malloc(txt * sizeof(int));

    FILE *fp = fopen("../test.txt", "r");
    for(i = 0; i < txt; i++){
        fread(d, 4, 100, fp);
    }
    fclose(fp);
}

struct share_data{
    pthread_mutex_t mutex;
    pthread_cond_t cv;
};

struct thread_data{
    int tid, flag, deal_order;  //人为给定的线程号和线程类别：0 - IO线程， 1 - 计算线程
    FILE *fin, *fout;
    SeqQueue input, output;
    thread_data *IO_data;
    share_data *shared;
};

void *io_thread_func(void *data){
    thread_data *my_data = (thread_data *)data;
    while(!feof(my_data->fin)) {
        int *batch_data = (int *) malloc(batchsize * sizeof(int));
        int actual_size = fread(batch_data, sizeof(int), batchsize, my_data->fin);
        for (int i = 0; i < actual_size; i++) {
            EnQueue(my_data->input, batch_data[i]);
        }
        pthread_cond_broadcast(&my_data->shared->cv);

        pthread_mutex_lock(&my_data->shared->mutex);
        if(my_data->output.size){
            fwrite(my_data->output.data + my_data->output.size, sizeof(int), my_data->deal_order - my_data->output.size, my_data->fout);
        }
        else pthread_cond_wait(&my_data->shared->cv, &my_data->shared->mutex);
        pthread_mutex_unlock(&my_data->shared->mutex);
    }
}

void *cal_thread_func(void *data){
    thread_data *my_data = (thread_data *)data;
    int data_id = (int)(my_data->tid / n_threads_one_IO);

    pthread_mutex_lock(&my_data->shared->mutex);
    if(my_data->IO_data[data_id].input.size) {
        fprintf(stderr, "thread %d is waiting.\n", my_data->tid);
        pthread_cond_wait(&my_data->shared->cv, &my_data->shared->mutex);
    }
    pthread_mutex_unlock(&my_data->shared->mutex);

    fprintf(stderr, "thread %d will be calculated.\n", my_data->tid);
    while (my_data->IO_data[data_id].input.size) {
        pthread_mutex_lock(&my_data->shared->mutex);
        if (my_data->IO_data[data_id].input.size < 50) {
            my_data->IO_data[data_id].output.size = my_data->IO_data[data_id].deal_order;
            pthread_cond_broadcast(&my_data->shared->cv);
        }
        pthread_mutex_unlock(&my_data->shared->mutex);
        my_data->IO_data[data_id].output.data[my_data->IO_data[data_id].deal_order] = my_data->IO_data[data_id].input.data[my_data->IO_data[data_id].deal_order];
        __sync_fetch_and_add(&my_data->IO_data[data_id].deal_order, 1);
        sleep(1);
    }
}

#define A(x) #x

int main() {
//    int a = 1;
//    cout << __sync_fetch_and_add(&a, 4) << endl;
//    cout << __sync_fetch_and_add(&a, 4) << endl;
//    cout << __sync_fetch_and_add(&a, 4) << endl;

    /*************generate data**************/
    //gen_data();

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
            data[i].deal_order = 0;
            InitQueue(data[i].input);
            InitQueue(data[i].output);
            data[i].fin = fopen("../test.txt", "r");
            sprintf(outpath, "%s%d%s", "../", i, ".txt");
            data[i].fout = fopen(outpath, "w");
        }
    }

    pthread_t *tid = (pthread_t *)malloc(all_threads * sizeof(pthread_t));
    for(i = 0; i < all_threads; i++) pthread_create(&tid[i], 0, data[i].flag ? cal_thread_func: io_thread_func, &data[i]);

    /*************thread join**************/
    for(i = 0; i < 2; i++) pthread_join(tid[i], 0);

    for(i = 0; i < 2; i++){
        fclose(data[i].fin);
        fclose(data[i].fout);
    }
    free(data);
    free(shared);
}

