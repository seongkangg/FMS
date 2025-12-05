#include "../include/tinyfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

extern void init_open_file_table();
extern Superblock* get_superblock();

/* Get the disk name to use - try last initialized disk, or use default */
static const char* get_disk_name(void) {
    static char disk_name[256] = "ram_disk";
    static bool loaded = false;
    
    if (!loaded) {
        FILE* f = fopen(".last_disk", "r");
        if (f) {
            if (fgets(disk_name, sizeof(disk_name), f) != NULL) {
                size_t len = strlen(disk_name);
                if (len > 0 && disk_name[len - 1] == '\n') {
                    disk_name[len - 1] = '\0';
                }
            }
            fclose(f);
        }
        loaded = true;
    }
    
    return disk_name;
}

/* Print usage information */
void print_usage(const char* program_name) {
    printf("Usage: %s <command> [arguments]\n\n", program_name);
    printf("Commands:\n");
    printf("  init <disk_name> [num_blocks]  - Initialize a new file system\n");
    printf("  touch <file_path>              - Create a new file\n");
    printf("  mkdir <dir_path>               - Create a new directory\n");
    printf("  ls [dir_path]                  - List directory contents\n");
    printf("  rm <file_path>                 - Remove a file\n");
    printf("  rmdir <dir_path>               - Remove an empty directory\n");
    printf("  cat <file_path>                - Display file contents\n");
    printf("  write <file_path> <text>       - Write text to a file\n");
    printf("  search <path>                  - Search for a file/directory\n");
    printf("\n");
}

/* Initialize file system */
int cmd_init(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s init <disk_name> [num_blocks]\n", argv[0]);
        return 1;
    }

    const char* disk_name = argv[1];
    uint32_t num_blocks = argc > 2 ? (uint32_t)atoi(argv[2]) : MAX_BLOCKS;

    if (num_blocks < 10 || num_blocks > MAX_BLOCKS) {
        fprintf(stderr, "Error: Number of blocks must be between 10 and %d\n", MAX_BLOCKS);
        return 1;
    }

    if (init_filesystem(disk_name, num_blocks) < 0) {
        fprintf(stderr, "Error: Failed to initialize file system\n");
        return 1;
    }

    printf("File system initialized: %s (%d blocks)\n", disk_name, num_blocks);
    
    close_disk();  /* This saves the RAM disk to file (and saves disk name to .last_disk) */
    return 0;
}

/* Touch command - create file */
int cmd_touch(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s touch <file_path>\n", argv[0]);
        return 1;
    }

    if (open_disk(get_disk_name()) < 0) {
        fprintf(stderr, "Error: Failed to open disk. Run 'init' first.\n");
        return 1;
    }

    if (createFile(argv[1], TYPE_FILE) < 0) {
        fprintf(stderr, "Error: Failed to create file: %s\n", argv[1]);
        close_disk();
        return 1;
    }

    printf("File created: %s\n", argv[1]);
    
    /* Ensure all changes are saved to disk before closing */
    if (save_superblock() < 0) {
        fprintf(stderr, "Warning: Failed to save superblock\n");
    }
    
    close_disk();  /* This saves the RAM disk to file */
    return 0;
}

/* Mkdir command - create directory */
int cmd_mkdir(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s mkdir <dir_path>\n", argv[0]);
        return 1;
    }

    if (open_disk(get_disk_name()) < 0) {
        fprintf(stderr, "Error: Failed to open disk. Run 'init' first.\n");
        return 1;
    }

    if (makeDirectory(argv[1]) < 0) {
        fprintf(stderr, "Error: Failed to create directory: %s\n", argv[1]);
        close_disk();
        return 1;
    }

    printf("Directory created: %s\n", argv[1]);
    close_disk();
    return 0;
}

/* Ls command - list directory */
int cmd_ls(int argc, char* argv[]) {
    const char* path = (argc >= 2) ? argv[1] : "/";

    if (open_disk(get_disk_name()) < 0) {
        fprintf(stderr, "Error: Failed to open disk. Run 'init' first.\n");
        return 1;
    }

    char output[4096];
    if (listDirectory(path, output, sizeof(output)) < 0) {
        fprintf(stderr, "Error: Failed to list directory: %s\n", path);
        close_disk();
        return 1;
    }

    printf("%s", output);
    close_disk();
    return 0;
}

