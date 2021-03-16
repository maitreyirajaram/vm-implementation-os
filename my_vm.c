#include "my_vm.h"

/*
Function responsible for allocating and setting your physical memory
*/
void*** PGDIR; 
void* PHYSICAL_MEM_START;
unsigned long NUM_PHYSICAL_PAGES;
unsigned long NUM_VIRTUAL_PAGES;
struct bit_map* PHYSICAL_PAGE_BMP; 
struct bit_map* VIRTUAL_PAGE_BMP;
unsigned long OFFSET_BITS;
unsigned long TOTAL_BITS; 
unsigned long PGDIR_BITS; 
unsigned long PGTBL_BITS; 
unsigned long PGTBL_SIZE; 
unsigned long PGDIR_ARRAY_SIZE; //no of rows (ie no of page tables)
pthread_mutex_t PutVal_mutex;
pthread_mutex_t Free_mutex;
pthread_mutex_t malloc_mutex;

//bitmap implementation methods: 
struct bit_map* bmp_init(uint64_t bits){

    struct bit_map *b = (struct bit_map *) malloc(sizeof(struct bit_map));
    if (b == NULL)
        err(1, "calloc error in bit_map_init().\n");

    b->num_bits = bits;
    b->num_ints = (bits + 32 - 1) / 32;
    b->bm = (uint32_t *) calloc(b->num_ints, sizeof(uint32_t));
    if (b->bm == NULL)
        err(1, "calloc error in bit_map_init().\n");

    return b;
}
/*Function sets bit at given index i to 1 */
void bit_map_set(struct bit_map *b, uint64_t i)
{
    while (i >= b->num_bits) {
        uint64_t new_bits = (b->num_bits)*2;
        uint32_t new_ints = (new_bits + 32 - 1) / 32;
        uint32_t *new_bm = (uint32_t *)calloc(new_ints, sizeof(uint32_t));
        if (new_bm== NULL)
            err(1, "calloc error in bit_map_set().\n");

        memcpy(new_bm, b->bm, (b->num_ints)*sizeof(uint32_t));

        free(b->bm);
        b->num_bits = new_bits;
        b->num_ints = new_ints;
        b->bm = new_bm;
    }


    b->bm[i/32] |= 1 << (31 - (i%32));
}
/*Function sets bit at given index i to 0 */
void bit_map_unset(struct bit_map *b, uint64_t i){
	while (i >= b->num_bits) {
        	uint64_t new_bits = (b->num_bits)*2;
       		uint32_t new_ints = (new_bits + 32 - 1) / 32;
        	uint32_t *new_bm = (uint32_t *)calloc(new_ints, sizeof(uint32_t));
        	if (new_bm== NULL)
            		err(1, "calloc error in bit_map_set().\n");

        memcpy(new_bm, b->bm, (b->num_ints)*sizeof(uint32_t));

        free(b->bm);
        b->num_bits = new_bits;
        b->num_ints = new_ints;
        b->bm = new_bm;
    }


    b->bm[i/32] &= ~(1 << (31 - (i%32)));

}

uint32_t bit_map_get(struct bit_map *b, uint64_t q)
{
    if (q > b->num_bits)
        return 0;

    return (( b->bm[q/32]) >> (31 - (q%32)) & 1);
}



//LL methods - rest in linkedlist.h
//
struct tlb* ll_init() {
        struct tlb* ll = (struct tlb*)malloc(sizeof(struct tlb*));
        ll->head =NULL; 
        ll->tail = NULL;
        return ll;
}
void SetPhysicalMem() {

    //Allocate physical memory using mmap or malloc; this is the total size of
    //your memory you are simulating

	//printf("PHYSICAL MEM SET"); 
    //HINT: Also calculate the number of physical and virtual pages and allocate
    //virtual and physical bitmaps and initialize them
    PHYSICAL_MEM_START = (intptr_t)malloc(sizeof(MEMSIZE));
    //printf("physical memory ptr %d",(intptr_t) PHYSICAL_MEM_START); 
   //printf("phys memory end %d",PHYSICAL_MEM_START+ MEMSIZE); 
    NUM_PHYSICAL_PAGES = MEMSIZE/PGSIZE;
    NUM_VIRTUAL_PAGES = MAX_MEMSIZE / PGSIZE;
    PHYSICAL_PAGE_BMP = bmp_init(NUM_PHYSICAL_PAGES);
    VIRTUAL_PAGE_BMP = bmp_init(NUM_VIRTUAL_PAGES);
    OFFSET_BITS = log2(PGSIZE);
    TOTAL_BITS = log2(MAX_MEMSIZE);
    PGDIR_BITS = (TOTAL_BITS-OFFSET_BITS)/2;
    PGTBL_BITS = TOTAL_BITS-PGDIR_BITS- OFFSET_BITS;
    PGTBL_SIZE =  pow(2,PGTBL_BITS); 
    PGDIR_ARRAY_SIZE = pow(2,PGDIR_BITS);
    PGDIR = malloc(PGDIR_ARRAY_SIZE*sizeof(void *));

}


