#include "../include/tinyfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static Superblock superblock_data;
static Inode inode_table[MAX_INODES];
OpenFileEntry open_file_table[MAX_OPEN_FILES];
static bool superblock_loaded = false;
static bool inode_table_loaded = false;

/* Get reference to superblock */
Superblock* get_superblock() {
    if (!superblock_loaded) {
        if (load_superblock() < 0) {
            return NULL;
        }
    }
    return &superblock_data;
}

/* Load superblock from disk */
int load_superblock() {
    if (read_block(0, &superblock_data) < 0) {
        return -1;
    }

    if (superblock_data.magic != MAGIC_NUMBER) {
        return -1;
    }

    superblock_loaded = true;
    return 0;
}

/* Save superblock to disk */
int save_superblock() {
    if (write_block(0, &superblock_data) < 0) {
        return -1;
    }
    return 0;
}

/* Initialize file system metadata */
int init_filesystem(uint32_t num_blocks) {
    /* Initialize disk */
    if (init_disk(num_blocks) < 0) {
        return -1;
    }

    /* Calculate layout */
    uint32_t inode_blocks = (MAX_INODES * sizeof(Inode) + BLOCK_SIZE - 1) / BLOCK_SIZE;
    uint32_t bitmap_bytes = (num_blocks + 7) / 8;
    uint32_t bitmap_blocks = (bitmap_bytes + BLOCK_SIZE - 1) / BLOCK_SIZE;
    uint32_t data_start = 1 + bitmap_blocks + inode_blocks;

    /* Initialize superblock */
    memset(&superblock_data, 0, sizeof(Superblock));
    superblock_data.magic = MAGIC_NUMBER;
    superblock_data.version = FS_VERSION;
    superblock_data.total_blocks = num_blocks;
    superblock_data.block_size = BLOCK_SIZE;
    superblock_data.bitmap_block = 1;
    superblock_data.inode_table_block = 1 + bitmap_blocks;
    superblock_data.inode_count = MAX_INODES;
    superblock_data.root_inode = ROOT_INODE;
    superblock_data.data_start_block = data_start;
    superblock_data.created_time = time(NULL);

    if (save_superblock() < 0) {
        return -1;
    }

    /* Initialize inode table in memory */
    memset(inode_table, 0, sizeof(inode_table));

    /* Initialize bitmap */
    if (init_bitmap() < 0) {
        return -1;
    }

    /* Initialize root directory */
    if (init_root_directory() < 0) {
        return -1;
    }

    superblock_loaded = true;
    return 0;
}

/* Load inode table from disk */
int load_inode_table() {
    if (!superblock_loaded) {
        if (load_superblock() < 0) {
            return -1;
        }
    }

    uint32_t inode_blocks = (MAX_INODES * sizeof(Inode) + BLOCK_SIZE - 1) / BLOCK_SIZE;
    uint8_t* block_buffer = malloc(BLOCK_SIZE);
    if (!block_buffer) {
        return -1;
    }

    for (uint32_t i = 0; i < inode_blocks; i++) {
        if (read_block(superblock_data.inode_table_block + i, block_buffer) < 0) {
            free(block_buffer);
            return -1;
        }

        uint32_t inodes_per_block = BLOCK_SIZE / sizeof(Inode);
        uint32_t start_inode = i * inodes_per_block;
        uint32_t copy_count = (start_inode + inodes_per_block > MAX_INODES) ?
                             (MAX_INODES - start_inode) : inodes_per_block;

        memcpy(&inode_table[start_inode], block_buffer, copy_count * sizeof(Inode));
    }

    free(block_buffer);
    inode_table_loaded = true;
    return 0;
}

/* Force reload inode table from disk (clears cached version) */
int reload_inode_table() {
    inode_table_loaded = false;
    return load_inode_table();
}

