#include "my_vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define DEBUG 0
#define debug(...) \
    do { if (DEBUG) fprintf(stderr, __VA_ARGS__); } while (0)

#define TLB 1

// For making virtual memory thread-safe
pthread_mutex_t my_vm_mutex;

// Vars for shifting and/or masking
int num_pages, num_dirs, num_entries, num_frames;
int num_page_bits, num_tbl_bits;
int tbl_shift, dir_shift;
unsigned long offset_mask, tbl_mask;

void* pm;   // Physical memory
char* vbm;   // Virtual bit map
char* pbm;   // Physical bit map
pde_t* pgdir;

int init_flag = 0;

void init() {
    if (init_flag) return;
    
    init_flag = 1;

    pthread_mutex_init(&my_vm_mutex, NULL);

    time_t t;
    srand((unsigned) time(&t)); // TLB replace policy is random for now
    SetPhysicalMem();
}

// Get the offset within the page
int getPageOffset(void* va) {
    return (unsigned long) va & offset_mask;
}

// Get index to the 2nd level page table
int getTblOffset(void* va) {
    return ((unsigned long) va >> tbl_shift) & tbl_mask;
}

// Get index to the 1st level page table
int getDirOffset(void* va) {
    return ((unsigned long) va >> dir_shift);
}

// log2 function so we don't have to link with math lib
int logTwo(unsigned long long x) {
    int i = 0;
    while (x > 1) {
        x = x >> 1;
        i++;
    }
    return i;
}

/*
Function responsible for allocating and setting your physical memory 
*/
void SetPhysicalMem() {

    //Allocate physical memory using mmap or malloc; this is the total size of
    //your memory you are simulating
    unsigned long long vMemSize = MAX_MEMSIZE;
    unsigned long pMemSize = MEMSIZE;
    debug("Page size: %d, vMemSize: %llu, pMemSize: %lu\n", PGSIZE, vMemSize, pMemSize);

    pm = malloc(MEMSIZE);
    debug("Allocated physical memory at: %p\n", pm);

    // Calculate vars
    num_frames = MEMSIZE / PGSIZE;
    num_pages = MAX_MEMSIZE / PGSIZE;
    num_entries = PGSIZE / sizeof(pte_t);                       // Page table entries
    num_dirs = 1 << logTwo(num_pages) - logTwo(num_entries);    // Dir table entries
    num_page_bits = logTwo(PGSIZE);
    num_tbl_bits = logTwo(num_entries);

    debug("Frames: %d, Pages: %d, DirTableEntries: %d, PageTableEntries: %d\n", num_frames, num_pages, num_dirs, num_entries);

    tbl_shift = logTwo(PGSIZE);
    offset_mask = (unsigned long) 0xffffffff >> ((sizeof(unsigned long) * 8 - tbl_shift));
    dir_shift = logTwo(num_entries) + tbl_shift;
    tbl_mask = (unsigned long) 0xffffffff >> (32 - num_tbl_bits);

    debug("TableShift: %d, PageOffsetMaks: %p, 1stTblShift: %d, 2ndTblShift: %p\n",
        tbl_shift, (void*) offset_mask, dir_shift, (void*) tbl_mask);

    // Bitmaps
    vbm = calloc(MAX_MEMSIZE / PGSIZE, sizeof(char));
    vbm[0] = 0xff;  // Reserve as the header

    pbm = calloc(MEMSIZE / PGSIZE, sizeof(char));

    // 1st level page table
    pgdir = (pde_t*) calloc(num_dirs, sizeof(pde_t));

    // Init first 2nd level page table for optimization
    pgdir[0] = (pte_t) calloc(num_entries, sizeof(pte_t));

    //HINT: Also calculate the number of physical and virtual pages and allocate
    //virtual and physical bitmaps and initialize them
}



/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t * Translate(pde_t *pgdir, void *va) {
    //HINT: Get the Page directory index (1st level) Then get the
    //2nd-level-page table index using the virtual address.  Using the page
    //directory index and page table index get the physical address


    //If translation not successfull
    return NULL; 
}


/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int
PageMap(pde_t *pgdir, void *va, void *pa)
{

    /*HINT: Similar to Translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping */

    return -1;
}


/*Function that gets the next available page
*/
void *get_next_avail(int num_pages) {
 
    //Use virtual address bitmap to find the next free page
}


/* Function responsible for allocating pages
and used by the benchmark
*/
void *myalloc(unsigned int num_bytes) {

    //HINT: If the physical memory is not yet initialized, then allocate and initialize.

   /* HINT: If the page directory is not initialized, then initialize the
   page directory. Next, using get_next_avail(), check if there are free pages. If
   free pages are available, set the bitmaps and map a new page. Note, you will 
   have to mark which physical pages are used. */

    return NULL;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
void myfree(void *va, int size) {

    //Free the page table entries starting from this virtual address (va)
    // Also mark the pages free in the bitmap
    //Only free if the memory from "va" to va+size is valid
}


/* The function copies data pointed by "val" to physical
 * memory pages using virtual address (va)
*/
void PutVal(void *va, void *val, int size) {

    /* HINT: Using the virtual address and Translate(), find the physical page. Copy
       the contents of "val" to a physical page. NOTE: The "size" value can be larger
       than one page. Therefore, you may have to find multiple pages using Translate()
       function.*/

}


/*Given a virtual address, this function copies the contents of the page to val*/
void GetVal(void *va, void *val, int size) {

    /* HINT: put the values pointed to by "va" inside the physical memory at given
    "val" address. Assume you can access "val" directly by derefencing them.
    If you are implementing TLB,  always check first the presence of translation
    in TLB before proceeding forward */


}



/*
This function receives two matrices mat1 and mat2 as an argument with size
argument representing the number of rows and columns. After performing matrix
multiplication, copy the result to answer.
*/
void MatMult(void *mat1, void *mat2, int size, void *answer) {

    /* Hint: You will index as [i * size + j] where  "i, j" are the indices of the
    matrix accessed. Similar to the code in test.c, you will use GetVal() to
    load each element and perform multiplication. Take a look at test.c! In addition to 
    getting the values from two matrices, you will perform multiplication and 
    store the result to the "answer array"*/

       
}
