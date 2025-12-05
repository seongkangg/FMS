#include "../include/tinyfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

extern Superblock* get_superblock();
extern OpenFileEntry* get_open_file_entry(int fd);
extern void init_open_file_table();
extern OpenFileEntry open_file_table[];

/* Parse a path into components */
int parse_path(const char* path, char components[][MAX_FILENAME_LEN], int* count) {
    if (!path || !components || !count) {
        return -1;
    }

    *count = 0;
    
    /* Skip leading slash */
    const char* p = path;
    if (*p == '/') {
        p++;
    }

    if (*p == '\0') {
        /* Root path */
        return 0;
    }

    char component[MAX_FILENAME_LEN];
    int comp_idx = 0;

    while (*p != '\0' && *count < 32) {
        if (*p == '/') {
            if (comp_idx > 0) {
                component[comp_idx] = '\0';
                strncpy(components[*count], component, MAX_FILENAME_LEN - 1);
                components[*count][MAX_FILENAME_LEN - 1] = '\0';
                (*count)++;
                comp_idx = 0;
            }
        } else {
            if (comp_idx >= MAX_FILENAME_LEN - 1) {
                return -1; /* Component too long */
            }
            component[comp_idx++] = *p;
        }
        p++;
    }

    if (comp_idx > 0) {
        component[comp_idx] = '\0';
        strncpy(components[*count], component, MAX_FILENAME_LEN - 1);
        components[*count][MAX_FILENAME_LEN - 1] = '\0';
        (*count)++;
    }

    return 0;
}

/* Find inode by path */
uint32_t find_inode_by_path(const char* path) {
    if (!path) {
        return (uint32_t)-1;
    }

    if (load_superblock() < 0) {
        return (uint32_t)-1;
    }

    /* Ensure inode table is loaded */
    if (load_inode_table() < 0) {
        return (uint32_t)-1;
    }

    Superblock* sb = get_superblock();
    if (!sb) {
        return (uint32_t)-1;
    }

    /* Root path */
    if (strcmp(path, "/") == 0) {
        return sb->root_inode;
    }

    char components[32][MAX_FILENAME_LEN];
    int count = 0;
    
    if (parse_path(path, components, &count) < 0) {
        return (uint32_t)-1;
    }

    /* Start from root */
    uint32_t current_inode = sb->root_inode;
    
    for (int i = 0; i < count; i++) {
        Inode current;
        if (load_inode(current_inode, &current) < 0) {
            return (uint32_t)-1;
        }

        if (current.type != TYPE_DIRECTORY) {
            return (uint32_t)-1;
        }

        /* Read directory entries */
        uint8_t* block = malloc(BLOCK_SIZE);
        if (!block) {
            return (uint32_t)-1;
        }

        if (read_block(current.data_block, block) < 0) {
            free(block);
            return (uint32_t)-1;
        }

        DirectoryEntry* entries = (DirectoryEntry*)block;
        int entries_per_block = BLOCK_SIZE / sizeof(DirectoryEntry);
        bool found = false;

        for (int j = 0; j < entries_per_block; j++) {
            if (entries[j].name[0] != '\0' && 
                strcmp(entries[j].name, components[i]) == 0) {
                current_inode = entries[j].inode_num;
                found = true;
                break;
            }
        }

        free(block);

        if (!found) {
            return (uint32_t)-1;
        }
    }

    return current_inode;
}

/* Read directory entries */
int read_directory_entries(uint32_t dir_inode, DirectoryEntry* entries, int max_entries) {
    Inode inode;
    if (load_inode(dir_inode, &inode) < 0) {
        return -1;
    }

    if (inode.type != TYPE_DIRECTORY) {
        return -1;
    }

    uint8_t* block = malloc(BLOCK_SIZE);
    if (!block) {
        return -1;
    }

    if (read_block(inode.data_block, block) < 0) {
        free(block);
        return -1;
    }

    DirectoryEntry* dir_entries = (DirectoryEntry*)block;
    int entries_per_block = BLOCK_SIZE / sizeof(DirectoryEntry);
    int count = 0;

    for (int i = 0; i < entries_per_block && count < max_entries; i++) {
        if (dir_entries[i].name[0] != '\0') {
            entries[count++] = dir_entries[i];
        }
    }

    free(block);
    return count;
}

