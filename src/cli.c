#include "../include/tinyfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

extern void init_open_file_table();
extern Superblock* get_superblock();


/* Print usage information */
void print_usage(const char* program_name) {
    printf("Usage: %s <command> [arguments]\n\n", program_name);
    printf("Commands:\n");
    printf("  init [num_blocks]               - Initialize a new file system\n");
    printf("  touch <file_path>              - Create a new file\n");
    printf("  mkdir <dir_path>               - Create a new directory\n");
    printf("  ls [dir_path]                  - List directory contents\n");
    printf("  rm <file_path>                 - Remove a file\n");
    printf("  rmdir <dir_path>               - Remove an empty directory\n");
    printf("  cat <file_path>                - Display file contents\n");
    printf("  write <file_path> <text>       - Write text to a file\n");
    printf("  search <path>                  - Search for a file/directory\n");
    printf("  shell                           - Start interactive shell (keeps RAM loaded)\n");
    printf("\n");
}

/* Initialize file system */
int cmd_init(int argc, char* argv[]) {
    uint32_t num_blocks = argc > 1 ? (uint32_t)atoi(argv[1]) : MAX_BLOCKS;

    if (num_blocks < 10 || num_blocks > MAX_BLOCKS) {
        fprintf(stderr, "Error: Number of blocks must be between 10 and %d\n", MAX_BLOCKS);
        return 1;
    }

    if (init_filesystem(num_blocks) < 0) {
        fprintf(stderr, "Error: Failed to initialize file system\n");
        return 1;
    }

    printf("File system initialized: %d blocks\n", num_blocks);
    
    return 0;
}

