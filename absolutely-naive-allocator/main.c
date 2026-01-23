#include "alloc.c"
#include <stdio.h>

int main(){
    printf("Start of block 1 : [ 1 ]\n");
    int* arr = (int*)malloc(sizeof(int)*5);
    arr[0] = 1;
    arr[1] = 2;
    arr[2] = 3;
    arr[3] = 4;
    arr[4] = 5;
    for(int i=0;i<5;i++){
        printf("%p: %d\n",arr+i,arr[i]);
    }
    printf("Start of block 2 : [ 1 ] -> [ 2 ]\n");
    int* arr2 = (int*)malloc(sizeof(int)*5);
    arr2[0] = 1;
    arr2[1] = 2;
    arr2[2] = 3;
    arr2[3] = 4;
    arr2[4] = 5;
    for(int i=0;i<5;i++){
        printf("%p: %d\n",arr2+i,arr2[i]);
    }
    printf("Freed block 1 : [ free ] -> [ 2 ]\n");
    free(arr);
    printf("Start of block 3 and the freed block 1 is allocated here : [ 3 ] -> [ 2 ]\n");
    int* arr3 = (int*)malloc(sizeof(int)*5);
    arr3[0] = 1;
    arr3[1] = 2;
    arr3[2] = 3;
    arr3[3] = 4;
    arr3[4] = 5;
    for(int i=0;i<5;i++){
        printf("%p: %d\n",arr3+i,arr3[i]);
    }
    printf("Start of block 4 allocated after block 2 : [ 3 ] -> [ 2 ] -> [ 4 ]\n");
    int* arr4 = (int*)malloc(sizeof(int)*5);
    arr4[0] = 1;
    arr4[1] = 2;
    arr4[2] = 3;
    arr4[3] = 4;
    arr4[4] = 5;
    for(int i=0;i<5;i++){
        printf("%p: %d\n",arr4+i,arr4[i]);
    }
    return 0;
}