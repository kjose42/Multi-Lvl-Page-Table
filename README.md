# Multi-Lvl-Page-Table
#This project simulates memory management. It simulates a memory multi-level page table that can translate virtual addresses to physical addresses. A Translation Lookaside Buffer is also implemented to make the #translation more efficient.




.h code
Created variables:
	pde_t = represents a Page Directory Entry (1st level of page directory)
	pte_t = represents a Page Table Entry (2nd level of page directory)




.c code
void set_physical_mem()
	Initializes and mallocs the memory buffer
	Initializes 2 bitmaps (the bitmap is represented by an array) to keep track of which pages are empty
		Each bit represents a page
		1 bitmap for physical memory
		1 bitmap for virtual memory
	Initializes a 2 level page directory
		Calculates the size of each page directory level by using PGSIZE
		Makes sure that no virtual address can be 0
			(Because the virtual address can be interpreted as NULL)
	Initializes TLB
	Initializes an array (arr) that keeps track of how many bytes of data are stored in each page 



int add_TLB(void* va, void* pa, int remove)
	Adds or removes address translation to TLB
	Input:
		va = virtual address
		pa = physical address
		remove = Boolean indicating whether or not this function is used to remove or add a translation 
			(0 for add, 1 for remove)
	Output: 
		Boolean indicating whether the function successfully executed or not



pte_t* check_TLB(void *va)
	Checks if the given virtual address' translation is in TLB
	Input:
		va = virtual address
	Output:
		va's physical address
	Calculates the tag and set from va
	If TLB contains the set, return va's translation



void print_TLB_missrate()
	Calculates TLB's rate of misses
	Prints rate of misses



pte_t* translate(pde_t* pgdir, void* va)
	Finds the physical address of the given virtual address
	Input:
		pgdir = address of page directory
		va = virtual address to be translated
	Output: 
		physical address of va
	First, check if the translation is already stored in the TLB
	If not:
	Using va, this function finds the physical address' indices in the page directory's first and second level.
	Adds translation to the TLB



int page_map(pde_t* pgdir, void* va, void* pa)
	Checks if the given virtual address's physical address is stored in the page directory
	Input: 
		pgdir = address of page directory
		va = virtual address
		pa = physical address of virtual address
	Output: 
		Boolean that indicates whether the page mapping was successful or not
	First, check if the translation is already stored in the TLB
	If not:
	Similar to translate(), finds the indices in the page directory. 
	If page table does not exist for a 1st level directory index:
		Using search_bitmap_for_pages(), looks for contiguous space for page table 
		Create page table
		Update physical bitmap for newly created page table
	If the translation does not exist, add translation



void* get_next_avail(int num_pages)
	Returns virtual address for available pages
	Input: 
		number of pages needed
	Output: 
		virtual address
	Using search_bitmap_for_pages(), looks for index in virtual memory to fit pages
	Create virtual address based on returned index
	Update virtual bitmap for occupied pages
	


void* t_malloc(unsigned int num_bytes)
	Initializes physical memory, page directory, and pages
	Input: 
		number of bytes to malloc/be allocated
	Output: 
		virtual address
	If physical memory is not yet initialized, use set_physical_mem()
	Calculates how many pages required for input
	Using get_next_avail(), find space for pages
	Using search_bitmap_for_pages(), looks for index in physical memory to fit pages
	Update physical bitmap for occupied pages
	Using page_map(), add translation in page directory



void t_free (void *va, int size)
	Frees pages starting from given virtual address
	Input:
		va = virtual address of data that needs to be freed
		size = size of data
	Number of pages that need to be freed is calculated from size
	Stops loop when page is empty or invalid
	Using clear_bit_at_index, clears bits that correspond to freed virtual pages
	Using translate, find the physical addresses of freed pages
	Clear bits that correspond to freed physical pages
	Using page_unmap, free pages starting from va
	Remove any freed virtual address' translation from TLB



void put_value(void *va, void *val, int size)
	Copies given data to physical memory pages
	Input:
		va = virtual address
		val = pointer of data to be inserted in virtual address
		size = size of data
	Finds physical address of virtual address (and additional virtual pages if needed) by using translate()
	For each page, update arr's pages with size of data
	Copies val's data to physical memory pages



void get_value(void* va, void* val, int size)
	Copies contents of pages to val
	Input:
		va = virtual address
		val = pointer of data to be inserted in virtual address
		size = size of data
	Finds physical address of virtual address (and additional virtual pages if needed) by using translate()
	For each page, compare arr's page's data size and size to check if size is bigger than data stored
		Return if size is bigger than data stored
	For each page, use translate() and copy contents of pages to val



void mat_mult(void* mat1, void* mat2, int size, void* answer)
	Multiplies matrices and stores result in answer
	Input:
		mat1 = 1st matrix
		mat2 = 2nd matrix
		size = size of matrix's rows and columns
		answer = pointer to store answer



unsigned long search_bitmap_for_pages(char* bitmap, int num_pages, int bitmap_length)
	Looks for bitmap index that points to the start of the desired free pages. Free pages must be contiguous
	Input:
		bitmap
		num_pages = number of empty pages to retrieve
		bitmap_length
	Output:
		Bitmap index that points to the start of the desired free pages
	Initialize counter of free pages (free_pages)
	For each loop iteration:
		If this bit represents the first free page (when free_page == 0), keep track of this page index (stored in index variable)
		If bit is empty, add 1 to free_pages
		If bit is not empty and more free pages need to be found, restart free page counter
		Return when found a num_pages amount of free pages



int get_bit_at_index(char* bitmap, int index)



void set_bit_at_index(char* bitmap, int index)
	Changes bitmap's bit at index



void clear_bit_at_index(char* bitmap, int index)
	Changes bitmap's bit to 0



void cleanup()
	Frees physical memory, bitmaps, and arr



int page_unmap (pde_t* pgdir, void *va)
	Unmaps page of the given virtual address from directory
	Input:
		pgdir = address of page directory
		va = virtual address
	Output: Boolean indicating whether the function successfully executed or not