/* Touch command - create file */
int cmd_touch(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s touch <file_path>\n", argv[0]);
        return 1;
    }

    if (open_disk() < 0) {
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

    if (open_disk() < 0) {
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

    if (open_disk() < 0) {
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

    if (open_disk() < 0) {
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

    if (open_disk() < 0) {
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

    if (open_disk() < 0) {
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

    printf("\n");
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

    if (open_disk() < 0) {
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

    if (open_disk() < 0) {
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

/* Interactive shell mode - commands that don't open/close disk */
static int shell_touch(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: touch <file_path>\n");
        return 1;
    }
    /* Ensure superblock and inode table are loaded */
    if (load_superblock() < 0 || load_inode_table() < 0) {
        fprintf(stderr, "Error: Failed to load file system\n");
        return 1;
    }
    if (createFile(argv[1], TYPE_FILE) < 0) {
        fprintf(stderr, "Error: Failed to create file: %s\n", argv[1]);
        return 1;
    }
    /* Reload inode table to ensure we have the latest data */
    if (reload_inode_table() < 0) {
        fprintf(stderr, "Warning: Failed to reload inode table\n");
    }
    printf("File created: %s\n", argv[1]);
    return 0;
}

static int shell_mkdir(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: mkdir <dir_path>\n");
        return 1;
    }
    if (makeDirectory(argv[1]) < 0) {
        fprintf(stderr, "Error: Failed to create directory: %s\n", argv[1]);
        return 1;
    }
    printf("Directory created: %s\n", argv[1]);
    return 0;
}

static int shell_ls(int argc, char* argv[]) {
    const char* path = (argc >= 2) ? argv[1] : "/";
    char output[4096];
    if (listDirectory(path, output, sizeof(output)) < 0) {
        fprintf(stderr, "Error: Failed to list directory: %s\n", path);
        return 1;
    }
    printf("%s", output);
    return 0;
}

static int shell_rm(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: rm <file_path>\n");
        return 1;
    }
    if (deleteFile(argv[1]) < 0) {
        fprintf(stderr, "Error: Failed to delete file: %s\n", argv[1]);
        return 1;
    }
    printf("File deleted: %s\n", argv[1]);
    return 0;
}

static int shell_rmdir(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: rmdir <dir_path>\n");
        return 1;
    }
    if (removeDirectory(argv[1]) < 0) {
        fprintf(stderr, "Error: Failed to remove directory: %s\n", argv[1]);
        return 1;
    }
    printf("Directory removed: %s\n", argv[1]);
    return 0;
}

static int shell_cat(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: cat <file_path>\n");
        return 1;
    }
    int fd = openFile(argv[1], MODE_READ);
    if (fd < 0) {
        fprintf(stderr, "Error: Failed to open file: %s\n", argv[1]);
        return 1;
    }
    char buffer[BLOCK_SIZE];
    int bytes_read;
    while ((bytes_read = readFile(fd, buffer, BLOCK_SIZE)) > 0) {
        fwrite(buffer, 1, bytes_read, stdout);
    }
    printf("\n");
    closeFile(fd);
    return 0;
}

static int shell_write(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: write <file_path> <text>\n");
        return 1;
    }
    
    /* Remove quotes from text if present */
    char* text = argv[2];
    size_t text_len = strlen(text);
    if (text_len >= 2 && text[0] == '"' && text[text_len - 1] == '"') {
        text[text_len - 1] = '\0';
        text++;
    }
    
    if (searchFile(argv[1]) < 0) {
        if (createFile(argv[1], TYPE_FILE) < 0) {
            fprintf(stderr, "Error: Failed to create file: %s\n", argv[1]);
            return 1;
        }
    }
    
    int fd = openFile(argv[1], MODE_WRITE);
    if (fd < 0) {
        fprintf(stderr, "Error: Failed to open file: %s\n", argv[1]);
        return 1;
    }
    if (writeFile(fd, text, strlen(text)) < 0) {
        fprintf(stderr, "Error: Failed to write to file: %s\n", argv[1]);
        closeFile(fd);
        return 1;
    }
    closeFile(fd);
    printf("Text written to: %s\n", argv[1]);
    return 0;
}

static int shell_search(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: search <path>\n");
        return 1;
    }
    if (searchFile(argv[1]) == 0) {
        printf("Found: %s\n", argv[1]);
        return 0;
    } else {
        printf("Not found: %s\n", argv[1]);
        return 1;
    }
}

/* Interactive shell mode - RAM only, no disk persistence */
int cmd_shell(int argc, char* argv[]) {
    printf("TinyFS Interactive Shell (RAM-only mode)\n");
    printf("Type 'help' for commands, 'exit' to quit\n");
    printf("Note: All data is in RAM and will be lost on exit\n\n");
    
    /* Ensure open file table is initialized */
    init_open_file_table();
    
    bool filesystem_initialized = false;
    char line[512];
    
    while (1) {
        printf("tfs> ");
        if (fgets(line, sizeof(line), stdin) == NULL) {
            break; /* EOF */
        }
        
        /* Remove newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        
        if (strlen(line) == 0) {
            continue;
        }
        
        /* Parse command */
        char* tokens[32];
        int token_count = 0;
        char* token = strtok(line, " \t");
        while (token && token_count < 32) {
            tokens[token_count++] = token;
            token = strtok(NULL, " \t");
        }
        
        if (token_count == 0) continue;
        
        /* Handle commands */
        if (strcmp(tokens[0], "exit") == 0 || strcmp(tokens[0], "quit") == 0) {
            break;
        } else if (strcmp(tokens[0], "help") == 0) {
            printf("Commands:\n");
            printf("  init [num_blocks]  - Initialize file system in RAM (default: 512 blocks)\n");
            printf("  touch <file_path>  - Create a new file\n");
            printf("  mkdir <dir_path>   - Create a new directory\n");
            printf("  ls [dir_path]      - List directory contents\n");
            printf("  rm <file_path>     - Remove a file\n");
            printf("  rmdir <dir_path>   - Remove an empty directory\n");
            printf("  cat <file_path>    - Display file contents\n");
            printf("  write <file_path> <text> - Write text to a file\n");
            printf("  search <path>      - Search for a file/directory\n");
            printf("  exit/quit          - Exit shell (all data will be lost)\n");
        } else if (strcmp(tokens[0], "init") == 0) {
            uint32_t num_blocks = (token_count >= 2) ? (uint32_t)atoi(tokens[1]) : 512;
            if (num_blocks < 10 || num_blocks > MAX_BLOCKS) {
                fprintf(stderr, "Error: Number of blocks must be between 10 and %d\n", MAX_BLOCKS);
                continue;
            }
            if (init_filesystem(num_blocks) < 0) {
                fprintf(stderr, "Error: Failed to initialize file system\n");
                continue;
            }
            filesystem_initialized = true;
            printf("File system initialized in RAM: %d blocks\n", num_blocks);
        } else {
            /* All other commands require filesystem to be initialized */
            if (!filesystem_initialized) {
                fprintf(stderr, "Error: File system not initialized. Type 'init' first.\n");
                continue;
            }
            
            if (strcmp(tokens[0], "touch") == 0) {
                shell_touch(token_count, tokens);
            } else if (strcmp(tokens[0], "mkdir") == 0) {
                shell_mkdir(token_count, tokens);
            } else if (strcmp(tokens[0], "ls") == 0) {
                shell_ls(token_count, tokens);
            } else if (strcmp(tokens[0], "rm") == 0) {
                shell_rm(token_count, tokens);
            } else if (strcmp(tokens[0], "rmdir") == 0) {
                shell_rmdir(token_count, tokens);
            } else if (strcmp(tokens[0], "cat") == 0) {
                shell_cat(token_count, tokens);
            } else if (strcmp(tokens[0], "write") == 0) {
                shell_write(token_count, tokens);
            } else if (strcmp(tokens[0], "search") == 0) {
                shell_search(token_count, tokens);
            } else {
                printf("Unknown command: %s (type 'help' for commands)\n", tokens[0]);
            }
        }
    }
    
    /* Free memory - NO saving to disk */
    if (filesystem_initialized) {
        extern int free_disk(void);
        free_disk();
    }
    printf("\nGoodbye! All data has been erased.\n");
    return 0;
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
    } else if (strcmp(command, "shell") == 0 || strcmp(command, "interactive") == 0) {
        return cmd_shell(argc - 1, argv + 1);
    } else {
        fprintf(stderr, "Unknown command: %s\n\n", command);
        print_usage(argv[0]);
        return 1;
    }
}

