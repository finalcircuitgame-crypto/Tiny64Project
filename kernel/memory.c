#include "../include/kernel.h"

// Memory Management for Tiny64 OS
// Implements a simple heap-based dynamic memory allocator

#define HEAP_START  0x200000        // Start at 2MB (after kernel)
#define HEAP_SIZE   0x100000        // 1MB heap size
#define HEAP_END    (HEAP_START + HEAP_SIZE)

#define MIN_BLOCK_SIZE  sizeof(block_t)

// Memory block header
typedef struct block {
    size_t size;           // Size of this block (excluding header)
    struct block *next;    // Next block in free list
    uint8_t free;          // 1 = free, 0 = allocated
} __attribute__((packed)) block_t;

// Global heap state
static block_t *free_list = NULL;
static uint8_t heap_initialized = 0;

// Initialize the heap
void init_heap(void) {
    if (heap_initialized) return;

    // Create initial free block spanning entire heap
    block_t *initial_block = (block_t *)HEAP_START;
    initial_block->size = HEAP_SIZE - sizeof(block_t);
    initial_block->next = NULL;
    initial_block->free = 1;

    free_list = initial_block;
    heap_initialized = 1;
}

// Find a free block using first-fit algorithm
static block_t* find_free_block(size_t size) {
    block_t *current = free_list;

    while (current) {
        if (current->free && current->size >= size) {
            return current;
        }
        current = current->next;
    }

    return NULL; // No suitable block found
}

// Split a block if it's too large
static void split_block(block_t *block, size_t size) {
    if (block->size >= size + sizeof(block_t) + MIN_BLOCK_SIZE) {
        // Create new block after this allocation
        block_t *new_block = (block_t *)((uint8_t *)block + sizeof(block_t) + size);
        new_block->size = block->size - size - sizeof(block_t);
        new_block->next = block->next;
        new_block->free = 1;

        // Update current block
        block->size = size;
        block->next = new_block;
    }
}

// Merge adjacent free blocks
static void merge_free_blocks(void) {
    block_t *current = free_list;

    while (current && current->next) {
        // Check if current block and next block are adjacent and both free
        uint8_t *current_end = (uint8_t *)current + sizeof(block_t) + current->size;
        block_t *next_block = (block_t *)current_end;

        if ((uint8_t *)next_block == (uint8_t *)current->next &&
            current->free && current->next->free) {

            // Merge blocks
            current->size += sizeof(block_t) + current->next->size;
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
}

// Allocate memory
void* kmalloc(size_t size) {
    if (!heap_initialized) init_heap();
    if (size == 0) return NULL;

    // Prevent excessive allocations
    if (size > HEAP_SIZE / 4) return NULL;

    // Align size to 8 bytes for better performance
    size = (size + 7) & ~7;

    // Find a suitable free block
    block_t *block = find_free_block(size);
    if (!block) {
        return NULL; // Out of memory
    }

    // Split block if necessary
    split_block(block, size);

    // Mark block as allocated
    block->free = 0;

    // Return pointer to data area
    void *ptr = (void *)((uint8_t *)block + sizeof(block_t));

    // Safety check: ensure pointer is within heap bounds
    if ((uint8_t *)ptr < (uint8_t *)HEAP_START ||
        (uint8_t *)ptr >= (uint8_t *)HEAP_END) {
        return NULL;
    }

    return ptr;
}

// Free memory
void kfree(void *ptr) {
    if (!ptr || !heap_initialized) return;

    // Bounds check
    if ((uint8_t *)ptr < (uint8_t *)HEAP_START + sizeof(block_t) ||
        (uint8_t *)ptr >= (uint8_t *)HEAP_END) {
        return; // Invalid pointer
    }

    // Get block header from data pointer
    block_t *block = (block_t *)((uint8_t *)ptr - sizeof(block_t));

    // Additional validation
    if (block->size == 0 || block->size > HEAP_SIZE) {
        return; // Corrupted block
    }

    // Mark as free
    block->free = 1;

    // Merge adjacent free blocks
    merge_free_blocks();
}

// Get heap statistics
void get_heap_stats(size_t *total_size, size_t *used_size, size_t *free_size) {
    if (!heap_initialized) {
        *total_size = *used_size = *free_size = 0;
        return;
    }

    *total_size = HEAP_SIZE;
    *used_size = 0;
    *free_size = 0;

    block_t *current = (block_t *)HEAP_START;
    uint8_t *heap_ptr = (uint8_t *)HEAP_START;

    while (heap_ptr < (uint8_t *)HEAP_END) {
        block_t *block = (block_t *)heap_ptr;

        if (block->free) {
            *free_size += block->size;
        } else {
            *used_size += block->size;
        }

        heap_ptr += sizeof(block_t) + block->size;
    }
}

// Debug function to print heap layout
void debug_heap(BootInfo *info, int start_y) {
    if (!heap_initialized) {
        kprint(info, "Heap not initialized", 10, start_y, 0xFFFF0000);
        return;
    }

    kprint(info, "Heap Layout:", 10, start_y, 0xFFFFFFFF);
    start_y += 20;

    block_t *current = (block_t *)HEAP_START;
    uint8_t *heap_ptr = (uint8_t *)HEAP_START;
    int block_num = 0;

    while (heap_ptr < (uint8_t *)HEAP_END && block_num < 10) { // Show first 10 blocks
        block_t *block = (block_t *)heap_ptr;

        char buffer[64];
        // This is a simplified print - you'd need a proper sprintf
        kprint(info, block->free ? "FREE" : "USED", 10, start_y, block->free ? 0xFF00FF00 : 0xFFFF0000);

        // Print size (simplified)
        kprint(info, " block", 80, start_y, 0xFFFFFFFF);

        start_y += 15;
        heap_ptr += sizeof(block_t) + block->size;
        block_num++;
    }
}
