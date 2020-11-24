// Alan Luo and Patrick Meng
// afl59 and pm708

#ifndef MY_VM_H_INCLUDED
#define MY_VM_H_INCLUDED
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

//Assume the address space is 32 bits, so the max memory size is 4GB
//Page size is 4KB

//Add any important includes here which you may need

#define PGSIZE 4096

// Maximum size of your memory
#define MAX_MEMSIZE 4ULL*1024*1024*1024 //4GB

#define MEMSIZE 1024ULL*1024*1024

// Represents a page table entry
typedef unsigned long pte_t;

// Represents a page directory entry
typedef unsigned long pde_t;

#define TLB_SIZE 120

//Structure to represents TLB
typedef struct tlb {
    void* va;
    void* pa;
    unsigned long ts;   // Timestamp for 
    //Assume your TLB is a direct mapped TLB of TBL_SIZE (entries)
    // You must also define wth TBL_SIZE in this file.
    //Assume each bucket to be 4 bytes
} tlb_t;
tlb_t tlb_store[TLB_SIZE];


void SetPhysicalMem();
pte_t* Translate(pde_t *pgdir, void *va);
int PageMap(pde_t *pgdir, void *va, void* pa);
bool check_in_tlb(void *va);
void put_in_tlb(void *va, void *pa);
void *myalloc(unsigned int num_bytes);
void myfree(void *va, int size);
void PutVal(void *va, void *val, int size);
void GetVal(void *va, void *val, int size);
void MatMult(void *mat1, void *mat2, int size, void *answer);

// User-defined helpers
void* get_in_tlb(void *va);
float get_tlb_miss_rate();
void print_TLB_missrate();

#endif
