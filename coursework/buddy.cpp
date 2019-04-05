/*
 * Buddy Page Allocation Algorithm
 * SKELETON IMPLEMENTATION -- TO BE FILLED IN FOR TASK (2)
 */

/*
 * STUDENT NUMBER: s1768094
 */
#include <infos/mm/page-allocator.h>
#include <infos/mm/mm.h>
#include <infos/kernel/kernel.h>
#include <infos/kernel/log.h>
#include <infos/util/math.h>
#include <infos/util/printf.h>

using namespace infos::kernel;
using namespace infos::mm;
using namespace infos::util;

#define MAX_ORDER	15

/**
 * A buddy page allocation algorithm.
 */
class BuddyPageAllocator : public PageAllocatorAlgorithm
{
private:
	/**
	 * Returns the number of pages that comprise a 'block', in a given order.
	 * @param order The order to base the calculation off of.
	 * @return Returns the number of pages in a block, in the order.
	 */
	static inline constexpr uint64_t pages_per_block(int order)
	{
		/* The number of pages per block in a given order is simply 1, shifted left by the order number.
		 * For example, in order-2, there are (1 << 2) == 4 pages in each block.
		 */
		return (1 << order);
	}
	
	/**
	 * Returns TRUE if the supplied page descriptor is correctly aligned for the 
	 * given order.  Returns FALSE otherwise.
	 * @param pgd The page descriptor to test alignment for.
	 * @param order The order to use for calculations.
	 */
	static inline bool is_correct_alignment_for_order(const PageDescriptor *pgd, int order)
	{
		// Calculate the page-frame-number for the page descriptor, and return TRUE if
		// it divides evenly into the number pages in a block of the given order.
		return (sys.mm().pgalloc().pgd_to_pfn(pgd) % pages_per_block(order)) == 0;
	}
	
	/** Given a page descriptor, and an order, returns the buddy PGD.  The buddy could either be
	 * to the left or the right of PGD, in the given order.
	 * @param pgd The page descriptor to find the buddy for.
	 * @param order The order in which the page descriptor lives.
	 * @return Returns the buddy of the given page descriptor, in the given order.
	 */
	PageDescriptor *buddy_of(PageDescriptor *pgd, int order)
	{
		// (1) Make sure 'order' is within range
		if (order >= MAX_ORDER) {
			return NULL;
		}

		// (2) Check to make sure that PGD is correctly aligned in the order
		if (!is_correct_alignment_for_order(pgd, order)) {
			return NULL;
		}
				
		// (3) Calculate the page-frame-number of the buddy of this page.
		// * If the PFN is aligned to the next order, then the buddy is the next block in THIS order.
		// * If it's not aligned, then the buddy must be the previous block in THIS order.
		uint64_t buddy_pfn = is_correct_alignment_for_order(pgd, order + 1) ?
			sys.mm().pgalloc().pgd_to_pfn(pgd) + pages_per_block(order) : 
			sys.mm().pgalloc().pgd_to_pfn(pgd) - pages_per_block(order);
		
		// (4) Return the page descriptor associated with the buddy page-frame-number.
		return sys.mm().pgalloc().pfn_to_pgd(buddy_pfn);
	}
	
	/**
	 * Inserts a block into the free list of the given order.  The block is inserted in ascending order.
	 * @param pgd The page descriptor of the block to insert.
	 * @param order The order in which to insert the block.
	 * @return Returns the block_pointer (i.e. a pointer to the pointer that points to the block) that the block
	 * was inserted into.
	 */
	PageDescriptor **insert_block(PageDescriptor *pgd, int order)
	{
		// Make sure the order is in range
		assert(order_in_range(order));

		// Starting from the _free_area array, find the block_pointer in which the page descriptor
		// should be inserted.
		PageDescriptor **block_pointer = &_free_areas[order];
		
		// Iterate whilst there is a block_pointer, and whilst the page descriptor pointer is numerically
		// greater than what the block_pointer is pointing to.
		while (*block_pointer && pgd > *block_pointer) {
			block_pointer = &(*block_pointer)->next_free;
		}
		
		// Insert the page descriptor into the linked list.
		pgd->next_free = *block_pointer;
		*block_pointer = pgd;
		
		// Return the insert point (i.e. block_pointer)
		return block_pointer;
	}
	
