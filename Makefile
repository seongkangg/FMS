CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g
INCLUDES = -I./include
SRCDIR = src
OBJDIR = obj
BINDIR = bin

# Source files (exclude test files)
SOURCES = $(wildcard $(SRCDIR)/*.c)
SOURCES := $(filter-out $(SRCDIR)/test_%.c, $(SOURCES))
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
TARGET = $(BINDIR)/tfs
TEST_TARGET = $(BINDIR)/test_file_ops

# Default target
all: $(TARGET) $(TEST_TARGET)

# Create directories if they don't exist
$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

# Link object files to create executable
$(TARGET): $(OBJECTS) | $(BINDIR)
	$(CC) $(OBJECTS) -o $(TARGET)

# Test program (includes test_file_operations.c)
TEST_OBJECTS = $(filter-out $(OBJDIR)/cli.o, $(OBJECTS)) $(OBJDIR)/test_file_operations.o
$(TEST_TARGET): $(TEST_OBJECTS) | $(BINDIR)
	$(CC) $(TEST_OBJECTS) -o $(TEST_TARGET)

# Compile source files to object files
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Compile test files
$(OBJDIR)/test_%.o: $(SRCDIR)/test_%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean build artifacts
clean:
	rm -rf $(OBJDIR) $(BINDIR)
	rm -f *.disk

# Install (optional)
install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

# Uninstall (optional)
uninstall:
	rm -f /usr/local/bin/tfs

# Run test
test: $(TEST_TARGET)
	./$(TEST_TARGET)

.PHONY: all clean install uninstall test

