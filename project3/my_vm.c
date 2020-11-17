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

// Fetch first free physical frame from bitmap
int getFreeFrame() {
    int i = 0;
    for(; i < num_frames; i++) {
        debug("pbm[%d]: %02x\n", i, pbm[i]);
        if ((pbm[i] & 0x01) == 0) break; // 0: free; 1: in use
    }

    // Assign as in use if in bounds
    if(i < num_frames) {
        pbm[i] = 1;
    } else {
        i = -1;
    }

    debug("Grabbed physical mem frame: %d\n", i);
    return i;
}

/*
    ******* TLB STUFF *******
*/

int tlb_index = 0;
int tlb_curr_size = 0;
unsigned long tlb_total = 0;    // TLB call count
unsigned long tlb_miss = 0;     // Miss count

// Helper function to get physical address from virtual address in the TLB
void* get_in_tlb(void *va) {
    tlb_total++;
    int i = tlb_curr_size;
    while(i >= 0) {
        if(tlb_store[i].va == (void*) ((unsigned long)va >> tbl_shift)) {
            tlb_store[i].ts = tlb_total; // Update timestamp of entry
            return tlb_store[i].pa
        }
        i--;
    }
    tlb_miss++;
    return NULL;
}

void remove_from_tlb(void* va_page_num) {
    int i = tlb_curr_size;
    while(i >= 0) {
        if(tlb_store[i].va == va_page_num) {
            tlb_store[i].va = NULL;
            debug("TLB removed va page: %p\n", va_page_num);
        }
        i--;
    }
}

// As defined in Part 2
// Checks the presence of a translation in TLB
pte_t* check_TLB(void *va) {
    return (pte_t*) get_in_tlb(va);
}

// As defined in .h file
bool check_in_tlb(void *va) {
    bool ret = false;
    if(get_in_tlb(va) != NULL) {
        ret = true;
    }
    return ret;
}

void put_in_tlb(void *va, void *pa) {
    debug("TLB put va: %p, pa: %p\n", va, pa);
    int i;

    if(tlb_curr_size < TLB_SIZE) {
        i = tlb_curr_size;
        tlb_curr_size++;
    } else {
        i = 0;
        for(int x = 0; x < tlb_curr_size; x++) {
            if(tlb_store[x].va != NULL && tlb_store[x].ts < tlb_store[i].ts) {
                i = x;
            }
        }
        // i = rand() % TLB_SIZE;
        debug("TLB is full, override old one: %d\n", i);
    }
    tlb_store[i].va = (void*)((unsigned long)va >> tbl_shift);
    tlb_store[i].pa = pa;
    tlb_store[i].ts = tlb_total;
}

int add_TLB(void* va, void* pa) {
    put_in_tlb(va, pa);
    return -1;
}

float get_tlb_miss_rate() {
    if(TLB) {
        debug("TLB total: %ld, miss: %ld\n", tlb_total, tlb_miss);
        return tlb_miss/(float) tlb_total;
    } else {
        return 1;
    }
}

void print_TLB_missrate() {
    if(TLB) {
        fprintf(stderr, "TLB miss rate %lf \n", get_tlb_miss_rate());
    } else {
        fprintf(stderr, "TLB is disabled\n");
    }
}

// ******** TRANSLATION STUFF *********

/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t * Translate(pde_t *pgdir, void *va) {
    //HINT: Get the Page directory index (1st level) Then get the
    //2nd-level-page table index using the virtual address.  Using the page
    //directory index and page table index get the physical address

    // For thread safety
    pthread_mutex_lock(&my_vm_mutex);

    // Check invalid access
    if((vbm[(unsigned long)va >> num_page_bits] & 0x03) == 0)

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
