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
static const char* current_disk_name = NULL; /* Current disk name for persistence */

/* Forward declarations for static helper functions */
static const char* get_config_filename(const char* disk_name);
static const char* get_disk_filename(const char* disk_name);
static int save_disk_config(const char* disk_name, uint32_t num_blocks);
static int load_ram_disk_from_file(const char* disk_name);
static int save_ram_disk_to_file(const char* disk_name);

/* Initialize a new disk in RAM */
int init_disk(const char* disk_name, uint32_t num_blocks) {
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
    current_disk_name = disk_name;
    
    /* Save configuration for future opens */
    save_disk_config(disk_name, num_blocks);

    /* All blocks are already zero-initialized by calloc */
    return 0;
}

/* Helper function to get config file name from disk name */
static const char* get_config_filename(const char* disk_name) {
    static char config_file[256];
    snprintf(config_file, sizeof(config_file), ".%s.config", disk_name);
    return config_file;
}

/* Helper function to get disk image file name from disk name */
static const char* get_disk_filename(const char* disk_name) {
    static char disk_file[256];
    snprintf(disk_file, sizeof(disk_file), ".%s.ramdisk", disk_name);
    return disk_file;
}

/* Save disk configuration (block count) to a file for persistence */
static int save_disk_config(const char* disk_name, uint32_t num_blocks) {
    const char* config_file = get_config_filename(disk_name);
    // check if the file is opened successfully
    FILE* f = fopen(config_file, "wb");
    if (!f) {
        return -1;
    }
    
    uint32_t magic = 0x54494E59; /* "TINY" */
    fwrite(&magic, sizeof(magic), 1, f);
    fwrite(&num_blocks, sizeof(num_blocks), 1, f);
    fclose(f);
    
    /* Also save as the last used disk */
    FILE* last_disk = fopen(".last_disk", "w");
    if (last_disk) {
        fprintf(last_disk, "%s", disk_name);
        fclose(last_disk);
    }
    
    return 0;
}

/* Get the last initialized disk name */
static int get_last_disk_name(char* disk_name, size_t size) {
    FILE* f = fopen(".last_disk", "r");
    // check if the file is opened successfully
    if (!f) {
        return -1;
    }
    
    if (fgets(disk_name, size, f) == NULL) {
        fclose(f);
        return -1;
    }
    
    /* Remove newline if present */
    size_t len = strlen(disk_name);
    if (len > 0 && disk_name[len - 1] == '\n') {
        disk_name[len - 1] = '\0';
    }
    
    fclose(f);
    return 0;
}

/* Load disk configuration (block count) from file */
static int load_disk_config(const char* disk_name, uint32_t* num_blocks) {
    const char* config_file = get_config_filename(disk_name);
    FILE* f = fopen(config_file, "rb");
    if (!f) {
        return -1;
    }
    
    uint32_t magic;
    if (fread(&magic, sizeof(magic), 1, f) != 1 || magic != 0x54494E59) {
        fclose(f);
        return -1;
    }
    
    if (fread(num_blocks, sizeof(*num_blocks), 1, f) != 1) {
        fclose(f);
        return -1;
    }
    
    fclose(f);
    return 0;
}

/* Open an existing disk (for RAM simulation, load from saved state if available) */
int open_disk(const char* disk_name) {
    /* If disk is already initialized with the same name, just return success */
    if (disk_initialized && ram_disk != NULL) {
        /* Check if it's the same disk by trying to load superblock */
        if (load_superblock() == 0) {
            return 0;
        }
        /* Different disk or corrupted, need to reload */
    }
    
    /* Try to load RAM disk from file */
    if (load_ram_disk_from_file(disk_name) == 0) {
        /* Successfully loaded RAM disk, verify file system is valid */
        if (load_superblock() == 0) {
            if (load_bitmap() == 0) {
                /* Load inode table into memory */
                if (load_inode_table() == 0) {
                    return 0; /* Successfully loaded existing file system */
                }
            }
        }
        /* File system is corrupted, free and return error */
        if (ram_disk != NULL) {
            free(ram_disk);
            ram_disk = NULL;
            disk_initialized = false;
        }
    }
    
    /* No saved state found or loading failed */
    return -1;
}

/* Save RAM disk to file for persistence across processes */
static int save_ram_disk_to_file(const char* disk_name) {
    if (!disk_initialized || ram_disk == NULL || total_blocks == 0) {
        return -1;
    }
    
    const char* disk_file = get_disk_filename(disk_name);
    FILE* f = fopen(disk_file, "wb");
    if (!f) {
        return -1;
    }
    
    /* Write header: magic number, block count, then all blocks */
    uint32_t magic = 0x54494E59; /* "TINY" */
    fwrite(&magic, sizeof(magic), 1, f);
    fwrite(&total_blocks, sizeof(total_blocks), 1, f);
    fwrite(ram_disk, BLOCK_SIZE, total_blocks, f);
    fclose(f);
    return 0;
}

/* Load RAM disk from file */
static int load_ram_disk_from_file(const char* disk_name) {
    const char* disk_file = get_disk_filename(disk_name);
    FILE* f = fopen(disk_file, "rb");
    if (!f) {
        return -1;
    }
    
    uint32_t magic;
    uint32_t num_blocks;
    if (fread(&magic, sizeof(magic), 1, f) != 1 || magic != 0x54494E59) {
        fclose(f);
        return -1;
    }
    
    if (fread(&num_blocks, sizeof(num_blocks), 1, f) != 1) {
        fclose(f);
        return -1;
    }
    
    /* Allocate RAM for blocks */
    if (ram_disk != NULL) {
        free(ram_disk);
    }
    ram_disk = malloc(num_blocks * BLOCK_SIZE);
    if (!ram_disk) {
        fclose(f);
        return -1;
    }
    
    if (fread(ram_disk, BLOCK_SIZE, num_blocks, f) != num_blocks) {
        free(ram_disk);
        ram_disk = NULL;
        fclose(f);
        return -1;
    }
    
    total_blocks = num_blocks;
    disk_initialized = true;
    current_disk_name = disk_name;
    fclose(f);
    return 0;
}

/* Close the disk (for RAM simulation, save to file for persistence) */
int close_disk() {
    /* Save RAM disk to file so it persists across process invocations */
    if (disk_initialized && current_disk_name) {
        save_ram_disk_to_file(current_disk_name);
    }
    
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