	/**
	 * Removes a block from the free list of the given order.  The block MUST be present in the free-list, otherwise
	 * the system will panic.
	 * @param pgd The page descriptor of the block to remove.
	 * @param order The order in which to remove the block from.
	 */
	void remove_block(PageDescriptor *pgd, int order)
	{
		// Make sure the order is in range
		assert(order_in_range(order));

		// Starting from the _free_area array, iterate until the block has been located in the linked-list.
		PageDescriptor **slot = &_free_areas[order];
		while (*slot && pgd != *slot) {
			slot = &(*slot)->next_free;
		}

		// Make sure the block actually exists.  Panic the system if it does not.
		assert(*slot == pgd);
		
		// Remove the block from the free list.
		*slot = pgd->next_free;
		pgd->next_free = NULL;
	}
	
	/**
	 * Given a pointer to a block of free memory in the order "source_order", this function will
	 * split the block in half, and insert it into the order below.
	 * @param block_pointer A pointer to a pointer containing the beginning of a block of free memory.
	 * @param source_order The order in which the block of free memory exists.  Naturally,
	 * the split will insert the two new blocks into the order below.
	 * @return Returns the left-hand-side of the new block.
	 */
	PageDescriptor *split_block(PageDescriptor **block_pointer, int source_order)
	{
		// Make sure there is an incoming pointer.
		assert(*block_pointer);
		
		// Make sure the block_pointer is correctly aligned.
		assert(is_correct_alignment_for_order(*block_pointer, source_order));
		
		// Make sure the order is valid
		assert(order_in_range(source_order));

		// Find the given block, and remove it from the list of given order
		PageDescriptor *block = *block_pointer;
		remove_block(block, source_order);

        // Insert the two splitted blocks into the list of one order below
		int aim_order = source_order - 1;
		PageDescriptor *buddy = buddy_of(block, aim_order);
		insert_block(buddy, aim_order);
		insert_block(block, aim_order);

        // Make sure the block is on the left hand 
		assert(block + pages_per_block(aim_order) == buddy);

        // Make sure the splitted blocks are indeed free
		assert(is_free(block, aim_order));
		assert(is_free(buddy, aim_order));
		
		return block;
	}
	
	/**
	 * Takes a block in the given source order, and merges it (and it's buddy) into the next order.
	 * This function assumes both the source block and the buddy block are in the free list for the
	 * source order.  If they aren't this function will panic the system.
	 * @param block_pointer A pointer to a pointer containing a block in the pair to merge.
	 * @param source_order The order in which the pair of blocks live.
	 * @return Returns the new block_pointer that points to the merged block.
	 */
	PageDescriptor **merge_block(PageDescriptor **block_pointer, int source_order)
	{
		assert(*block_pointer);
		
		// Make sure the area_pointer is correctly aligned.
		assert(is_correct_alignment_for_order(*block_pointer, source_order));

        // Make sure the order is in range
		assert(order_in_range(source_order));

		PageDescriptor *block = *block_pointer;
		PageDescriptor *buddy = buddy_of(block, source_order);

		// Remove the given block and its buddy from the free list of given order
        remove_block(buddy, source_order);
		remove_block(block, source_order);

        // Make sure the inserted block is correctly aligned
		int aim_order = source_order + 1;
		PageDescriptor *merged_block = is_correct_alignment_for_order(block, aim_order) ? block : buddy;

        // Insert the merged block into the list of one order higher 
		return insert_block(merged_block, aim_order);		
	}

	/**
	 * Decided whether a given order is valid
	 * @param order The order to be decided.
	 * @return Returns true if the order is greater or equal to 0 and less than 17 otherwise false
	 */
	bool order_in_range(int order) {
		return order >=0 && order < MAX_ORDER;
	}
	
public:
	/**
	 * Constructs a new instance of the Buddy Page Allocator.
	 */
	BuddyPageAllocator() {
		// Iterate over each free area, and clear it.
		for (unsigned int i = 0; i < ARRAY_SIZE(_free_areas); i++) {
			_free_areas[i] = NULL;
		}
	}

