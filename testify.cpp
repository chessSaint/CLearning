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

#define all_threads 5  //总线程数
#define io_threads 1  // IO线程数
#define group_threads (int)(all_threads / io_threads)
#define txt 200  //文件中int类型数据的个数
#define batchsize 30  //IO线程每次读取的数据大小
#define MaxSize 50

extern int threshold = 3 * group_threads;

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


int main() {
    /*************generate data**************/
//    gen_data();
    read_data();
}