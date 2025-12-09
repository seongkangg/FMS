#include "../include/tinyfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

extern Superblock* get_superblock();

static uint8_t* bitmap = NULL;
static uint32_t bitmap_size = 0;
static uint32_t bitmap_blocks = 0;

/* Calculate how many blocks are needed for the bitmap */
static uint32_t calculate_bitmap_blocks(uint32_t total_blocks) {
    uint32_t bits_needed = total_blocks;
    uint32_t bytes_needed = (bits_needed + 7) / 8;   // rounds up value to nearest integer
    return (bytes_needed + BLOCK_SIZE - 1) / BLOCK_SIZE;  // gets number of blocks needed for bitmap
}

/* Initialize the free block bitmap */
int init_bitmap() {
    if (load_superblock() < 0) {
        return -1;
    }

    Superblock* sb = get_superblock();
    if (!sb) {
        return -1;
    }

    bitmap_blocks = calculate_bitmap_blocks(sb->total_blocks);
    bitmap_size = bitmap_blocks * BLOCK_SIZE;
    
    bitmap = calloc(bitmap_size, 1);
    if (!bitmap) {
        return -1;
    }

    /* Mark all blocks as free initially */
    memset(bitmap, 0, bitmap_size);

    /* Mark block 0 (superblock) as used */
    bitmap[0] |= 0x01;

    /* Mark bitmap blocks as used */
    uint32_t start = sb->bitmap_block;
    for (uint32_t i = 0; i < bitmap_blocks; i++) {
        uint32_t byte = (start + i) / 8;
        uint32_t bit = (start + i) % 8;
        bitmap[byte] |= (1 << bit);
    }

    /* Mark inode table blocks as used */
    uint32_t inode_blocks = (sb->inode_count * sizeof(Inode) + BLOCK_SIZE - 1) / BLOCK_SIZE;
    for (uint32_t i = 0; i < inode_blocks; i++) {
        uint32_t block_num = sb->inode_table_block + i;
        uint32_t byte = block_num / 8;
        uint32_t bit = block_num % 8;
        bitmap[byte] |= (1 << bit);
    }

    return save_bitmap();
}

/* Load the bitmap*/
int load_bitmap() {
    if (load_superblock() < 0) {
        return -1;
    }

    Superblock* sb = get_superblock();
    if (!sb) {
        return -1;
    }

    bitmap_blocks = calculate_bitmap_blocks(sb->total_blocks);
    bitmap_size = bitmap_blocks * BLOCK_SIZE;

    if (bitmap) {
        free(bitmap);
    }

    bitmap = malloc(bitmap_size);
    if (!bitmap) {
        return -1;
    }

    /* Read bitmap blocks from disk */
    uint8_t* block_buffer = malloc(BLOCK_SIZE);
    if (!block_buffer) {
        free(bitmap);
        bitmap = NULL;
        return -1;
    }

    for (uint32_t i = 0; i < bitmap_blocks; i++) {
        if (read_block(sb->bitmap_block + i, block_buffer) < 0) {
            free(block_buffer);
            free(bitmap);
            bitmap = NULL;
            return -1;
        }
        memcpy(bitmap + (i * BLOCK_SIZE), block_buffer, BLOCK_SIZE);
    }

    free(block_buffer);
    return 0;
}

/* Save the bitmap*/
int save_bitmap() {
    if (!bitmap || load_superblock() < 0) {
        return -1;
    }

    Superblock* sb = get_superblock();
    if (!sb) {
        return -1;
    }

    uint8_t* block_buffer = malloc(BLOCK_SIZE);
    if (!block_buffer) {
        return -1;
    }

    for (uint32_t i = 0; i < bitmap_blocks; i++) {
        memset(block_buffer, 0, BLOCK_SIZE);
        uint32_t copy_size = (i == bitmap_blocks - 1) ? 
                            (bitmap_size - i * BLOCK_SIZE) : BLOCK_SIZE;
        memcpy(block_buffer, bitmap + (i * BLOCK_SIZE), copy_size);
        
        if (write_block(sb->bitmap_block + i, block_buffer) < 0) {
            free(block_buffer);
            return -1;
        }
    }

    free(block_buffer);
    return 0;
}

/* Allocate a free block */
int allocate_block() {
    if (!bitmap) {
        if (load_bitmap() < 0) {
            return -1;
        }
    }

    if (load_superblock() < 0) {
        return -1;
    }

    Superblock* sb = get_superblock();
    if (!sb) {
        return -1;
    }

    /* Find the first free block */
    for (uint32_t i = 0; i < sb->total_blocks; i++) {
        uint32_t byte = i / 8;
        uint32_t bit = i % 8;
        
        if (!(bitmap[byte] & (1 << bit))) {
            /* Mark block as used */
            bitmap[byte] |= (1 << bit);
            save_bitmap();
            return i;
        }
    }

    return -1; /* No free blocks */
}

/* Free a block */
int free_block(uint32_t block_num) {
    if (!bitmap) {
        if (load_bitmap() < 0) {
            return -1;
        }
    }

    if (load_superblock() < 0) {
        return -1;
    }

    Superblock* sb = get_superblock();
    if (!sb) {
        return -1;
    }

    if (block_num >= sb->total_blocks) {
        return -1;
    }

    uint32_t byte = block_num / 8;
    uint32_t bit = block_num % 8;

    /* Mark block as free */
    bitmap[byte] &= ~(1 << bit);
    return save_bitmap();
}