    /**
	 * Check if the block is in the free list of given order
	 * @param pgd A pointer to an array of page descriptors which represent the block to be checked.
	 * @param order The power of a number of contiguous pages.
	 * @return Returns true if the block is found in the free list of given order
	 */
    bool is_free(PageDescriptor *pgd, int order) 
	{
		// Make sure that the incoming page descriptor is correctly aligned
		assert(is_correct_alignment_for_order(pgd, order));

		// Make sure the order is in range
		assert(order_in_range(order));

        // Iterate whilst there is a block_pointer, and whilst the page descriptor pointer need to be found is
		// not equal to what the block_pointer is pointing to i.e. the block isn't found in the list.
		PageDescriptor **block_pointer = &_free_areas[order];
		while (*block_pointer && pgd != *block_pointer) {
			block_pointer = &(*block_pointer)->next_free;
		}

		return pgd == *block_pointer;
	}

	/**
	 * Find the block of given order that contains a page (block of order 0) from the free list.
	 * Helper function for reserve_page()
	 * @param pgd A pointer to a page descriptor needed to be found.
	 * @param order The power of the block.
	 * @return Returns the block that contains the page. If the block containing the page isn't found, return null
	 */
	PageDescriptor *get_block(PageDescriptor *pgd, int order) 
	{
		// Make sure the order is in range
		assert(order_in_range(order));
		
		// Calculate the block in given order that containing the page
		PageDescriptor *aim_block = 
			sys.mm().pgalloc().pfn_to_pgd((
				sys.mm().pgalloc().pgd_to_pfn(pgd) / pages_per_block(order)) * pages_per_block(order));

        // Iterate whilst there is a block_pointer, and whilst the aim_block is
		// not equal to what the block_pointer is pointing to i.e. the aim_block
		// that conataining the page hasn't been found.
		PageDescriptor **block_pointer = &_free_areas[order];
		while (*block_pointer && aim_block != *block_pointer) {
			block_pointer = &(*block_pointer)->next_free;
		}

		return *block_pointer;
	}

	
	/**
	 * Allocates 2^order number of contiguous pages
	 * @param order The power of two, of the number of contiguous pages to allocate.
	 * @return Returns a pointer to the first page descriptor for the newly allocated page range, or NULL if
	 * allocation failed.
	 */
	PageDescriptor *alloc_pages(int order) override
	{
		// Make sure the order is valid
		assert(order_in_range(order));
		
		// Check whether there exists free block in the order
		int free_order = order;
		PageDescriptor *allocated_block = _free_areas[free_order];
		
		// Increase the order if no free block is avaliable for allocation i.e. _free_areas[free_order] is empty
		while (!allocated_block){ 
			if (!order_in_range(free_order)) return NULL;
			free_order++;
			allocated_block = _free_areas[free_order];
		}

		// Split the block until reach the order to allocate
		while (free_order > order) {
			allocated_block = split_block(&allocated_block, free_order);
			free_order--;
		}

        // Make sure the block is indeed free in the order to allocate and remove it from the free area
		assert(is_free(allocated_block, order));
		remove_block(allocated_block, order);
		
		return allocated_block;
	}

	
	/**
	 * Frees 2^order contiguous pages.
	 * @param pgd A pointer to an array of page descriptors to be freed.
	 * @param order The power of two number of contiguous pages to free.
	 */
	void free_pages(PageDescriptor *pgd, int order) override
	{
		// Make sure that the incoming page descriptor is correctly aligned
		// for the order on which it is being freed, for example, it is
		// illegal to free page 1 in order-1.
		assert(is_correct_alignment_for_order(pgd, order));

		// Make sure the order is in range
		assert(order_in_range(order));

        // Insert the block into the free list of given order 
		insert_block(pgd, order);
		
		// Continuously merge the block with its buddy until the buddy is not free or the maximum order is reached
		do {	
			PageDescriptor *buddy = buddy_of(pgd, order);
		    if (!is_free(buddy, order)) break;

		    merge_block(&pgd, order);
			order ++;
			// Determine the start of the block with higher order
		    pgd = is_correct_alignment_for_order(pgd, order) ? pgd : buddy;
		} while(order < MAX_ORDER-1);

		assert(is_free(pgd, order));

	}
	