/* Add directory entry */
int add_directory_entry(uint32_t dir_inode, const char* name, uint32_t inode_num, uint8_t type) {
    Inode inode;
    if (load_inode(dir_inode, &inode) < 0) {
        return -1;
    }

    if (inode.type != TYPE_DIRECTORY) {
        return -1;
    }

    uint8_t* block = malloc(BLOCK_SIZE);
    if (!block) {
        return -1;
    }

    if (read_block(inode.data_block, block) < 0) {
        free(block);
        return -1;
    }

    DirectoryEntry* entries = (DirectoryEntry*)block;
    int entries_per_block = BLOCK_SIZE / sizeof(DirectoryEntry);

    /* Check if entry already exists */
    for (int i = 0; i < entries_per_block; i++) {
        if (entries[i].name[0] != '\0' && strcmp(entries[i].name, name) == 0) {
            free(block);
            return -1; /* Entry already exists */
        }
    }

    /* Find empty slot */
    for (int i = 0; i < entries_per_block; i++) {
        if (entries[i].name[0] == '\0') {
            strncpy(entries[i].name, name, MAX_FILENAME_LEN - 1);
            entries[i].name[MAX_FILENAME_LEN - 1] = '\0';
            entries[i].inode_num = inode_num;
            entries[i].type = type;
            
            if (write_block(inode.data_block, block) < 0) {
                free(block);
                return -1;
            }

            inode.modified_time = time(NULL);
            save_inode(&inode);
            
            free(block);
            return 0;
        }
    }

    free(block);
    return -1; /* Directory full */
}

/* Remove directory entry */
int remove_directory_entry(uint32_t dir_inode, const char* name) {
    Inode inode;
    if (load_inode(dir_inode, &inode) < 0) {
        return -1;
    }

    if (inode.type != TYPE_DIRECTORY) {
        return -1;
    }

    uint8_t* block = malloc(BLOCK_SIZE);
    if (!block) {
        return -1;
    }

    if (read_block(inode.data_block, block) < 0) {
        free(block);
        return -1;
    }

    DirectoryEntry* entries = (DirectoryEntry*)block;
    int entries_per_block = BLOCK_SIZE / sizeof(DirectoryEntry);

    for (int i = 0; i < entries_per_block; i++) {
        if (entries[i].name[0] != '\0' && strcmp(entries[i].name, name) == 0) {
            memset(&entries[i], 0, sizeof(DirectoryEntry));
            
            if (write_block(inode.data_block, block) < 0) {
                free(block);
                return -1;
            }

            inode.modified_time = time(NULL);
            save_inode(&inode);
            
            free(block);
            return 0;
        }
    }

    free(block);
    return -1; /* Entry not found */
}

/* Check if directory is empty */
bool is_directory_empty(uint32_t dir_inode) {
    DirectoryEntry entries[16];
    int count = read_directory_entries(dir_inode, entries, 16);
    return count == 0;
}

/* Get file descriptor for an open file */
int get_file_descriptor(uint32_t inode_num, uint8_t mode) {
    int index = get_open_file_index();
    if (index < 0) {
        return -1;
    }

    open_file_table[index].inode_num = inode_num;
    open_file_table[index].position = 0;
    open_file_table[index].mode = mode;
    open_file_table[index].in_use = true;

    return index;
}

/* Create a file or directory */
int createFile(const char* path, uint8_t type) {
    if (!path) {
        return -1;
    }

    if (load_superblock() < 0) {
        return -1;
    }

    /* Find parent directory */
    char parent_path[MAX_PATH_LEN];
    char filename[MAX_FILENAME_LEN];
    
    const char* last_slash = strrchr(path, '/');
    if (!last_slash || last_slash == path) {
        /* Root directory or invalid path */
        strncpy(parent_path, "/", MAX_PATH_LEN - 1);
        strncpy(filename, path + (path[0] == '/' ? 1 : 0), MAX_FILENAME_LEN - 1);
    } else {
        size_t parent_len = last_slash - path;
        if (parent_len == 0) parent_len = 1;
        strncpy(parent_path, path, parent_len);
        parent_path[parent_len] = '\0';
        if (parent_path[0] != '/') {
            parent_path[0] = '/';
            parent_path[1] = '\0';
        }
        strncpy(filename, last_slash + 1, MAX_FILENAME_LEN - 1);
    }

    filename[MAX_FILENAME_LEN - 1] = '\0';

    uint32_t parent_inode = find_inode_by_path(parent_path);
    if (parent_inode == (uint32_t)-1) {
        return -1;
    }

    /* Check if file already exists */
    Inode parent;
    if (load_inode(parent_inode, &parent) < 0) {
        return -1;
    }

    uint8_t* block = malloc(BLOCK_SIZE);
    if (!block) {
        return -1;
    }

    if (read_block(parent.data_block, block) < 0) {
        free(block);
        return -1;
    }

    DirectoryEntry* entries = (DirectoryEntry*)block;
    int entries_per_block = BLOCK_SIZE / sizeof(DirectoryEntry);

    for (int i = 0; i < entries_per_block; i++) {
        if (entries[i].name[0] != '\0' && strcmp(entries[i].name, filename) == 0) {
            free(block);
            return -1; /* File already exists */
        }
    }

    free(block);

    /* Allocate new inode */
    uint32_t new_inode = allocate_inode();
    if (new_inode == (uint32_t)-1) {
        return -1;
    }

    Inode inode;
    inode.inode_num = new_inode;
    inode.type = type;
    strncpy(inode.name, filename, MAX_FILENAME_LEN - 1);
    inode.name[MAX_FILENAME_LEN - 1] = '\0';
    inode.size = 0;
    inode.parent_inode = parent_inode;
    inode.created_time = time(NULL);
    inode.modified_time = time(NULL);
    inode.accessed_time = time(NULL);
    inode.used = 1;

    if (type == TYPE_DIRECTORY) {
        /* Allocate data block for directory entries */
        inode.data_block = allocate_block();
        if (inode.data_block == (uint32_t)-1) {
            free_inode(new_inode);
            return -1;
        }

        /* Initialize directory as empty */
        uint8_t* dir_block = calloc(BLOCK_SIZE, 1);
        if (!dir_block) {
            free_block(inode.data_block);
            free_inode(new_inode);
            return -1;
        }
        write_block(inode.data_block, dir_block);
        free(dir_block);
    } else {
        inode.data_block = 0; /* Will be allocated on first write */
    }

    if (save_inode(&inode) < 0) {
        if (type == TYPE_DIRECTORY && inode.data_block != 0) {
            free_block(inode.data_block);
        }
        free_inode(new_inode);
        return -1;
    }

    /* Add to parent directory */
    if (add_directory_entry(parent_inode, filename, new_inode, type) < 0) {
        if (type == TYPE_DIRECTORY && inode.data_block != 0) {
            free_block(inode.data_block);
        }
        free_inode(new_inode);
        return -1;
    }

    return 0;
}

