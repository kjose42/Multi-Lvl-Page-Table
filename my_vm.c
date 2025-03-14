#include "my_vm.h"


void *physical_memory = NULL;

// number of pages divided by 8
int num_physical_page_bytes;
int num_virtual_page_bytes;

char *physical_bitmap = NULL;
char *virtual_bitmap = NULL;

int virtual_offset_bits = 1;
int first_level_bits, second_level_bits;

struct tlb TLB;
double lookups = 0;
double misses = 0;

long *arr = NULL;

int lock = 0;

/*
Function responsible for allocating and setting physical memory 
*/
void set_physical_mem() {

    //Allocate physical memory using mmap or malloc; this is the total size of
    //memory that is being simulated

    
    //Calculates the number of physical and virtual pages and allocates
    //virtual and physical bitmaps and initializes them

    physical_memory = malloc(MEMSIZE);

    // divide by 8 because each char (byte) is 8 bits
    num_physical_page_bytes = MEMSIZE / PGSIZE / 8;
    num_virtual_page_bytes = MAX_MEMSIZE / PGSIZE / 8;
    physical_bitmap = (char *) malloc(num_physical_page_bytes);
    virtual_bitmap = (char *) malloc(num_virtual_page_bytes);
    memset(physical_bitmap, 0, num_physical_page_bytes);
    memset(virtual_bitmap, 0, num_virtual_page_bytes);

    // calculates the log of page size (number of virtual offset bits)
    while((PGSIZE >> virtual_offset_bits) != 1) virtual_offset_bits++;
    int leftover_bits = 32 - virtual_offset_bits;
    // uneven leftover bits == uneven level bits
    if(leftover_bits % 2 == 1) {
        second_level_bits = (leftover_bits + 1) / 2;
        first_level_bits = second_level_bits - 1;
    }
    else {//even
        second_level_bits = leftover_bits / 2;
        first_level_bits = second_level_bits;
    }
    // create page directory
    unsigned long num_pde = 1 << first_level_bits;
    pde_t *pgdir_array = (pde_t *) malloc(num_pde * sizeof(pde_t));
    // add it to physical memory
    memcpy(physical_memory, (void *) pgdir_array, num_pde * sizeof(pde_t));
    // update physical bitmap
    for(unsigned long i = 0; i < (num_pde * sizeof(pde_t)); i += PGSIZE) {
        set_bit_at_index(physical_bitmap, (int) i / PGSIZE);
    }

    // make sure no virtual address can be 0
    set_bit_at_index(virtual_bitmap, 0);

    //initialize TLB entries to be empty
    for(int inc = 0; inc < TLB_ENTRIES; inc++) TLB.entries[inc].valid = 0;

    // malloc array to track sizes
    arr = malloc(sizeof(long) * (MEMSIZE / PGSIZE));

    // cleanup function on exit
    atexit(cleanup);

}


/*
 * Adds a virtual to physical page translation to the TLB.
 */
int
add_TLB(void *va, void *pa, int remove)
{

    unsigned int page_directory_index = (unsigned int) va >> (32 - first_level_bits);
	unsigned int mask = (1 << (32 - first_level_bits - virtual_offset_bits)) - 1;
	unsigned int page_table_index = (unsigned int) va >> virtual_offset_bits & mask;
	unsigned long num_pte = 1 << second_level_bits;
	unsigned int tag = (page_directory_index*num_pte) + page_table_index; 
	unsigned int set = tag % TLB_ENTRIES;
    if(remove) {//remove entry
        TLB.entries[set].valid = 0;
	    TLB.entries[set].virtual_address = NULL;
	    TLB.entries[set].physical_address = NULL;
    }
    else {//add entry
	    TLB.entries[set].valid = 1;
	    TLB.entries[set].virtual_address = va;
	    TLB.entries[set].physical_address = pa;
    }

    return -1;
}


/*
 * Checks TLB for a valid translation.
 * Returns the physical page address.
 */
