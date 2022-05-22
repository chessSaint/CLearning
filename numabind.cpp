#include <iostream>
#include "numa.h"
#include <sys/time.h>

using namespace std;

/***************测试numa_alloc_onnode和malloc的分配内存的性能***************/
void alloc_performance(){
    int N = 200000;
    struct timeval start, end;
    gettimeofday(&start, NULL);
    int **a = (int **)numa_alloc_onnode(N * sizeof(int*), 0);
//    int **a = (int **) malloc(N * sizeof(int *));
    for(int i = 0; i < N; i++){
        a[i] = (int *)numa_alloc_onnode(8 * sizeof(int), 0);
//        a[i] = (int *) malloc(8 * sizeof(int));
        for(int j = 0; j < 8; j++){
            a[i][j] = j;
        }
    }

    gettimeofday(&end, NULL);
    double usedTime = (end.tv_sec - start.tv_sec) * 1.0 + (end.tv_usec - start.tv_usec) * 1.0 / 1000000.0;

    std::cout<<usedTime<<" s."<<std::endl;

    for(int i = 0; i < N; i++){
        numa_free(a[i], 8 * sizeof(int));
//        free(a[i]);
    }
    numa_free(a, N * sizeof(int));
//    free(a);
}