/* Open a file */
int openFile(const char* path, uint8_t mode) {
    uint32_t inode_num = find_inode_by_path(path);
    if (inode_num == (uint32_t)-1) {
        return -1;
    }

    Inode inode;
    if (load_inode(inode_num, &inode) < 0) {
        return -1;
    }

    /* Check if inode is actually used */
    if (!inode.used) {
        return -1;
    }

    if (inode.type != TYPE_FILE) {
        return -1; /* Not a file */
    }

    int fd = get_file_descriptor(inode_num, mode);
    if (fd < 0) {
        return -1;
    }

    /* Update access time */
    inode.accessed_time = time(NULL);
    save_inode(&inode);

    return fd;
}

/* Close a file */
int closeFile(int fd) {
    OpenFileEntry* entry = get_open_file_entry(fd);
    if (!entry) {
        return -1;
    }

    Inode inode;
    if (load_inode(entry->inode_num, &inode) >= 0) {
        inode.accessed_time = time(NULL);
        save_inode(&inode);
    }

    return release_open_file(fd);
}

/* Read from a file */
int readFile(int fd, void* buffer, uint32_t size) {
    OpenFileEntry* entry = get_open_file_entry(fd);
    if (!entry || !buffer) {
        return -1;
    }

    if (!(entry->mode & MODE_READ)) {
        return -1; /* File not opened for reading */
    }

    Inode inode;
    if (load_inode(entry->inode_num, &inode) < 0) {
        return -1;
    }

    if (inode.type != TYPE_FILE) {
        return -1;
    }

    if (entry->position >= inode.size) {
        return 0; /* EOF */
    }

    if (inode.data_block == 0) {
        return 0; /* Empty file */
    }

    uint32_t bytes_to_read = size;
    if (entry->position + bytes_to_read > inode.size) {
        bytes_to_read = inode.size - entry->position;
    }

    uint8_t* block = malloc(BLOCK_SIZE);
    if (!block) {
        return -1;
    }

    if (read_block(inode.data_block, block) < 0) {
        free(block);
        return -1;
    }

    memcpy(buffer, block + entry->position, bytes_to_read);
    entry->position += bytes_to_read;

    inode.accessed_time = time(NULL);
    save_inode(&inode);

    free(block);
    return bytes_to_read;
}