unsigned get_bits(unsigned num, int num_bits, int start) {
	return (((1<<num_bits)-1) & (num >> (start -1)));
}

/*Function to initialize page table, returns empty page table (does not set in pgdir yet!) */

pte_t* pgtable_init(){
        int num_pgtable_entries = pow(2,PGTBL_BITS);
        pte_t* pgtable = malloc(num_pgtable_entries*sizeof(void*));
        for (int i = 0; i < num_pgtable_entries; i++){
                pgtable[i]= NULL;
        }
        return pgtable;
}


/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t * Translate(pde_t *pgdir, void *va) {
    //HINT: Get the Page directory index (1st level) Then get the
    //2nd-level-page table index using the virtual address.  Using the page
    //directory index and page table index get the physical address
  	//printf("in translate"); 
    if (va == NULL){
    	return NULL; 
    }
    unsigned dir_index = get_bits((intptr_t)va,PGDIR_BITS, 0);
        unsigned table_index = get_bits((intptr_t)va,PGTBL_BITS, PGDIR_BITS);
    unsigned offset = get_bits((intptr_t)va, OFFSET_BITS, PGDIR_BITS+PGTBL_BITS);
    if (dir_index < PGDIR_ARRAY_SIZE && table_index < PGTBL_SIZE) {
      return pgdir[dir_index][table_index] + offset;
    }
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
	if (va == NULL){
		return -1;
	}
	//printf("virtual ptr: %d\n", (intptr_t)va);
       //printf("bit%d ", get_bits((intptr_t)va,PGDIR_BITS, 0));
	 //printf("bit%d ", get_bits((intptr_t)va,PGTBL_BITS, PGDIR_BITS));	
       //printf("bit mask2%d ", get_mask(PGDIR_BITS,PGTBL_BITS-1)); 
 //	printf("dir bits%d\n", PGDIR_BITS); 	
	unsigned dir_index = get_bits((intptr_t)va,PGDIR_BITS, 0);
	unsigned table_index = get_bits((intptr_t)va,PGTBL_BITS, PGDIR_BITS); 
//	printf("INDEXES: %d%d\n", dir_index, table_index); 
	if (pgdir[dir_index] == NULL){ //initialize page table if it does not exist
		pgdir[dir_index] = pgtable_init();
	//	printf("table initialized");
	}	
	if (pgdir[dir_index][table_index] == NULL){
	//	printf("PA PTR%d", (intptr_t)pa); 
		pgdir[dir_index][table_index] = pa;
	//	printf("%d", pgdir[dir_index][table_index]); 
	}

	return 0;//mapping successful
}


