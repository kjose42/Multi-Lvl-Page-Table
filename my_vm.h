#ifndef MY_VM_H_INCLUDED
#define MY_VM_H_INCLUDED
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

//Assume the address space is 32 bits, so the max memory size is 4GB
//Page size is 4KB

//Add any important includes here which you may need
#include <string.h>
#include <pthread.h>

#define PGSIZE 4096

// Maximum size of virtual memory
#define MAX_MEMSIZE 4ULL*1024*1024*1024

// Size of "physcial memory"
#define MEMSIZE 1024*1024*1024

// Represents a page table entry
typedef unsigned long pte_t;

// Represents a page directory entry
typedef unsigned long pde_t;

#define TLB_ENTRIES 512

typedef struct tlb_entry{
	int valid;
	void* virtual_address;
	void* physical_address;
} tlb_entry;

//Structure to represents TLB
struct tlb {
    /*
    * TLB is a direct mapped TLB with number of entries as TLB_ENTRIES
    */
    tlb_entry entries [TLB_ENTRIES];
};
struct tlb tlb_store;


void set_physical_mem();
pte_t* translate(pde_t *pgdir, void *va);
int page_map(pde_t *pgdir, void *va, void* pa);
bool check_in_tlb(void *va);
void put_in_tlb(void *va, void *pa);
void *t_malloc(unsigned int num_bytes);
void t_free(void *va, int size);
void put_value(void *va, void *val, int size);
void get_value(void *va, void *val, int size);
void mat_mult(void *mat1, void *mat2, int size, void *answer);
void print_TLB_missrate();

// Our helper functions
unsigned long search_bitmap_for_pages(char *bitmap, int num_pages, int bitmap_length);
int get_bit_at_index(char *bitmap, int index);
void set_bit_at_index(char *bitmap, int index);
void clear_bit_at_index(char *bitmap, int index);
void cleanup();
int page_unmap(pde_t *pgdir, void *va);

#endif
