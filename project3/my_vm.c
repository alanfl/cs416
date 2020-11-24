// Alan Luo and Patrick Meng
// afl59 and pm708

#include "my_vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define DEBUG 0
#define debug(...) \
    do { if (DEBUG) fprintf(stderr, __VA_ARGS__); } while (0)

#define TLB 0

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
    num_entries = 1 << ((int)logTwo(num_pages) / 2); // 2nd level page table entries
    num_dirs = 1 << logTwo(num_pages) - logTwo(num_entries);    // Dir table entries
    num_page_bits = logTwo(PGSIZE);
    num_tbl_bits = logTwo(num_entries);

    debug("Frames: %d, Pages: %d, DirTableEntries: %d, PageTableEntries: %d\n", num_frames, num_pages, num_dirs, num_entries);

    tbl_shift = logTwo(PGSIZE);
    offset_mask = (unsigned long) 0xffffffff >> ((sizeof(unsigned long) * 8 - tbl_shift));
    dir_shift = logTwo(num_entries) + tbl_shift;
    tbl_mask = (unsigned long) 0xffffffff >> (32 - num_tbl_bits);
    debug("TableShift: %d, PageOffsetMask: %p, 1stTableShift: %d, 2ndTableMask: %p\n",
        tbl_shift, (void*)offset_mask, dir_shift, (void*)tbl_mask);

    // create vm bitmap
    vbm = calloc(MAX_MEMSIZE/PGSIZE, sizeof(char));
    vbm[0] = 0xff; // reserv as header;
    // create pm bitmap
    pbm = calloc(MEMSIZE/PGSIZE, sizeof(char));

    // 1st level page table
    pgdir = (pde_t*) calloc(num_dirs, sizeof(pde_t));

    // Init first 2nd level page table for optimization
    pgdir[0] = (pte_t) calloc(num_entries, sizeof(pte_t));
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
            return tlb_store[i].pa;
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

/****
    The function takes a virtual address and page directories starting address and
    performs translation to return the physical address
****/
pte_t * Translate(pde_t *pgdir, void *va) {
    pthread_mutex_lock(&my_vm_mutex);
    // Check invalid access
    if((vbm[(unsigned long)va >> num_page_bits] & 0x03) == 0) {
        printf("Error: invalid memory access at address: %p\n", va);
        abort();
    }

    // Check TLB for translation
    void* pa;
    void* pa_base = NULL;
    if(TLB) {
        pa_base = get_in_tlb(va);
    }

    // Couldn't find in TLB, perform translation (and allocation)
    if(pa_base == NULL) {
        int dirOffset = getDirOffset(va);
        if ((void*)(pgdir[dirOffset]) == NULL) {
            pgdir[dirOffset] = (pte_t)calloc(num_entries, sizeof(pte_t));  // create 2nd level page table
        }
        int tblOffset = getTblOffset(va);
        if((void*)(((pte_t*)pgdir[dirOffset])[tblOffset]) == NULL) {
            long frameOffset = (unsigned long)getFreeFrame();
            if(frameOffset < 0) {
                debug("Error: not enough physical memory\n");
                abort();
            }
            PageMap(pgdir, va, pm + PGSIZE * frameOffset);
        }
        pa_base = (void*) (((pte_t*)pgdir[dirOffset])[tblOffset]);

        // Add into the TLB
        if(TLB) {
            put_in_tlb(va, pa_base);
        }
    }
    pthread_mutex_unlock(&my_vm_mutex);

    pa = (void*) (pa_base + getPageOffset(va));
    debug("Translated va: %p, pa: %p\n", va, pa);
    return (pte_t*) pa;
}


/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int PageMap(pde_t *pgdir, void *va, void *pa) {
    ((pte_t*) pgdir[getDirOffset(va)])[getTblOffset(va)] = (pte_t)pa;
    return 0;
}


/*Function that gets the next available page
*/
void *get_next_avail(int num_of_pages) {
    //Use virtual address bitmap to find the next free page

    int more_pages = num_of_pages;
    int i = 0;

    while(more_pages > 0) {
        // Not in use, okay to assign
        if((vbm[i] & 0x03) == 0) {
            more_pages--;
        }
        // In use, must reset for contiguous block
        else {
            more_pages = num_of_pages;
        }
        i++;
    }

    // Begin allocating
    int start = i - num_of_pages;
    vbm[start] = vbm[start] & 0xfd | 0x01; // Mark as start of in use block
    for(int j = 1; j < num_of_pages - 1; j++) {
        vbm[start + j] = vbm[start + j] & 0xfd | 0x02; // Mark as in use, middle of block
    }
    vbm[start + num_of_pages - 1] = vbm[start + num_of_pages - 1] & 0xfd | 0x03; // Mark as in use, end of block
    debug("Allocated virtual memory: %p\n", (void*)((i - num_of_pages) * PGSIZE));
    return (void*)((i - num_of_pages) * PGSIZE);
}


