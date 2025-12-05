#ifndef TINYFS_H
#define TINYFS_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* Constants */
#define BLOCK_SIZE 256
#define MAX_BLOCKS 1024
#define MAX_FILENAME_LEN 32
#define MAX_PATH_LEN 256
#define MAX_OPEN_FILES 64
#define MAX_INODES 128
#define MAGIC_NUMBER 0x54494E59  /* "TINY" */
#define ROOT_INODE 0

/* File System Version */
#define FS_VERSION 1

/* Inode Types */
#define TYPE_FILE 1
#define TYPE_DIRECTORY 2

/* File Access Modes */
#define MODE_READ 1
#define MODE_WRITE 2
#define MODE_APPEND 4

/* Superblock Structure */
typedef struct {
    uint32_t magic;              /* Magic number to identify file system */
    uint32_t version;            /* File system version */
    uint32_t total_blocks;       /* Total number of blocks in the file system */
    uint32_t block_size;         /* Size of each block */
    uint32_t inode_table_block;  /* Starting block of inode table */
    uint32_t inode_count;        /* Number of inodes */
    uint32_t root_inode;         /* Root directory inode number */
    uint32_t bitmap_block;       /* Starting block of free block bitmap */
    uint32_t data_start_block;   /* Starting block of data area */
    time_t created_time;         /* File system creation time */
    uint32_t reserved[16];       /* Reserved for future use */
} Superblock;

/* Inode Structure (File Control Block) */
typedef struct {
    uint32_t inode_num;          /* Inode number */
    uint8_t type;                /* TYPE_FILE or TYPE_DIRECTORY */
    char name[MAX_FILENAME_LEN]; /* File or directory name */
    uint32_t size;               /* Size of file in bytes */
    uint32_t data_block;         /* Starting data block number */
    uint32_t parent_inode;       /* Parent directory inode */
    time_t created_time;         /* Creation timestamp */
    time_t modified_time;        /* Last modification timestamp */
    time_t accessed_time;        /* Last access timestamp */
    uint8_t used;                /* 1 if inode is in use, 0 if free */
    uint32_t reserved[8];        /* Reserved for future use */
} Inode;

/* Directory Entry Structure */
typedef struct {
    char name[MAX_FILENAME_LEN]; /* File or directory name */
    uint32_t inode_num;          /* Corresponding inode number */
    uint8_t type;                /* TYPE_FILE or TYPE_DIRECTORY */
} DirectoryEntry;

/* Open File Table Entry */
typedef struct {
    int fd;                      /* File descriptor */
    uint32_t inode_num;          /* Inode number of the file */
    uint32_t position;           /* Current read/write position */
    uint8_t mode;                /* Access mode (read, write, append) */
    bool in_use;                 /* Whether this entry is in use */
} OpenFileEntry;

/* Function Prototypes */

/* Storage Manager Functions */
int init_disk(uint32_t num_blocks);
int open_disk(void);
int close_disk(void);
int read_block(uint32_t block_num, void* buffer);
int write_block(uint32_t block_num, const void* buffer);

/* Allocator Functions */
int init_bitmap();
int allocate_block();
int free_block(uint32_t block_num);
bool is_block_free(uint32_t block_num);
int load_bitmap();
int save_bitmap();

/* Metadata Manager Functions */
int init_filesystem(uint32_t num_blocks);
int load_superblock();
int save_superblock();
int load_inode_table();
int reload_inode_table();
int load_inode(uint32_t inode_num, Inode* inode);
int save_inode(const Inode* inode);
uint32_t allocate_inode();
int free_inode(uint32_t inode_num);
int init_root_directory();
int get_open_file_index();
int release_open_file(int fd);

/* API Layer - File Operations */
int createFile(const char* path, uint8_t type);
int openFile(const char* path, uint8_t mode);
int closeFile(int fd);
int readFile(int fd, void* buffer, uint32_t size);
int writeFile(int fd, const void* buffer, uint32_t size);
int deleteFile(const char* path);
int searchFile(const char* path);

/* API Layer - Directory Operations */
int makeDirectory(const char* path);
int removeDirectory(const char* path);
int listDirectory(const char* path, char* output, uint32_t output_size);
int searchDirectory(const char* path, const char* pattern);

/* Helper Functions */
int parse_path(const char* path, char components[][MAX_FILENAME_LEN], int* count);
uint32_t find_inode_by_path(const char* path);
int get_file_descriptor(uint32_t inode_num, uint8_t mode);
int read_directory_entries(uint32_t dir_inode, DirectoryEntry* entries, int max_entries);
int add_directory_entry(uint32_t dir_inode, const char* name, uint32_t inode_num, uint8_t type);
int remove_directory_entry(uint32_t dir_inode, const char* name);
bool is_directory_empty(uint32_t dir_inode);

#endif /* TINYFS_H */

