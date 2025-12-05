# TinyFS File Management System

A complete file management system implementation based on the TinyFS architecture. This system provides a layered approach to file management with block-based storage, inode management, and a command-line interface.

## Architecture

The TinyFS file management system is organized into five main layers:

1. **Storage Manager** (`src/storage_manager.c`)
   - Handles low-level input/output operations
   - **Uses RAM to simulate hard disk storage** - all blocks are stored in memory
   - Reads and writes fixed-size data blocks (256 bytes) from/to RAM
   - Manages virtual storage device interactions (simulated in memory)

2. **Allocator** (`src/allocator.c`)
   - Manages allocation of storage space
   - Maintains a free block bitmap to track used/free blocks
   - Provides block allocation and deallocation functions

3. **Metadata Manager** (`src/metadata_manager.c`)
   - Manages file system metadata:
     - **Superblock**: File system information (total blocks, block size, inode table location, etc.)
     - **Inodes**: File Control Blocks storing file/directory metadata
     - **Directory Entries**: Maps file/directory names to inode numbers
     - **Open File Table**: Tracks currently open files with descriptors, modes, and positions

4. **API Layer** (`src/api_layer.c`)
   - Provides high-level file and directory operations
   - Implements system calls and library functions
   - Handles path resolution and file operations

5. **CLI Tool** (`src/cli.c`)
   - Command-line interface for user interaction
   - Implements commands: `tfs touch`, `tfs ls`, `tfs rm`, etc.

## Core Data Structures

### Superblock
Stores file system metadata:
- Magic number for file system identification
- Total number of blocks and block size
- Location of inode table, bitmap, and data area
- Root directory inode number
- File system creation timestamp

### Inode (File Control Block)
Represents each file or directory:
- Inode number and type (file or directory)
- File/directory name
- Size in bytes
- Starting data block number
- Parent directory inode
- Creation, modification, and access timestamps

### Directory Entry
Maps names to inodes:
- File or directory name
- Corresponding inode number
- Entry type (file or directory)

### Free Block Bitmap
Array of bits where each bit represents a block:
- 0 = free block
- 1 = used block

### Open File Table
Tracks open files:
- File descriptor
- Inode number
- Current read/write position
- Access mode (read, write, append)

## Building the Project

### Prerequisites
- GCC compiler
- Make utility
- Unix-like environment (Linux, macOS, or WSL on Windows)

### Build Commands

```bash
# Build the project
make

# Clean build artifacts
make clean

# Install to system (optional)
make install
```

The executable will be created in the `bin/` directory as `tfs`.

## RAM-Based Disk Simulation

**Important**: This file system uses **RAM to simulate hard disk storage**. All file system blocks are stored in memory rather than on actual disk files. This allows for:
- Fast file operations
- Complete simulation of disk I/O behavior
- Data persistence during the program's lifetime
- Easy testing and demonstration

### File Operation Workflow

The system supports the complete file operation workflow:
1. **Open a file** - `openFile(path, mode)`
2. **Write to the file** - `writeFile(fd, buffer, size)`
3. **Close the file** - `closeFile(fd)`
4. **Open the file again** - `openFile(path, mode)`
5. **Read the content** - `readFile(fd, buffer, size)`

All operations use RAM-based block storage, simulating how a real file system would interact with a hard disk.

## Usage

### Initializing a File System

```bash
./bin/tfs init <disk_name> [num_blocks]
```

Creates a new file system image. Default is 1024 blocks if not specified.

Example:
```bash
./bin/tfs init tinyfs.disk 512
```

### Available Commands

#### File Operations

- **touch** - Create a new file
  ```bash
  ./bin/tfs touch /path/to/file.txt
  ```

- **cat** - Display file contents
  ```bash
  ./bin/tfs cat /path/to/file.txt
  ```

- **write** - Write text to a file
  ```bash
  ./bin/tfs write /path/to/file.txt "Hello, World!"
  ```

- **rm** - Delete a file
  ```bash
  ./bin/tfs rm /path/to/file.txt
  ```

- **search** - Search for a file or directory
  ```bash
  ./bin/tfs search /path/to/file.txt
  ```

#### Directory Operations

- **mkdir** - Create a directory
  ```bash
  ./bin/tfs mkdir /path/to/directory
  ```

- **ls** - List directory contents
  ```bash
  ./bin/tfs ls /path/to/directory
  ./bin/tfs ls          # Lists root directory
  ```

- **rmdir** - Remove an empty directory
  ```bash
  ./bin/tfs rmdir /path/to/directory
  ```

## File System Specifications

- **Block Size**: 256 bytes
- **Maximum Blocks**: 1024 (configurable)
- **Maximum Inodes**: 128
- **Maximum Filename Length**: 32 characters
- **Maximum Open Files**: 64
- **Maximum Path Length**: 256 characters

## File System Layout

```
Block 0:              Superblock
Blocks 1-N:           Free Block Bitmap
Blocks N+1-M:         Inode Table
Blocks M+1-end:       Data Blocks
```

## API Functions

### File Operations
- `int createFile(const char* path, uint8_t type)`
- `int openFile(const char* path, uint8_t mode)`
- `int closeFile(int fd)`
- `int readFile(int fd, void* buffer, uint32_t size)`
- `int writeFile(int fd, const void* buffer, uint32_t size)`
- `int deleteFile(const char* path)`
- `int searchFile(const char* path)`

### Directory Operations
- `int makeDirectory(const char* path)`
- `int removeDirectory(const char* path)`
- `int listDirectory(const char* path, char* output, uint32_t output_size)`
- `int searchDirectory(const char* path, const char* pattern)`

### Access Modes
- `MODE_READ` - Read-only access
- `MODE_WRITE` - Write access (overwrites)
- `MODE_APPEND` - Append access

## Error Handling

Functions return:
- `0` on success
- `-1` on error (check `errno` for details on system calls)

Common errors:
- File not found
- Directory not empty (for rmdir)
- Out of disk space
- Too many open files
- Invalid path

## Limitations

1. Single-block files: Files are limited to one block (256 bytes) of data
2. Flat directory structure: Directories can hold limited entries per block
3. No symbolic links or hard links
4. No file permissions or ownership
5. Simple allocation: No fragmentation handling beyond single blocks

## Testing

A test program is included to demonstrate the RAM-based file operations:

```bash
# Build the test program
make

# Run the test (demonstrates open → write → close → reopen → read)
./bin/test_file_ops
```

The test program verifies that:
- Files can be opened, written to, and closed
- Files can be reopened and read correctly
- Data persists correctly in RAM-based storage

## Development

### Project Structure
```
FMS/
├── include/
│   └── tinyfs.h          # Header file with all definitions
├── src/
│   ├── storage_manager.c # Low-level block I/O
│   ├── allocator.c       # Block allocation
│   ├── metadata_manager.c # Metadata management
│   ├── api_layer.c       # High-level API
│   └── cli.c             # Command-line interface
├── obj/                  # Object files (generated)
├── bin/                  # Executable (generated)
├── Makefile              # Build configuration
└── README.md             # This file
```

### Extending the System

To add new features:
1. Add function prototypes to `include/tinyfs.h`
2. Implement functions in appropriate layer
3. Add CLI commands in `src/cli.c` if needed
4. Update this README

## License

This project is provided as-is for educational purposes.

