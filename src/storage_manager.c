#include "../include/tinyfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

/* RAM-based disk simulation */
static uint8_t* ram_disk = NULL;      /* Memory buffer simulating disk blocks */
static uint32_t total_blocks = 0;     /* Total number of blocks in RAM */
static bool disk_initialized = false; /* Whether disk is initialized */


/* Initialize a new disk in RAM */
int init_disk(uint32_t num_blocks) {
    /* Free existing disk if any */
    if (ram_disk != NULL) {
        free(ram_disk);
        ram_disk = NULL;
    }

    /* Allocate RAM for all blocks */
    ram_disk = calloc(num_blocks, BLOCK_SIZE); //use calloc to automatically initialize the memory to 0
    if (!ram_disk) {
        return -1;
    }

    total_blocks = num_blocks;
    disk_initialized = true;

    /* All blocks are already zero-initialized by calloc */
    return 0;
}


/* Open an existing disk (RAM-only - checks if already initialized) */
int open_disk(void) {
    /* If disk is already initialized, just return success */
    if (disk_initialized && ram_disk != NULL) {
        /* Check if file system is valid by trying to load superblock */
        if (load_superblock() == 0) {
            return 0;
        }
    }
    
    /* Disk not initialized - must call init first */
    return -1;
}


/* Close the disk (RAM-only mode - no persistence) */
int close_disk() {
    /* Don't free memory on close - keep it for potential reopening in same process */
    return 0;
}

/* Free all RAM disk memory (call this when completely done) */
int free_disk() {
    if (ram_disk != NULL) {
        free(ram_disk);
        ram_disk = NULL;
        total_blocks = 0;
        disk_initialized = false;
    }
    return 0;
}

/* Read a block from RAM disk */
int read_block(uint32_t block_num, void* buffer) {
    if (!disk_initialized || !buffer || ram_disk == NULL) {
        return -1;
    }

    if (block_num >= total_blocks) {
        return -1;
    }

    /* Copy block from RAM */
    memcpy(buffer, ram_disk + (block_num * BLOCK_SIZE), BLOCK_SIZE);
    return 0;
}

/* Write a block to RAM disk */
int write_block(uint32_t block_num, const void* buffer) {
    if (!disk_initialized || !buffer || ram_disk == NULL) {
        return -1;
    }

    if (block_num >= total_blocks) {
        return -1;
    }

    /* Copy block to RAM */
    memcpy(ram_disk + (block_num * BLOCK_SIZE), buffer, BLOCK_SIZE);
    return 0;
}