pte_t *
check_TLB(void *va) {

    unsigned int page_directory_index = (unsigned int) va >> (32 - first_level_bits);
	unsigned int mask = (1 << (32 - first_level_bits - virtual_offset_bits)) - 1;
	unsigned int page_table_index = (unsigned int) va >> virtual_offset_bits & mask;
	unsigned long num_pte = 1 << second_level_bits;
	unsigned int tag = (page_directory_index*num_pte) + page_table_index; 
	unsigned int set = tag % TLB_ENTRIES;
	if(TLB.entries[set].valid == 1) {
		void *TLB_virtual_address = TLB.entries[set].virtual_address;
		unsigned int va_vpn = (unsigned int) va >> virtual_offset_bits;
		unsigned int TLB_vpn = (unsigned int) TLB_virtual_address >> virtual_offset_bits;
		if(va_vpn == TLB_vpn) return TLB.entries[set].physical_address;
	}
	return 0;

}


/*
 * Calculate and prints TLB miss rate.
 */
void
print_TLB_missrate()
{
    double miss_rate = 0;	
    miss_rate = misses/lookups;
    fprintf(stderr, "TLB miss rate %lf \n", miss_rate);
}


/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t *translate(pde_t *pgdir, void *va) {
    /*Gets the Page directory index (1st level) Then gets the
    * 2nd-level-page table index using the virtual address.  Using the page
    * directory index and page table index gets the physical address.
    *
    * Part 2 HINT: Check the TLB before performing the translation. If
    * translation exists, then you can return physical address from the TLB.
    */

    // Part 2 TLB Check
    lookups = lookups + 1;
	pte_t *pa = check_TLB(va);
	if(pa != 0){//if pa is in TLB already
		return pa;}
	misses = misses + 1;//not in TLB

    unsigned long page_directory_index = (unsigned long) va >> (32 - first_level_bits);
    unsigned long mask = (1 << (32 - first_level_bits - virtual_offset_bits)) - 1;
    unsigned long page_table_index = (unsigned long) va >> virtual_offset_bits & mask;

    pde_t *pde = pgdir + (page_directory_index * sizeof(pde_t));

    if(pde != NULL) {//check if dir entry is empty
        pte_t *pte = (pte_t *) (*pde + (page_table_index * sizeof(pte_t)));
        pte_t *pa = (pte_t *) *pte;

        if(pa != NULL) {//check if address is empty
            // add translation to the TLB
            add_TLB(va, pa, 0);
            return pa;
        }
    }

    //If translation not successful, then return NULL
    return NULL; 
}


/*
 *The function takes a page directory address, virtual address, physical address
 *as an argument, and sets a page table entry. This function will walk the page
 *directory to see if there is an existing mapping for a virtual address. If the
 *virtual address is not present, then a new entry will be added
*/
int
page_map(pde_t *pgdir, void *va, void *pa)
{

    /*Similar to translate(), finds the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, sets the
    virtual to physical mapping */

    // Part 2 TLB Check
    pte_t *tlb_return = check_TLB(va);
    if(tlb_return != 0) return -1;

    unsigned long page_directory_index = (unsigned long) va >> (32 - first_level_bits);
    unsigned long mask = (1 << (32 - first_level_bits - virtual_offset_bits)) - 1;
    unsigned long page_table_index = (unsigned long) va >> virtual_offset_bits & mask;

    pde_t *pde = pgdir + (page_directory_index * sizeof(pde_t));

    // create page table for this directory entry if empty
    if(*pde == 0) {
        unsigned long num_pages;
        unsigned long num_pte = 1 << second_level_bits;
        unsigned long pgtbl_size = num_pte * sizeof(pte_t);
	//round up if new page size doesnt divide evenly with PGSIZE
        if(pgtbl_size % PGSIZE != 0) num_pages = ((pgtbl_size - (pgtbl_size % PGSIZE)) / PGSIZE) + 1;
        else num_pages = pgtbl_size / PGSIZE;//if divides evenly
        // look for contiguous space in physical memory for page table
        unsigned long index = search_bitmap_for_pages(physical_bitmap, num_pages, num_physical_page_bytes);
        // malloc the page table
        pte_t *pgtbl_array = (pte_t *) malloc(pgtbl_size);
        // move page table to index in physical memory and map to this page directory entry
        *pde = (pde_t) memcpy((physical_memory + (index * PGSIZE)), pgtbl_array, pgtbl_size);
        // update physical bitmap
        for(unsigned long i = 0; i < num_pages; i++) {
            set_bit_at_index(physical_bitmap, index + i);
        }
    }

    pte_t *pte = (pte_t *) (*pde + (page_table_index * sizeof(pte_t)));
    //empty table entry or no physical address
    if(*pte == 0) {
        *pte = (unsigned long) pa;


        pde_t *pde2 = physical_memory + (page_directory_index * sizeof(pde_t));
        pte_t *pte2 = (pte_t *) (*pde + (page_table_index * sizeof(pte_t)));

        return 0;
    }
    
    return -1;
}

