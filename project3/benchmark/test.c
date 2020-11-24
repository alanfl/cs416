#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "../my_vm.h"

int SIZE = 5;

int main(int argc, char** argv) {
    if (argc == 2)
        SIZE = atoi(argv[1]);

    struct timespec start, end;
    clock_gettime(CLOCK_REALTIME, &start);

    printf("Allocating three arrays of %d bytes\n", SIZE*SIZE*4);
    void *a = myalloc(SIZE*SIZE*4);
    int old_a = (int)a;
    void *b = myalloc(SIZE*SIZE*4);
    void *c = myalloc(SIZE*SIZE*4);
    int x = 1;
    int y, z;
    int i =0, j=0;
    int address_a = 0, address_b = 0;
    int address_c = 0;

    printf("Addresses of the allocations: %x, %x, %x\n", (int)a, (int)b, (int)c);

    printf("Storing integers to generate a SIZExSIZE matrix\n");
    for (i = 0; i < SIZE; i++) {
        for (j = 0; j < SIZE; j++) {
            address_a = (unsigned int)a + ((i * SIZE * sizeof(int))) + (j * sizeof(int));
            address_b = (unsigned int)b + ((i * SIZE * sizeof(int))) + (j * sizeof(int));
            PutVal((void *)address_a, &x, sizeof(int));
            PutVal((void *)address_b, &x, sizeof(int));
        }
    } 

    printf("Fetching matrix elements stored in the arrays\n");

    for (i = 0; i < SIZE; i++) {
        for (j = 0; j < SIZE; j++) {
            address_a = (unsigned int)a + ((i * SIZE * sizeof(int))) + (j * sizeof(int));
            address_b = (unsigned int)b + ((i * SIZE * sizeof(int))) + (j * sizeof(int));
            GetVal((void *)address_a, &y, sizeof(int));
            GetVal( (void *)address_b, &z, sizeof(int));
            printf("%d ", y);
        }
        printf("\n");
    } 

    printf("Performing matrix multiplication with itself!\n");
    MatMult(a, b, SIZE, c);


    for (i = 0; i < SIZE; i++) {
        for (j = 0; j < SIZE; j++) {
            address_c = (unsigned int)c + ((i * SIZE * sizeof(int))) + (j * sizeof(int));
            GetVal((void *)address_c, &y, sizeof(int));
            printf("%d ", y);
        }
        printf("\n");
    }
    printf("Freeing the allocations!\n");
    myfree(a, SIZE*SIZE*4);
    myfree(b, SIZE*SIZE*4);
    myfree(c, SIZE*SIZE*4);

    printf("Checking if allocations were freed!\n");
    a = myalloc(100*4);
    printf("a: %p\n", a);
    if ((int)a == old_a)
        printf("free function works\n");
    else
        printf("free function does not work\n");

    clock_gettime(CLOCK_REALTIME, &end);
    printf("running time: %lu nano-seconds\n",
        (end.tv_sec - start.tv_sec) * 1000 * 1000 * 1000 + (end.tv_nsec - start.tv_nsec));

    print_TLB_missrate();
    return 0;
}