/*Function that gets the next available page
*/
void *get_next_avail_virt(int num_pages) {
  //  printf("entering get next virt\n");
    //Use virtual address bitmap to find the next free page
    for (int i =1; i < NUM_VIRTUAL_PAGES; i++){ //start at i=1 in bitmap,reserve 0 for NULL 
    	if (bit_map_get(VIRTUAL_PAGE_BMP, i)==0){
		for (int j=1; j <num_pages;j++){
			if (bit_map_get(VIRTUAL_PAGE_BMP, i+j)==1){
				break; 
			}	
		}
		return i * PGSIZE;
	}
    }
    return NULL; 
}
void *get_next_avail_phys(int num_pages) {

    //Use virtual address bitmap to find the next free page
    //assumption that physical pages are contiguous since method is only returning address of first page
    //need array of page indices to mark pages being used outside of method (ie in myalloc)
    //printf("entering get next phys\n"); 
    for (int i =0; i < NUM_PHYSICAL_PAGES; i++){
        if (bit_map_get(PHYSICAL_PAGE_BMP, i)==0){
                for (int j=1; j <num_pages;j++){
                        if (bit_map_get(PHYSICAL_PAGE_BMP, i+j)==1){
                                break;
                        }
                }
                return i * PGSIZE+ PHYSICAL_MEM_START;
        }
    }
    return NULL;

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
	//initialize page directory
	//printf("ENTERING MYALLOC"); 

	if (PHYSICAL_MEM_START == NULL){
		SetPhysicalMem();
	}
	int num_pg_required = num_bytes/PGSIZE;	
	if (num_bytes % PGSIZE > 0){
		num_pg_required += 1;
	}
	//printf("NO PAGES: %d\n", num_pg_required); 
	void* phy_avail_pgaddr = get_next_avail_phys(num_pg_required);
	void* virt_avail_pgaddr = get_next_avail_virt(num_pg_required);
        if ((phy_avail_pgaddr == NULL) || (virt_avail_pgaddr==NULL)){
    		//printf("VIRT ADDR %d", (int)virt_avail_pgaddr); 
		//printf("PHYSICAL ADDR: %d", (int)phy_avail_pgaddr); 
    		return NULL;
        }

	//mark bitmaps
	 pthread_mutex_lock(&malloc_mutex);
	int phy_start = (intptr_t)(phy_avail_pgaddr-PHYSICAL_MEM_START)/PGSIZE;
	//printf("PHY START%d", phy_start); 
	for (int i = phy_start; i < (phy_start + num_pg_required); i++){
		//printf("Print BITMAP %d", i);
		bit_map_set(PHYSICAL_PAGE_BMP,i);
	}
	int virt_start = (intptr_t)virt_avail_pgaddr/PGSIZE;
	//printf("VIRT START%d", virt_start); 
	for (int j = virt_start; j < (virt_start + num_pg_required); j++){
		bit_map_set(VIRTUAL_PAGE_BMP,j);
	      // printf("set in bitmap virt%d", j); 	
	}
	 pthread_mutex_unlock(&malloc_mutex);
	//map page
	PageMap(PGDIR, virt_avail_pgaddr, phy_avail_pgaddr);// Check happens prior to this method, va/pa would never be null, hence mapping has to work at this point
     	return virt_avail_pgaddr;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
void myfree(void *va, int size) {

    //Free the page table entries starting from this virtual address (va)
    // Also mark the pages free in the bitmap
    //Only free if the memory from "va" to va+size is valid
    //
    int num_pages = size/PGSIZE;
    if (size % PGSIZE > 0) {
    	num_pages +=1;
    }
  // printf("NUM PAGES FREE%d", num_pages); 
    /* if ((va== NULL)||(va+size >MAX_MEMSIZE)){
    	return NULL;
    }
*/
    //assumes that user overwrites in page directory 
    //
    pthread_mutex_lock(&Free_mutex);
     //mark bitmaps (critical section)
     	int phy_start = (intptr_t)Translate(PGDIR, va)/PGSIZE;
//	printf("FREE:phy_start%d\n", phy_start); 
        for (int i = phy_start; i < (phy_start + num_pages); i++){
                bit_map_unset(PHYSICAL_PAGE_BMP,i);
//		printf("UNSET %d\n", i);
        }
        int virt_start = (intptr_t)va/PGSIZE;
//	    printf("FREE:virt_start%d\n", virt_start);
        for (int j = virt_start; j < (virt_start + num_pages); j++){
                bit_map_unset(VIRTUAL_PAGE_BMP,j);
  //      	 printf("UNSET VIRTUAL%d\n", j);
	}
	 pthread_mutex_unlock(&Free_mutex);
	 //ensure marking bitmaps is thread safe
}


/* The function copies data pointed by "val" to physical
 * memory pages using virtual address (va)
*/
void PutVal(void *va, void *val, int size) {

    /* HINT: Using the virtual address and Translate(), find the physical page. Copy
       the contents of "val" to a physical page. NOTE: The "size" value can be larger
       than one page. Therefore, you may have to find multiple pages using Translate()
       function.*/
	if (va == NULL){
		return NULL; 
	}
	pthread_mutex_lock(&PutVal_mutex);
	int num_pages = size/PGSIZE+1; 
	for (int i = 0; i < num_pages; i++){
		void* new_va = va+PGSIZE*i;
		 void* new_pa  = Translate(PGDIR, new_va);
//		 printf("TRANSLATE %d", (int)new_pa);

		 if (i==num_pages-1){
		 	memcpy(new_pa, val,size - (PGSIZE*i));
		 }
		 else{
		  memcpy(new_pa, val,PGSIZE);
		 } 
	} 
	pthread_mutex_unlock(&PutVal_mutex);
//mutex locking 
}


/*Given a virtual address, this function copies the contents of the page to val*/
void GetVal(void *va, void *val, int size) {

    /* HINT: put the values pointed to by "va" inside the physical memory at given
    "val" address. Assume you can access "val" directly by derefencing them.
    If you are implementing TLB,  always check first the presence of translation
    in TLB before proceeding forward */
	if (va != NULL){
                
        int num_pages = size/PGSIZE+1;
        for (int i = 0; i < num_pages; i++){
                void* new_va = va+PGSIZE*i;
                 void* new_pa  = Translate(PGDIR, new_va);
                 if (i==num_pages-1){
                        memcpy(val, new_pa,size - (PGSIZE*i));
                 }
		 else{
                  memcpy(val, new_pa,PGSIZE);
		 }
        }
	}

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
	 //multiply matrices
	

	int address_mat1=0; int address_mat2=0;
	int address_answer=0;
	int y =0; int z =0; int i =0; int j=0; 
	for (i = 0; i < size; i++) {
		int sum = 0;
     	   for (j = 0; j < size; j++) {
        	address_mat1 = (unsigned int)mat1 + ((i * size * sizeof(int))) + (j * sizeof(int));
            	address_mat2 = (unsigned int)mat2 + ((i * size * sizeof(int))) + (j * sizeof(int));
            	GetVal((void *)address_mat1, &y, sizeof(int));
            	GetVal( (void *)address_mat2, &z, sizeof(int));
            	sum += y * z;
        }
	address_answer = (unsigned int)answer + ((i * size * sizeof(int))) + (j * sizeof(int));
        PutVal((void *)address_answer, &sum, sizeof(int));
    }


}
