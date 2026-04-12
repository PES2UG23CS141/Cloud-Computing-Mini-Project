# Compiler and Flags
CC = gcc
CFLAGS = -Wall -g `pkg-config fuse3 --cflags`
LIBS = `pkg-config fuse3 --libs`

# Project Structure
SRC_DIR = src
SRC = $(SRC_DIR)/main.c $(SRC_DIR)/operations.c $(SRC_DIR)/path_utils.c $(SRC_DIR)/cow_logic.c
OBJ = $(SRC:.c=.o)
TARGET = mini_unionfs

# Default Target
all: $(TARGET)

# Link the binary
$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LIBS)

# Compile source files into object files
# Depends on the header file to ensure re-compilation if headers change
%.o: %.c $(SRC_DIR)/mini_unionfs.h
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build artifacts
clean:
	rm -f $(SRC_DIR)/*.o $(TARGET)

# Phony targets
.PHONY: all clean
