#include <iostream>
#include <sys/time.h>
#include <pthread.h>
#include <numa.h>

struct pthread_data
{
    float* a;
    float* b;
    float* c;
    int i;
    // long m;
    long n;
    // long k;
};

void* matrix_multi_pthread(void* data) {
    struct pthread_data * d = (struct pthread_data *) data;
    float* a = d->a;
    float* b = d->b;
    float* c = d->c;
    int id = d->i;
    long n = d->n;
    for(int i = id * (n/64); i < (id+1)*(n/64); i++) c[i] += a[i] * b[i];
//    for(int i = id * (n/8); i < (id+1)*(n/8); i++) c[i] += a[i] * b[i];
    return nullptr;
}

int main() {
    long long N = 640000000;
    float *A = (float *) malloc(N * sizeof(float));
	float *B = (float *) malloc(N * sizeof(float));
	float *C = (float *) malloc(N * sizeof(float));

//     float *a[8], *b[8], *c[8];
//     for(int i = 0; i < 8; i++) {
//         a[i] = (float *) numa_alloc_onnode(N/8 * sizeof(float), i);
//         b[i] = (float *) numa_alloc_onnode(N/8 * sizeof(float), i);
//         c[i] = (float *) numa_alloc_onnode(N/8 * sizeof(float), i);
//     }
//
    double drand48();

	for(int i = 0; i < N; i++) {
        A[i] = 2.0 * (float)drand48() - 1.0;
        B[i] = 2.0 * (float)drand48() - 1.0;
        C[i] = 0;
    }
//     for(int i = 0; i < 8; i++) {
//         for(int j = 0; j < N/8; j++) {
//             a[i][j] = 2.0 * (float)drand48() - 1.0;
//             b[i][j] = 2.0 * (float)drand48() - 1.0;
//             c[i][j] = 0;
//         }
//     }

	int pthread_number = 32;
    pthread_t threads[pthread_number];
    struct pthread_data args[pthread_number];
    cpu_set_t cpuSet;
    pthread_attr_t pthreadAttrs[pthread_number];

    struct timeval start, end;
	gettimeofday(&start, NULL);
	for(int i = 0; i < pthread_number; i++) {
        args[i].a = A;
        args[i].b = B;
        args[i].c = C;
        args[i].i = i;
        args[i].n = N;

//         args[i].a = a[i/8];
//         args[i].b = b[i/8];
//         args[i].c = c[i/8];
//         args[i].i = i%8;
//         args[i].n = N/8;

        pthread_attr_init(&pthreadAttrs[i]);
        CPU_ZERO(&cpuSet);
        CPU_SET(i, &cpuSet);
        pthread_attr_setaffinity_np(&pthreadAttrs[i], sizeof(cpu_set_t), &cpuSet);
        pthread_create(&threads[i], &pthreadAttrs[i], matrix_multi_pthread, (void *)(args + i));
    }

    for (int i = 0; i < pthread_number; i++) pthread_join(threads[i], NULL);
	gettimeofday(&end, NULL);
	double usedTime = (end.tv_sec - start.tv_sec) * 1.0 + (end.tv_usec - start.tv_usec) * 1.0 / 1000000.0;

	std::cout<<usedTime<<" s."<<std::endl;
//     std::cout<<M<<" "<<K<<" "<<N<<std::endl;
//	 std::cout<<M*1.0*K*N*2.0*1.0e-9/usedTime<<" GFLOPS."<<std::endl;
//	 std::cout<<M*1.0*K*N*2.0*1.0e-9/usedTime/17.6/64*100<<" %."<<std::endl;

    free(A);
    free(B);
    free(C);

//     for(int i = 0; i < 8; i++) {
//         for(int j = 0; j < N/8; j++) {
//             std::cout<<a[i][j]<<"*"<<b[i][j]<<"="<<c[i][j]<<std::endl;
//         }
//     }

//     for(int i = 0; i < 8; i++) {
//         numa_free(a[i], N/8 * sizeof(float));
//         numa_free(b[i], N/8 * sizeof(float));
//         numa_free(c[i], N/8 * sizeof(float));
//     }

    return 0;
}