/* Rm command - remove file */
int cmd_rm(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s rm <file_path>\n", argv[0]);
        return 1;
    }

    if (open_disk(get_disk_name()) < 0) {
        fprintf(stderr, "Error: Failed to open disk. Run 'init' first.\n");
        return 1;
    }

    if (deleteFile(argv[1]) < 0) {
        fprintf(stderr, "Error: Failed to delete file: %s\n", argv[1]);
        close_disk();
        return 1;
    }

    printf("File deleted: %s\n", argv[1]);
    close_disk();
    return 0;
}

/* Rmdir command - remove directory */
int cmd_rmdir(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s rmdir <dir_path>\n", argv[0]);
        return 1;
    }

    if (open_disk(get_disk_name()) < 0) {
        fprintf(stderr, "Error: Failed to open disk. Run 'init' first.\n");
        return 1;
    }

    if (removeDirectory(argv[1]) < 0) {
        fprintf(stderr, "Error: Failed to remove directory: %s\n", argv[1]);
        close_disk();
        return 1;
    }

    printf("Directory removed: %s\n", argv[1]);
    close_disk();
    return 0;
}

/* Cat command - display file contents */
int cmd_cat(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s cat <file_path>\n", argv[0]);
        return 1;
    }

    if (open_disk(get_disk_name()) < 0) {
        fprintf(stderr, "Error: Failed to open disk. Run 'init' first.\n");
        return 1;
    }

    int fd = openFile(argv[1], MODE_READ);
    if (fd < 0) {
        fprintf(stderr, "Error: Failed to open file: %s\n", argv[1]);
        close_disk();
        return 1;
    }

    char buffer[BLOCK_SIZE];
    int bytes_read;
    while ((bytes_read = readFile(fd, buffer, BLOCK_SIZE)) > 0) {
        fwrite(buffer, 1, bytes_read, stdout);
    }

    closeFile(fd);
    close_disk();
    return 0;
}

/* Write command - write text to file */
int cmd_write(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s write <file_path> <text>\n", argv[0]);
        return 1;
    }

    if (open_disk(get_disk_name()) < 0) {
        fprintf(stderr, "Error: Failed to open disk. Run 'init' first.\n");
        return 1;
    }

    /* Create file if it doesn't exist */
    if (searchFile(argv[1]) < 0) {
        if (createFile(argv[1], TYPE_FILE) < 0) {
            fprintf(stderr, "Error: Failed to create file: %s\n", argv[1]);
            close_disk();
            return 1;
        }
    }

    int fd = openFile(argv[1], MODE_WRITE);
    if (fd < 0) {
        fprintf(stderr, "Error: Failed to open file: %s\n", argv[1]);
        close_disk();
        return 1;
    }

    if (writeFile(fd, argv[2], strlen(argv[2])) < 0) {
        fprintf(stderr, "Error: Failed to write to file: %s\n", argv[1]);
        closeFile(fd);
        close_disk();
        return 1;
    }

    closeFile(fd);
    close_disk();
    printf("Text written to: %s\n", argv[1]);
    return 0;
}

/* Search command - search for file/directory */
int cmd_search(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s search <path>\n", argv[0]);
        return 1;
    }

    if (open_disk(get_disk_name()) < 0) {
        fprintf(stderr, "Error: Failed to open disk. Run 'init' first.\n");
        return 1;
    }

    if (searchFile(argv[1]) == 0) {
        printf("Found: %s\n", argv[1]);
        close_disk();
        return 0;
    } else {
        printf("Not found: %s\n", argv[1]);
        close_disk();
        return 1;
    }
}

/* Main function */
int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    init_open_file_table();

    const char* command = argv[1];

    if (strcmp(command, "init") == 0) {
        return cmd_init(argc - 1, argv + 1);
    } else if (strcmp(command, "touch") == 0) {
        return cmd_touch(argc - 1, argv + 1);
    } else if (strcmp(command, "mkdir") == 0) {
        return cmd_mkdir(argc - 1, argv + 1);
    } else if (strcmp(command, "ls") == 0) {
        return cmd_ls(argc - 1, argv + 1);
    } else if (strcmp(command, "rm") == 0) {
        return cmd_rm(argc - 1, argv + 1);
    } else if (strcmp(command, "rmdir") == 0) {
        return cmd_rmdir(argc - 1, argv + 1);
    } else if (strcmp(command, "cat") == 0) {
        return cmd_cat(argc - 1, argv + 1);
    } else if (strcmp(command, "write") == 0) {
        return cmd_write(argc - 1, argv + 1);
    } else if (strcmp(command, "search") == 0) {
        return cmd_search(argc - 1, argv + 1);
    } else {
        fprintf(stderr, "Unknown command: %s\n\n", command);
        print_usage(argv[0]);
        return 1;
    }
}