	/**
	 * Reserves a specific page, so that it cannot be allocated.
	 * @param pgd The page descriptor of the page to reserve.
	 * @return Returns TRUE if the reservation was successful, FALSE otherwise.
	 */
	bool reserve_page(PageDescriptor *pgd)
	{
		// Start from the maximum order, loop through the free area to find the block containing the page to reserve 
		int order = MAX_ORDER - 1;
		while (order >= 0 && get_block(pgd, order) == NULL) order --;

        // If the block hasn't been found, i.e. the page is not free, return false
		if(!order_in_range(order)) return false;

        // If the page is in a block with order higher than 0, split the allocation blocks down 
		// (as per the buddy allocation algorithm) until only the page being reserved is allocated.
		PageDescriptor *block_containing_page;
		while(order > 0) {
			block_containing_page = get_block(pgd, order);
			split_block(&block_containing_page, order);
			order --;
		}

        // Make sure the page to be reserved is free with order 0
		assert(is_free(pgd,0));
		// Remove the page from free area so that it cannot be allocated i.e. is reserved
		remove_block(pgd, 0);
		
		return true;
		
	}
	
	/**
	 * Initialises the allocation algorithm.
	 * @return Returns TRUE if the algorithm was successfully initialised, FALSE otherwise.
	 */
	bool init(PageDescriptor *page_descriptors, uint64_t nr_page_descriptors) override
	{
		mm_log.messagef(LogLevel::DEBUG, "Buddy Allocator Initialising pd=%p, nr=0x%lx", page_descriptors, nr_page_descriptors);

        // Makesure initialise with enough pages
		assert(nr_page_descriptors > 0);
		
		// Start from the maximum order, continuously combine the remaining pages to blocks with the highest possible order 
		//  and add the block to the free_area until no more page is remained
		int order = MAX_ORDER;
		do {
			order--;

            // Calculate the number of blocks that can be formed by remaining pages in this order
			int num_of_blocks = nr_page_descriptors / pages_per_block(order);
			// Substract the pages from the remaining
			nr_page_descriptors -= num_of_blocks * pages_per_block(order);
			
			// Insert the block into the free_area
			while (num_of_blocks > 0) {
				insert_block(page_descriptors, order);
				page_descriptors += pages_per_block(order);
				num_of_blocks--;
			}

		} while (nr_page_descriptors > 0 && order > 0);

		return true;
	}

	/**
	 * Returns the friendly name of the allocation algorithm, for debugging and selection purposes.
	 */
	const char* name() const override { return "buddy"; }
	
	/**
	 * Dumps out the current state of the buddy system
	 */
	void dump_state() const override
	{
		// Print out a header, so we can find the output in the logs.
		mm_log.messagef(LogLevel::DEBUG, "BUDDY STATE:");
		
		// Iterate over each free area.
		for (unsigned int i = 0; i < ARRAY_SIZE(_free_areas); i++) {
			char buffer[256];
			snprintf(buffer, sizeof(buffer), "[%d] ", i);
						
			// Iterate over each block in the free area.
			PageDescriptor *pg = _free_areas[i];
			while (pg) {
				// Append the PFN of the free block to the output buffer.
				snprintf(buffer, sizeof(buffer), "%s%lx ", buffer, sys.mm().pgalloc().pgd_to_pfn(pg));
				pg = pg->next_free;
			}
			
			mm_log.messagef(LogLevel::DEBUG, "%s", buffer);
		}
	}

	
private:
	PageDescriptor *_free_areas[MAX_ORDER];
};

/* --- DO NOT CHANGE ANYTHING BELOW THIS LINE --- */

/*
 * Allocation algorithm registration framework
 */
RegisterPageAllocator(BuddyPageAllocator);
