#include "../include/tinyfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Demonstration of file operations using RAM-based disk simulation */
int main() {
    /* Initialize the open file table */
    init_open_file_table();
    
    printf("=== TinyFS RAM-based File System Simulation ===\n\n");

    /* Step 1: Initialize file system in RAM */
    printf("1. Initializing file system in RAM...\n");
    if (init_filesystem("ram_disk", 512) < 0) {
        fprintf(stderr, "Error: Failed to initialize file system\n");
        return 1;
    }
    printf("   ✓ File system initialized in RAM (512 blocks)\n\n");

    /* Step 2: Create a file */
    printf("2. Creating file '/test.txt'...\n");
    if (createFile("/test.txt", TYPE_FILE) < 0) {
        fprintf(stderr, "Error: Failed to create file\n");
        close_disk();
        return 1;
    }
    printf("   ✓ File created\n\n");

    /* Step 3: Open the file for writing */
    printf("3. Opening file '/test.txt' for writing...\n");
    int fd = openFile("/test.txt", MODE_WRITE);
    if (fd < 0) {
        fprintf(stderr, "Error: Failed to open file for writing\n");
        close_disk();
        return 1;
    }
    printf("   ✓ File opened (fd=%d)\n\n", fd);

    /* Step 4: Write data to the file */
    printf("4. Writing data to file...\n");
    const char* test_data = "Hello, TinyFS! This is a test of the RAM-based file system.";
    int bytes_written = writeFile(fd, test_data, strlen(test_data));
    if (bytes_written < 0) {
        fprintf(stderr, "Error: Failed to write to file\n");
        closeFile(fd);
        close_disk();
        return 1;
    }
    printf("   ✓ Written %d bytes: \"%s\"\n\n", bytes_written, test_data);

    /* Step 5: Close the file */
    printf("5. Closing file...\n");
    if (closeFile(fd) < 0) {
        fprintf(stderr, "Error: Failed to close file\n");
        close_disk();
        return 1;
    }
    printf("   ✓ File closed\n\n");

    /* Step 6: Open the file again for reading */
    printf("6. Opening file '/test.txt' again for reading...\n");
    fd = openFile("/test.txt", MODE_READ);
    if (fd < 0) {
        fprintf(stderr, "Error: Failed to open file for reading\n");
        close_disk();
        return 1;
    }
    printf("   ✓ File reopened (fd=%d)\n\n", fd);

    /* Step 7: Read data from the file */
    printf("7. Reading data from file...\n");
    char read_buffer[256];
    memset(read_buffer, 0, sizeof(read_buffer));
    int bytes_read = readFile(fd, read_buffer, sizeof(read_buffer) - 1);
    if (bytes_read < 0) {
        fprintf(stderr, "Error: Failed to read from file\n");
        closeFile(fd);
        close_disk();
        return 1;
    }
    printf("   ✓ Read %d bytes: \"%s\"\n\n", bytes_read, read_buffer);

    /* Step 8: Verify the data matches */
    printf("8. Verifying data integrity...\n");
    if (strcmp(test_data, read_buffer) == 0) {
        printf("   ✓ Data matches perfectly!\n\n");
    } else {
        printf("   ✗ Data mismatch!\n");
        printf("     Expected: \"%s\"\n", test_data);
        printf("     Got:      \"%s\"\n\n", read_buffer);
        closeFile(fd);
        close_disk();
        return 1;
    }

    /* Step 9: Close the file again */
    printf("9. Closing file again...\n");
    if (closeFile(fd) < 0) {
        fprintf(stderr, "Error: Failed to close file\n");
        close_disk();
        return 1;
    }
    printf("   ✓ File closed\n\n");

    /* Step 10: Demonstrate that data persists in RAM */
    printf("10. Verifying data persistence (reopen and read again)...\n");
    fd = openFile("/test.txt", MODE_READ);
    if (fd < 0) {
        fprintf(stderr, "Error: Failed to reopen file\n");
        close_disk();
        return 1;
    }
    
    memset(read_buffer, 0, sizeof(read_buffer));
    bytes_read = readFile(fd, read_buffer, sizeof(read_buffer) - 1);
    if (bytes_read >= 0 && strcmp(test_data, read_buffer) == 0) {
        printf("   ✓ Data persists correctly in RAM!\n\n");
    } else {
        printf("   ✗ Data was not persisted correctly\n\n");
        closeFile(fd);
        close_disk();
        return 1;
    }
    
    closeFile(fd);
    close_disk();

    printf("=== Test completed successfully! ===\n");
    printf("The file system is using RAM to simulate hard disk storage.\n");
    printf("All file operations (open, write, close, reopen, read) work correctly.\n");

    return 0;
}