void *get_next_avail(int num_pages) {
 
    //Use virtual address bitmap to find the next free page

    // search virtual bitmap

    unsigned long index = search_bitmap_for_pages(virtual_bitmap, num_pages, num_virtual_page_bytes);

    if(index != 0) {
        unsigned long num_pte = 1 << second_level_bits;
        unsigned long second_level_va = index % num_pte;
        unsigned long first_level_va = (index - second_level_va) / num_pte;

        pde_t va = (first_level_va << (second_level_bits + virtual_offset_bits) ) + (second_level_va << virtual_offset_bits);

        // update virtual bitmap
        for(unsigned long i = 0; i < num_pages; i++) {
            set_bit_at_index(virtual_bitmap, index + i);
        }

        return (void *) va;
    }

    return NULL;
}

void *t_malloc(unsigned int num_bytes) {

    /* 
    * If the physical memory is not yet initialized, then allocates and initializes.
    */

    /* 
    * If the page directory is not initialized, then initializes the
    * page directory. Next, using get_next_avail(), checks if there are free pages. If
    * free pages are available, sets the bitmaps and map a new page.
    * Marks which physical pages are used. 
    */

    while(__atomic_test_and_set(&lock, 1) == 1) {}

    // initialize memory and page directory on first call
    if(physical_memory == NULL) set_physical_mem();

    // gather the number of pages we will need
    unsigned int num_pages;
    //if num_bytes isnt divided evenly by PGSIZE
    if((num_bytes) % PGSIZE != 0) num_pages = ((num_bytes - (num_bytes % PGSIZE)) / PGSIZE) + 1;
    else num_pages = num_bytes / PGSIZE;//if divides evenly

    void *va = get_next_avail(num_pages);

    // map virtual to physical pages
    int error = 0;
    for(unsigned int i = 0; i < num_pages; i++) {
        unsigned long index = search_bitmap_for_pages(physical_bitmap, 1, num_physical_page_bytes);
        if(!index) {//no space to allocate pages in bitmap
            error = 1;
            break;
        }
        set_bit_at_index(physical_bitmap, index);
        page_map(physical_memory, va + (i * 1 << virtual_offset_bits), physical_memory + (index * PGSIZE));
    }

    lock = 0;

    // no page available
    if(error) return NULL;
    // return va
    return va;
}

/* 
 *Responsible for releasing one or more memory pages using virtual address (va)
*/
void t_free(void *va, int size) {

    /* Frees the page table entries starting from this virtual address
     * (va). Also marks the pages free in the bitmap. Perform free only if the 
     * memory from "va" to va+size is valid.
     *
     * Removes the translation from the TLB
     */


    unsigned long num_pages;
    //if size doesnt get divided by PGSIZE evenly, round up
    if(size % PGSIZE != 0) num_pages = ((size - (size % PGSIZE)) / PGSIZE) + 1;
    else num_pages = size / PGSIZE;//divides evenly
    while(__atomic_test_and_set(&lock, 1) == 1) {}
    for(int i = 0; i < num_pages; i++) {
        // if any page is invalid then return
        unsigned long index = ((pde_t) va >> virtual_offset_bits) + i;
        if(get_bit_at_index(virtual_bitmap, index) == 0) {
            lock = 0;
            return;
        }
    }

    for(int i = 0; i < num_pages; i++) {
        // update virtual bitmap
        unsigned long index = ((pde_t) va >> virtual_offset_bits) + i;

        clear_bit_at_index(virtual_bitmap, index);

        // update physical bitmap
        pte_t* pa = translate((pde_t *) physical_memory, va + (PGSIZE * i));
        index = ((pte_t) pa - (pte_t) physical_memory) / PGSIZE;
        clear_bit_at_index(physical_bitmap, index);

        // clean up page table entry
        page_unmap(physical_memory, va + (PGSIZE * i));

        // remove from TLB
        add_TLB(va + (PGSIZE * i), pa, 1);
    }
    lock = 0;
}


