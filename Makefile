# Makefile for ADB Tool
# Requires MinGW-w64 with gcc and windres

# Compiler and flags
CC = gcc
WINDRES = windres
CFLAGS = -Wall -O2 -DUNICODE -D_UNICODE -Iinclude
LDFLAGS = -mconsole -luser32 -lkernel32 -lshell32 -lole32

# Directories
SRC_DIR = src
INC_DIR = include
RES_DIR = resources
BUILD_DIR = build

# Source files
SOURCES = $(SRC_DIR)/main.c \
          $(SRC_DIR)/adb_wrapper.c \
          $(SRC_DIR)/fastboot_wrapper.c \
          $(SRC_DIR)/device_manager.c \
          $(SRC_DIR)/file_transfer.c \
          $(SRC_DIR)/fastboot_manager.c \
          $(SRC_DIR)/resource_extractor.c \
          $(SRC_DIR)/cli.c \
          $(SRC_DIR)/utils.c

# Object files
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Resource file
RESOURCE_OBJECT = $(BUILD_DIR)/resources.o

# Output
OUTPUT = $(BUILD_DIR)/adbtool.exe

# Default target
all: $(OUTPUT)

# Link
$(OUTPUT): $(OBJECTS) $(RESOURCE_OBJECT)
	@echo "Linking $@..."
	$(CC) $(OBJECTS) $(RESOURCE_OBJECT) -o $@ $(LDFLAGS)
	@echo "Build complete: $(OUTPUT)"
	@echo "Size: `du -h $(OUTPUT) | cut -f1`"

# Compile C files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Compile resource
$(RESOURCE_OBJECT): $(RES_DIR)/resources.rc
	@mkdir -p $(BUILD_DIR)
	@echo "Compiling resources..."
	$(WINDRES) $< $@

# Clean
clean:
	@echo "Cleaning build directory..."
	rm -rf $(BUILD_DIR)
	@echo "Clean complete."

# Run
run: $(OUTPUT)
	@echo "Running $(OUTPUT)..."
	$(OUTPUT)

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: clean $(OUTPUT)

# Help
help:
	@echo "Available targets:"
	@echo "  all     - Build the project (default)"
	@echo "  clean   - Remove build artifacts"
	@echo "  run     - Build and run the tool"
	@echo "  debug   - Build with debug symbols"
	@echo "  help    - Show this help message"

.PHONY: all clean run debug help
