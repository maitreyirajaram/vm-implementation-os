#ifndef MY_VM_H_INCLUDED
#define MY_VM_H_INCLUDED
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

//Assume the address space is 32 bits, so the max memory size is 4GB
//Page size is 4KB

//Add any important includes here which you may need
#include <stdint.h>
#include <err.h>
#include <sysexits.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

#define PGSIZE 4096

// Maximum size of your memory
#define MAX_MEMSIZE 4ULL*1024*1024*1024 //4GB

#define MEMSIZE 1024*1024*1024

// Represents a page table entry
typedef void*  pte_t;

// Represents a page directory entry
typedef pte_t*  pde_t;

#define TLB_SIZE 120

//Structure to represents TLB
struct tlb_entry{
	void* pa;
	void* va;
       int priority_no; 	
};
struct tlb {
	struct tlb_entry* head; 
	struct tlb_entry* tail; 
//	struct tlb_entry tlbentries[TLB_SIZE]; 
    //Assume your TLB is a direct mapped TLB of TBL_SIZE (entries)
    // You must also define wth TBL_SIZE in this file.
    //Assume each bucket to be 4 bytes
};
struct tlb tlb_store;

struct bit_map{
    uint64_t num_bits;
    uint32_t num_ints;
    uint32_t *bm;
};

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
void print_TLB_missrate();

#endif