/* Write to a file */
int writeFile(int fd, const void* buffer, uint32_t size) {
    OpenFileEntry* entry = get_open_file_entry(fd);
    if (!entry || !buffer) {
        return -1;
    }

    if (!(entry->mode & (MODE_WRITE | MODE_APPEND))) {
        return -1; /* File not opened for writing */
    }

    Inode inode;
    if (load_inode(entry->inode_num, &inode) < 0) {
        return -1;
    }

    if (inode.type != TYPE_FILE) {
        return -1;
    }

    /* Allocate data block if needed */
    if (inode.data_block == 0) {
        inode.data_block = allocate_block();
        if (inode.data_block == (uint32_t)-1) {
            return -1;
        }
    }

    uint8_t* block = malloc(BLOCK_SIZE);
    if (!block) {
        return -1;
    }

    if (read_block(inode.data_block, block) < 0) {
        free(block);
        return -1;
    }

    uint32_t write_pos = (entry->mode & MODE_APPEND) ? inode.size : entry->position;
    uint32_t bytes_to_write = size;
    if (write_pos + bytes_to_write > BLOCK_SIZE) {
        bytes_to_write = BLOCK_SIZE - write_pos;
    }

    memcpy(block + write_pos, buffer, bytes_to_write);
    
    if (write_block(inode.data_block, block) < 0) {
        free(block);
        return -1;
    }

    if (write_pos + bytes_to_write > inode.size) {
        inode.size = write_pos + bytes_to_write;
    }

    entry->position = (entry->mode & MODE_APPEND) ? inode.size : (entry->position + bytes_to_write);
    inode.modified_time = time(NULL);
    inode.accessed_time = time(NULL);
    save_inode(&inode);

    free(block);
    return bytes_to_write;
}

/* Delete a file */
int deleteFile(const char* path) {
    uint32_t inode_num = find_inode_by_path(path);
    if (inode_num == (uint32_t)-1) {
        return -1;
    }

    Inode inode;
    if (load_inode(inode_num, &inode) < 0) {
        return -1;
    }

    if (inode.type != TYPE_FILE) {
        return -1;
    }

    /* Remove from parent directory */
    Inode parent;
    if (load_inode(inode.parent_inode, &parent) < 0) {
        return -1;
    }

    if (remove_directory_entry(inode.parent_inode, inode.name) < 0) {
        return -1;
    }

    /* Free data block */
    if (inode.data_block != 0) {
        free_block(inode.data_block);
    }

    /* Free inode */
    free_inode(inode_num);

    return 0;
}

/* Search for a file */
int searchFile(const char* path) {
    uint32_t inode_num = find_inode_by_path(path);
    return (inode_num != (uint32_t)-1) ? 0 : -1;
}

/* Make a directory */
int makeDirectory(const char* path) {
    return createFile(path, TYPE_DIRECTORY);
}

/* Remove a directory */
int removeDirectory(const char* path) {
    uint32_t inode_num = find_inode_by_path(path);
    if (inode_num == (uint32_t)-1) {
        return -1;
    }

    Inode inode;
    if (load_inode(inode_num, &inode) < 0) {
        return -1;
    }

    if (inode.type != TYPE_DIRECTORY) {
        return -1;
    }

    /* Check if directory is empty */
    if (!is_directory_empty(inode_num)) {
        return -1; /* Directory not empty */
    }

    /* Cannot remove root */
    Superblock* sb = get_superblock();
    if (!sb) {
        return -1;
    }
    
    if (inode_num == sb->root_inode) {
        return -1;
    }

    /* Remove from parent directory */
    if (remove_directory_entry(inode.parent_inode, inode.name) < 0) {
        return -1;
    }

    /* Free data block */
    if (inode.data_block != 0) {
        free_block(inode.data_block);
    }

    /* Free inode */
    free_inode(inode_num);

    return 0;
}

/* List directory contents */
int listDirectory(const char* path, char* output, uint32_t output_size) {
    uint32_t inode_num = find_inode_by_path(path);
    if (inode_num == (uint32_t)-1) {
        return -1;
    }

    Inode inode;
    if (load_inode(inode_num, &inode) < 0) {
        return -1;
    }

    if (inode.type != TYPE_DIRECTORY) {
        return -1;
    }

    DirectoryEntry entries[16];
    int count = read_directory_entries(inode_num, entries, 16);
    if (count < 0) {
        return -1;
    }

    output[0] = '\0';
    int written = 0;

    for (int i = 0; i < count; i++) {
        char line[MAX_FILENAME_LEN + 10];
        snprintf(line, sizeof(line), "%s %s\n", 
                entries[i].type == TYPE_DIRECTORY ? "DIR" : "FILE",
                entries[i].name);
        
        if (written + strlen(line) >= output_size - 1) {
            break;
        }
        
        strcat(output, line);
        written += strlen(line);
    }

    inode.accessed_time = time(NULL);
    save_inode(&inode);

    return count;
}

/* Search directory for pattern */
int searchDirectory(const char* path, const char* pattern) {
    uint32_t inode_num = find_inode_by_path(path);
    if (inode_num == (uint32_t)-1) {
        return -1;
    }

    Inode inode;
    if (load_inode(inode_num, &inode) < 0) {
        return -1;
    }

    if (inode.type != TYPE_DIRECTORY) {
        return -1;
    }

    DirectoryEntry entries[16];
    int count = read_directory_entries(inode_num, entries, 16);
    if (count < 0) {
        return -1;
    }

    for (int i = 0; i < count; i++) {
        if (strstr(entries[i].name, pattern) != NULL) {
            return 0; /* Found */
        }
    }

    return -1; /* Not found */
}