/* The function copies data pointed by "val" to physical
 * memory pages using virtual address (va)
 * The function returns 0 if the put is successful and -1 otherwise.
*/
void put_value(void *va, void *val, int size) {

    /* Using the virtual address and translate(), find the physical page. Copies
     * the contents of "val" to a physical page. The "size" value can be larger 
     * than one page. Therefore, may have to find multiple pages using translate()
     * function.
     */

    unsigned long num_pages;
	//ceil(size_change/PGSIZE)
	if(size % PGSIZE != 0){
		num_pages = ((size - (size % PGSIZE)) / PGSIZE) + 1;
	}
	else{num_pages = size / PGSIZE;}//unless it divides evenly
	for(int inc = 0; inc < num_pages; inc++){
		while(__atomic_test_and_set(&lock, 1) == 1) {}
		pte_t* pa = translate((pde_t *)physical_memory, va + (PGSIZE*inc));
		unsigned int page_directory_index = (unsigned int) va >> (32 - first_level_bits);
		unsigned int mask = (1 << (32 - first_level_bits - virtual_offset_bits)) - 1;
		unsigned int page_table_index = (unsigned int) va >> virtual_offset_bits & mask;
		unsigned long num_pte = 1 << second_level_bits;
		unsigned int arr_index = (page_directory_index*num_pte) + page_table_index;
		if(arr[arr_index] + size < PGSIZE){
			arr[arr_index] += size;}//end of adding val
		else{arr[arr_index] = PGSIZE;}//need another page to fit val
		unsigned long mask_offset = (1 << virtual_offset_bits) - 1;
    		unsigned long page_offset = (unsigned long) va & mask_offset;
		char *pa_change = (char *) pa + page_offset;
		pa = (pte_t *) pa_change;
		//Because size_type_change was added:
		//4 bytes added to table_entry
		//4 bytes subtracted from PGSIZE
		if(size < PGSIZE){//end of adding val
			memcpy((void *)pa, val+(PGSIZE*inc), size);}
		else{memcpy((void *)pa, val+(PGSIZE*inc), PGSIZE);
			size = size - PGSIZE;}//need another page to fit val
		lock = 0;
	}

}


/*Given a virtual address, this function copies the contents of the page to val*/
void get_value(void *va, void *val, int size) {

    /* Puts the values pointed to by "va" inside the physical memory at given
    * "val" address
    */

    unsigned long num_pages;
	//ceil(size_change/PGSIZE)
	if(size % PGSIZE != 0){
		num_pages = ((size - (size % PGSIZE)) / PGSIZE) + 1;
	}
	else{num_pages = size / PGSIZE;}
	//checking size start
	int size_copy = size;
    while(__atomic_test_and_set(&lock, 1) == 1) {}
	for(int inc = 0; inc < num_pages; inc++){
		pte_t* pa = translate((pde_t *)physical_memory, va + (PGSIZE*inc));
		unsigned int page_directory_index = (unsigned int) va >> (32 - first_level_bits);
		unsigned int mask = (1 << (32 - first_level_bits - virtual_offset_bits)) - 1;
		unsigned int page_table_index = (unsigned int) va >> virtual_offset_bits & mask;
		unsigned long num_pte = 1 << second_level_bits;
		unsigned int arr_index = (page_directory_index*num_pte) + page_table_index;
		int bytes_in_page = arr[arr_index];
		if(size_copy <= PGSIZE && size_copy > bytes_in_page){
            		lock = 0;
			return;}//size is bigger than page's data size
		if(size_copy >= PGSIZE){
			size_copy = size_copy - PGSIZE;}//need to find other val pg
	}
	//checking size end
	for(int inc = 0; inc < num_pages; inc++){
		pte_t* pa = translate((pde_t *)physical_memory, va + (PGSIZE*inc));
		unsigned long mask = (1 << virtual_offset_bits) - 1;
    		unsigned long page_offset = (unsigned long) va & mask;
		char *pa_change = (char *) pa + page_offset;
		pa = (pte_t *) pa_change;
		//Because size_type_change was added:
		//4 bytes added to table_entry
		//4 bytes subtracted from PGSIZE
		if(size < PGSIZE ){
			memcpy(val+(PGSIZE*inc), (void *)pa, size);}
		else{memcpy(val+(PGSIZE*inc), (void *)pa, PGSIZE);
			size = size - PGSIZE;}
	}
    lock = 0;

}