/* Save inode table to disk */
static int save_inode_table() {
    if (!superblock_loaded) {
        if (load_superblock() < 0) {
            return -1;
        }
    }

    uint32_t inode_blocks = (MAX_INODES * sizeof(Inode) + BLOCK_SIZE - 1) / BLOCK_SIZE;
    uint8_t* block_buffer = malloc(BLOCK_SIZE);
    if (!block_buffer) {
        return -1;
    }

    for (uint32_t i = 0; i < inode_blocks; i++) {
        memset(block_buffer, 0, BLOCK_SIZE);

        uint32_t inodes_per_block = BLOCK_SIZE / sizeof(Inode);
        uint32_t start_inode = i * inodes_per_block;
        uint32_t copy_count = (start_inode + inodes_per_block > MAX_INODES) ?
                             (MAX_INODES - start_inode) : inodes_per_block;

        memcpy(block_buffer, &inode_table[start_inode], copy_count * sizeof(Inode));

        if (write_block(superblock_data.inode_table_block + i, block_buffer) < 0) {
            free(block_buffer);
            return -1;
        }
    }

    free(block_buffer);
    return 0;
}

/* Load an inode from the inode table */
int load_inode(uint32_t inode_num, Inode* inode) {
    if (!inode_table_loaded) {
        if (load_inode_table() < 0) {
            return -1;
        }
    }

    if (inode_num >= MAX_INODES || !inode) {
        return -1;
    }

    *inode = inode_table[inode_num];
    return 0;
}

/* Save an inode to the inode table */
int save_inode(const Inode* inode) {
    if (!inode || inode->inode_num >= MAX_INODES) {
        return -1;
    }

    if (!inode_table_loaded) {
        if (load_inode_table() < 0) {
            return -1;
        }
    }

    inode_table[inode->inode_num] = *inode;
    return save_inode_table();
}

/* Allocate a free inode */
uint32_t allocate_inode() {
    if (!inode_table_loaded) {
        if (load_inode_table() < 0) {
            return (uint32_t)-1;
        }
    }

    for (uint32_t i = 0; i < MAX_INODES; i++) {
        if (!inode_table[i].used) {
            memset(&inode_table[i], 0, sizeof(Inode));
            inode_table[i].inode_num = i;
            inode_table[i].used = 1;
            inode_table[i].created_time = time(NULL);
            inode_table[i].modified_time = time(NULL);
            inode_table[i].accessed_time = time(NULL);
            save_inode_table();
            return i;
        }
    }

    return (uint32_t)-1; /* No free inodes */
}

/* Free an inode */
int free_inode(uint32_t inode_num) {
    if (!inode_table_loaded) {
        if (load_inode_table() < 0) {
            return -1;
        }
    }

    if (inode_num >= MAX_INODES) {
        return -1;
    }

    inode_table[inode_num].used = 0;
    return save_inode_table();
}

/* Initialize root directory */
int init_root_directory() {
    uint32_t root_inode = allocate_inode();
    if (root_inode == (uint32_t)-1) {
        return -1;
    }

    Inode root;
    root.inode_num = root_inode;
    root.type = TYPE_DIRECTORY;
    strncpy(root.name, "/", MAX_FILENAME_LEN - 1);
    root.name[MAX_FILENAME_LEN - 1] = '\0';
    root.size = 0;
    root.parent_inode = root_inode; /* Root is its own parent */
    root.created_time = time(NULL);
    root.modified_time = time(NULL);
    root.accessed_time = time(NULL);
    root.used = 1;

    /* Allocate a data block for directory entries */
    root.data_block = allocate_block();
    if (root.data_block == (uint32_t)-1) {
        free_inode(root_inode);
        return -1;
    }

    /* Initialize directory as empty */
    uint8_t* block = calloc(BLOCK_SIZE, 1);
    if (!block) {
        free_block(root.data_block);
        free_inode(root_inode);
        return -1;
    }

    write_block(root.data_block, block);
    free(block);

    if (save_inode(&root) < 0) {
        free_block(root.data_block);
        free_inode(root_inode);
        return -1;
    }

    superblock_data.root_inode = root_inode;
    return save_superblock();
}

/* Get an available slot in the open file table */
int get_open_file_index() {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!open_file_table[i].in_use) {
            return i;
        }
    }
    return -1;
}

/* Release an open file entry */
int release_open_file(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES) {
        return -1;
    }

    if (!open_file_table[fd].in_use) {
        return -1;
    }

    open_file_table[fd].in_use = false;
    return 0;
}

/* Get open file entry by file descriptor */
OpenFileEntry* get_open_file_entry(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES) {
        return NULL;
    }

    if (!open_file_table[fd].in_use) {
        return NULL;
    }

    return &open_file_table[fd];
}

/* Initialize open file table */
void init_open_file_table() {
    memset(open_file_table, 0, sizeof(open_file_table));
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        open_file_table[i].fd = i;
    }
}

