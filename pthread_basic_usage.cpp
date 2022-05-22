#include <iostream>
#include "numa.h"
#include <sys/time.h>

using namespace std;

/***************pthread使用***************/
#define N 16
#define num_thread 4
struct worker_t{
//    int a[N / num_thread], b[N / num_thread], c[N / num_thread];
    int *a, *b, *c;
    int num;
};

void *worker(void *data){
    struct worker_t *w = (worker_t *)data;
    for(int i = 0; i < w->num; i++){
        w->c[i] = w->a[i] + w->b[i];
    }
}

void basic_usage(){
    int a[N], b[N], c[N];
    int i, j;
    for(i = 0; i < N; i++){
        a[i] = i;
        b[i] = i;
        c[i] = 0;
    }

    worker_t *worker_data = (worker_t *) malloc(num_thread * sizeof(worker_t));
    for(i = 0; i < num_thread; i++){
        worker_data[i].a = a + i * N / num_thread;
        worker_data[i].b = b + i * N / num_thread;
        worker_data[i].c = c + i * N / num_thread;
        worker_data[i].num = N / num_thread;
    }

    pthread_t *tid = (pthread_t *) malloc(N * sizeof(pthread_t));
    for(i = 0; i < num_thread; i++) pthread_create(&tid[i], NULL, worker, &worker_data[i]);
    for(i = 0; i < num_thread; i++) pthread_join(tid[i], NULL);

    for(i = 0; i < N; i++){
        cout << c[i] << '\t';
    }
}

int main(){
    basic_usage();
}