/* Function responsible for allocating pages
and used by the benchmark
*/
void *myalloc(unsigned int num_bytes) {
    init();
    void* va = NULL;
    pthread_mutex_lock(&my_vm_mutex);
    va = get_next_avail((num_bytes + PGSIZE - 1)/PGSIZE); // ceil equivalent
    pthread_mutex_unlock(&my_vm_mutex);
    return va;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
void myfree(void *va, int size) {
    if(va == NULL) return;
    init();
    unsigned long start_index = (unsigned long) va >> num_page_bits;
    unsigned long end_index = ((unsigned long) va + size - 1) >> num_page_bits;
    debug("free called on va: %p, size: %d, start: %lu, end: %lu\n", va, size, start_index, end_index);
    debug("start bitmap: %02x, end bitmap: %02x\n", vbm[start_index], vbm[end_index]);

    pthread_mutex_lock(&my_vm_mutex);
    if(((start_index == end_index) && ((vbm[start_index] & 0x03) == 3)) || 
      (((vbm[start_index] & 0x03) == 1) && ((vbm[end_index] & 0x03) == 3)) ) {
        debug("start and end match\n");
        for(unsigned long i = start_index + 1; i < end_index; i++) {
            if((vbm[i] & 0x03) != 2) {
                debug("Virtual address in between pages is invalid\n");
                pthread_mutex_unlock(&my_vm_mutex);
                return;
            }
        }
        for(unsigned long i = start_index; i <= end_index; i++) {
            remove_from_tlb((void*) i); // Freeing, so we need to remove from TLB
            unsigned long dir_offset = i >> (dir_shift - tbl_shift);
            if((void*)(pgdir[dir_offset]) != NULL) {
                unsigned long tbl_offset = i & tbl_mask;
                void* pa = (void*)(((pte_t*) pgdir[dir_offset])[tbl_offset]);

                // 0 is NULL pointer
                ((pte_t*) pgdir[dir_offset])[tbl_offset] = 0; // Clear 2nd level page table
                if(pa != NULL) {
                    unsigned long f = ((unsigned long) pa - (unsigned long) pm) / PGSIZE;
                    pbm[f] = pbm[f] & 0xfe; // Clear physical bit map
                    debug("Freed physical frame pbm[%lu]: %02x\n", f, pbm[f]);
                }
            }
            vbm[i] = vbm[i] & 0xfc; // Clear virtual bit map
            debug("Freed virtual vbm[%lu]: %02x\n", i, vbm[i]);
        }
        debug("Freed virtual mem from: %lu to %lu\n", start_index, end_index);
    } else {
        debug("Virtual address start or end page was invalid\n");
    }
    pthread_mutex_unlock(&my_vm_mutex);
}

void PutVal(void *va, void *val, int size) {
    init();
    int first = PGSIZE - getPageOffset(va);
    if(first > size) {
        first = size;
    }

    int others = (size - first) / PGSIZE;
    int last = size - first - others * PGSIZE;
    if(last < 0) {
        last = 0;
    }
    debug("PutVal va: %p, val: %p, first: %d, others: %d, last: %d\n", va, val, first, others, last);

    void* pa;
    // First page
    if(first > 0) {
        pa = (void*) Translate(pgdir, va);
        memcpy(pa, val, first);
    }

    // Other pages
    va += first;
    val += first;
    for(int i = 0; i < others; i++) {
        pa = (void*) Translate(pgdir, va);
        memcpy(pa, val, PGSIZE);
        va += PGSIZE;
        val += PGSIZE;
    }

    // Last page
    if(last > 0) {
        pa = (void*) Translate(pgdir, va);
        memcpy(pa, val, last);
    }
}


/*Given a virtual address, this function copies the contents of the page to val*/
void GetVal(void *va, void *val, int size) {
    init();
    int first = PGSIZE - getPageOffset(va);
    if(first > size) {
        first = size;
    }
    int others = (size - first) / PGSIZE;
    int last = size - first - others * PGSIZE;
    if(last < 0) {
        last = 0;
    }
    debug("GetVal va: %p, val: %p, first: %d, others: %d, last:%d\n", va, val, first, others, last);

    void* pa;
    // First page
    if(first > 0) {
        pa = (void*) Translate(pgdir, va);
        memcpy(val, pa, first);
    }

    // Other pages
    va += first;
    val += first;
    for(int i = 0; i < others; i++) {
        pa = (void*) Translate(pgdir, va);
        memcpy(val, pa, PGSIZE);
        va += PGSIZE;
        val += PGSIZE;
    }

    // Last page
    if(last > 0) {
        pa = (void*) Translate(pgdir, va);
        memcpy(val, pa, last);
    }
}

void MatMult(void *mat1, void *mat2, int size, void *answer) {
    int x, y, temp;
    for(int i = 0; i < size; i++) {
        for(int j = 0; j < size; j++) {
            temp = 0;
            for (int k = 0; k < size; k++) {
                GetVal(mat1 + (i * size * sizeof(int)) + k * sizeof(int), &x, sizeof(int));
                GetVal(mat2 + (k * size * sizeof(int)) + j * sizeof(int), &y, sizeof(int));
                temp += x * y;
                PutVal(answer + (i * size * sizeof(int)) + j * sizeof(int), &temp, sizeof(int));
                // answer[i][j] += mat1[i][k] * mat2[k][j];
            }
        }
    }       
}
