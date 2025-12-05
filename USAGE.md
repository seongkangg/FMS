# TinyFS Usage Guide

## Quick Start

### 1. Build the Project

```bash
make
```

This creates two executables:
- `bin/tfs` - The command-line interface tool
- `bin/test_file_ops` - Test program demonstrating RAM-based file operations

### 2. Initialize a File System

First, you need to initialize a file system in RAM:

```bash
./bin/tfs init ram_disk 512
```

This creates a RAM-based file system with 512 blocks (the name "ram_disk" is kept for compatibility but all storage is in RAM).

### 3. Basic File Operations

#### Create a file
```bash
./bin/tfs touch /test.txt
```

#### Write content to a file
```bash
./bin/tfs write /test.txt "Hello, TinyFS!"
```

#### Read file content
```bash
./bin/tfs cat /test.txt
```

#### List directory contents
```bash
./bin/tfs ls /
./bin/tfs ls /subdirectory
```

#### Delete a file
```bash
./bin/tfs rm /test.txt
```

### 4. Directory Operations

#### Create a directory
```bash
./bin/tfs mkdir /documents
./bin/tfs mkdir /documents/subfolder
```

#### Remove an empty directory
```bash
./bin/tfs rmdir /documents/subfolder
```

#### Search for a file/directory
```bash
./bin/tfs search /test.txt
```

## Complete Example Workflow

Here's a complete example demonstrating the file operation workflow:

```bash
# 1. Initialize the file system
./bin/tfs init ram_disk 512

# 2. Create a file
./bin/tfs touch /hello.txt

# 3. Write to the file
./bin/tfs write /hello.txt "This is test data stored in RAM!"

# 4. Read from the file
./bin/tfs cat /hello.txt

# 5. Create a directory
./bin/tfs mkdir /mydir

# 6. Create a file in the directory
./bin/tfs touch /mydir/file.txt

# 7. Write to the file in the directory
./bin/tfs write /mydir/file.txt "Nested file content"

# 8. List root directory
./bin/tfs ls /

# 9. List the subdirectory
./bin/tfs ls /mydir

# 10. Read the nested file
./bin/tfs cat /mydir/file.txt

# 11. Clean up
./bin/tfs rm /mydir/file.txt
./bin/tfs rmdir /mydir
./bin/tfs rm /hello.txt
```

## Testing the RAM-Based File Operations

Run the automated test to see the complete workflow in action:

```bash
# Build (if not already built)
make

# Run the test program
./bin/test_file_ops
# or
make test
```

The test program demonstrates:
1. Initializing the file system in RAM
2. Creating a file
3. Opening the file for writing
4. Writing data to it
5. Closing the file
6. Reopening the file for reading
7. Reading the data back
8. Verifying data integrity

## Programmatic Usage (C API)

If you want to use the file system in your own C program, here's an example:

```c
#include "include/tinyfs.h"
#include <stdio.h>

int main() {
    // Initialize open file table
    init_open_file_table();
    
    // Initialize file system in RAM
    if (init_filesystem("ram_disk", 512) < 0) {
        fprintf(stderr, "Failed to initialize file system\n");
        return 1;
    }
    
    // Create a file
    if (createFile("/example.txt", TYPE_FILE) < 0) {
        fprintf(stderr, "Failed to create file\n");
        return 1;
    }
    
    // Open file for writing
    int fd = openFile("/example.txt", MODE_WRITE);
    if (fd < 0) {
        fprintf(stderr, "Failed to open file\n");
        return 1;
    }
    
    // Write data
    const char* data = "Hello from C API!";
    writeFile(fd, data, strlen(data));
    
    // Close file
    closeFile(fd);
    
    // Reopen for reading
    fd = openFile("/example.txt", MODE_READ);
    
    // Read data
    char buffer[256];
    int bytes = readFile(fd, buffer, sizeof(buffer) - 1);
    buffer[bytes] = '\0';
    printf("Read: %s\n", buffer);
    
    // Close and cleanup
    closeFile(fd);
    close_disk();
    
    return 0;
}
```

## Important Notes

1. **RAM-Based Storage**: All file system data is stored in RAM, not on disk. When the program exits, all data is lost.

2. **File System Initialization**: You must run `init` before using any other commands. Each `init` creates a fresh file system.

3. **Path Format**: Always use absolute paths starting with `/`:
   - ✅ `/file.txt`
   - ✅ `/dir/subdir/file.txt`
   - ❌ `file.txt` (relative paths not supported)

4. **File Size Limit**: Files are limited to 256 bytes (one block) in the current implementation.

5. **Directory Limitations**: Directories can hold a limited number of entries (depends on block size and directory entry size).

## Troubleshooting

**Error: "Failed to open disk. Run 'init' first."**
- Solution: Run `./bin/tfs init ram_disk 512` first

**Error: "Failed to create file"**
- Possible causes:
  - File already exists
  - Invalid path
  - Out of disk space (all blocks used)
  - Parent directory doesn't exist

**Error: "Directory not empty" (when using rmdir)**
- Solution: Remove all files and subdirectories first, then remove the directory

**Error: "No free blocks"**
- Solution: Increase the number of blocks when initializing: `./bin/tfs init ram_disk 1024`

## Available Commands Summary

| Command | Description | Example |
|---------|-------------|---------|
| `init <name> [blocks]` | Initialize file system | `init ram_disk 512` |
| `touch <path>` | Create a file | `touch /file.txt` |
| `mkdir <path>` | Create a directory | `mkdir /mydir` |
| `ls [path]` | List directory | `ls /` or `ls /mydir` |
| `cat <path>` | Display file content | `cat /file.txt` |
| `write <path> <text>` | Write text to file | `write /file.txt "Hello"` |
| `rm <path>` | Delete a file | `rm /file.txt` |
| `rmdir <path>` | Remove directory | `rmdir /mydir` |
| `search <path>` | Search for file/dir | `search /file.txt` |