/*
This function receives two matrices mat1 and mat2 as an argument with size
argument representing the number of rows and columns. After performing matrix
multiplication, copies the result to answer.
*/
void mat_mult(void *mat1, void *mat2, int size, void *answer) {

    int x, y, val_size = sizeof(int);
    int i, j, k;
    for (i = 0; i < size; i++) {
        for(j = 0; j < size; j++) {
            unsigned int a, b, c = 0;
            for (k = 0; k < size; k++) {
                int address_a = (unsigned int)mat1 + ((i * size * sizeof(int))) + (k * sizeof(int));
                int address_b = (unsigned int)mat2 + ((k * size * sizeof(int))) + (j * sizeof(int));
                get_value( (void *)address_a, &a, sizeof(int));
                get_value( (void *)address_b, &b, sizeof(int));
                // printf("Values at the index: %d, %d, %d, %d, %d\n", 
                //     a, b, size, (i * size + k), (k * size + j));
                c += (a * b);
            }
            int address_c = (unsigned int)answer + ((i * size * sizeof(int))) + (j * sizeof(int));
            //printf("This is the c: %d, address: %x!\n", c, address_c);
            put_value((void *)address_c, (void *)&c, sizeof(int));
        }
    }
}


unsigned long search_bitmap_for_pages(char *bitmap, int num_pages, int bitmap_length) {
    unsigned long mask = 11111111;
    int free_pages = 0;
    unsigned long index;

    for(unsigned long i = 0; i < bitmap_length; i++) {
        // check if there is atleast 1 free page within this set of 8 (or within the byte)
        if(bitmap[i] ^ mask > 0) {
            for(int bit = 0; bit < 8; bit++) {
                // we have located a free page
                if(get_bit_at_index(bitmap, (i * 8 + bit)) == 0) {
                    // first page, save location?
                    if(!free_pages) index = (i * 8) + bit;
                    free_pages++;
                } 
                else free_pages = 0;
                // found the set of contiguous pages
                if(free_pages == num_pages) {
                    return index;
                }
            }
        }
        else free_pages = 0;
    }

    // an index can never be 0, pgdir takes up >= 1 page in physical
    // and we manually take up first virtual page
    return 0;
}


/* 
 * Function to get a bit at "index"
 */
int get_bit_at_index(char *bitmap, int index) {
    //Get to the location in the character bitmap array
    
    int index_of_bit = index % 8;
    int index_of_char = (index - index_of_bit) / 8;

    return ((bitmap[index_of_char] >> (8-index_of_bit-1)) % 2) != 0;
}


/* 
 * Function to set a bit at "index" bitmap
 */
void set_bit_at_index(char *bitmap, int index)
{	

    int index_of_bit = index % 8;
    int index_of_char = (index - index_of_bit) / 8;

    bitmap[index_of_char] = bitmap[index_of_char] | (1LU << (8-index_of_bit-1));

}


/* 
 * Function to clear a bit at "index" bitmap
 */
void clear_bit_at_index(char *bitmap, int index)
{	

    int index_of_bit = index % 8;
    int index_of_char = (index - index_of_bit) / 8;

    bitmap[index_of_char] = bitmap[index_of_char] & ~(1LU << (8-index_of_bit-1));

}


void cleanup() {
    free(physical_memory);
    free(physical_bitmap);
    free(virtual_bitmap);
    free(arr);
}


int
page_unmap(pde_t *pgdir, void *va)
{
    unsigned long page_directory_index = (unsigned long) va >> (32 - first_level_bits);
    unsigned long mask = (1 << (32 - first_level_bits - virtual_offset_bits)) - 1;
    unsigned long page_table_index = (unsigned long) va >> virtual_offset_bits & mask;

    pde_t *pde = pgdir + (page_directory_index * sizeof(pde_t));

    pte_t *pte = (pte_t *) (*pde + (page_table_index * sizeof(pte_t)));
    if(*pte == 0) {
        *pte = (unsigned long) 0;

        return 0;
    }
}

