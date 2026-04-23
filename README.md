# Mini-UnionFS

A lightweight implementation of a Union Filesystem using FUSE (Filesystem in Userspace) in C. This project demonstrates the core concepts of layered filesystems with copy-on-write semantics.

## Project Overview

Union Filesystems (UnionFS) allow you to overlay multiple directories, creating a unified view. Mini-UnionFS implements a two-layer model:

- **Lower Layer (Read-Only)**: A base directory that serves as the read-only foundation
- **Upper Layer (Read-Write)**: An overlay directory for write operations and modifications
- **Mount Point**: Where the unified filesystem is exposed to the user

### Key Features

1. **Layered Directory Structure**: Access files from both layers transparently
2. **Copy-on-Write (CoW)**: Files from the lower layer are copied to the upper layer before modification
3. **Whiteout Mechanism**: Files deleted from the mounted filesystem are tracked via whiteout files (`.wh.filename`)
4. **Path Resolution**: Intelligent path resolution that checks upper layer first, then falls back to lower layer
5. **File Visibility**: Files in the upper layer take precedence over the lower layer

## Project Structure

```
mini-unionfs/
├── Makefile              # Build configuration
├── README.md             # This file
├── test_unionfs.sh       # Test suite
├── mini_unionfs          # Compiled binary (after build)
└── src/
    ├── main.c            # Entry point and FUSE initialization
    ├── mini_unionfs.h    # Header file with data structures
    ├── operations.c      # FUSE filesystem operations (read, write, delete, etc.)
    ├── path_utils.c      # Path resolution logic between layers
    └── cow_logic.c       # Copy-on-write file copying logic
```

## How It Works

### Architecture

```
User Application
      ↓
Mount Point (/mnt/unionfs)
      ↓
FUSE (Filesystem in Userspace)
      ↓
Mini-UnionFS
      ├─→ Path Resolution
      ├─→ Copy-on-Write Logic
      └─→ Upper & Lower Directories
```

### Critical Components

**1. Path Resolution (`path_utils.c`)**
- Checks if a file is whited-out (deleted) in the upper layer
- Returns the path from the upper layer if it exists
- Falls back to the lower layer if not in upper
- Returns ENOENT if the file doesn't exist in either layer

**2. Copy-on-Write (`cow_logic.c`)**
- When modifying a file from the lower layer, it's first copied to the upper layer
- Preserves file permissions and metadata
- Uses buffered I/O (8KB chunks) for efficiency

**3. Filesystem Operations (`operations.c`)**
- `getattr`: Get file attributes (stat)
- `readdir`: List directory contents from both layers
- `open/read/write`: File operations with CoW
- `unlink`: Delete files using whiteout mechanism
- `mkdir/create`: Create directories and files

**4. Whiteout Mechanism**
- When a file from the lower layer is deleted, a special file `.wh.filename` is created in the upper layer
- This marks the file as "deleted" without actually removing it from the lower layer
- Ensures consistency across remounts

## Dependencies

- **FUSE 3.x**: Filesystem in Userspace library
- **UTHash**: C hashing library
- **GCC**: C compiler
- **pkg-config**: For build configuration

### Installation (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install fuse3 libfuse3-dev gcc make pkg-config uthash-dev
```

## Building the Project

Clone or navigate to the project directory:

```bash
cd mini-unionfs
```

Compile the project:

```bash
make
```

This creates the `mini_unionfs` binary in the project root.

To clean build artifacts:

```bash
make clean
```

## Running Mini-UnionFS

### Basic Usage

```bash
./mini_unionfs <lower_dir> <upper_dir> <mount_point> [fuse_options]
```

**Arguments:**
- `<lower_dir>`: Path to the read-only base layer directory
- `<upper_dir>`: Path to the read-write overlay directory (can be empty or existing)
- `<mount_point>`: Where to mount the union filesystem
- `[fuse_options]`: Optional FUSE flags (e.g., `-d` for debug, `-s` for single-threaded)

### Example

```bash
# Setup test directories
mkdir -p lower_layer upper_layer mount_point

# Create a file in the lower layer
echo "Original content" > lower_layer/file.txt

# Mount the union filesystem
./mini_unionfs lower_layer upper_layer mount_point

# Access the file through the mount point
cat mount_point/file.txt

# Modify the file (triggers Copy-on-Write)
echo "Modified content" >> mount_point/file.txt

# The original file in lower_layer remains unchanged
cat lower_layer/file.txt
cat upper_layer/file.txt  # Modified copy is here

# Delete a file (creates a whiteout)
rm mount_point/file.txt
ls -la upper_layer/  # Shows .wh.file.txt

# Unmount the filesystem
fusermount -u mount_point
# or on macOS:
# umount mount_point
```

## Testing

The project includes a comprehensive test suite: `test_unionfs.sh`

Run all tests:

```bash
chmod +x test_unionfs.sh
./test_unionfs.sh
```

### Test Cases

The test suite verifies:

1. **Layer Visibility** - Files from the lower layer are visible through the mount point
2. **Copy-on-Write** - Modifying a lower-layer file creates a copy in the upper layer without affecting the original
3. **Whiteout Mechanism** - Deleted files are properly marked with whiteout files and don't reappear

### Expected Output

```
Starting Mini-UnionFS Test Suite...
Test 1: Layer Visibility... PASSED
Test 2: Copy-on-Write... PASSED
Test 3: Whiteout mechanism... PASSED
Test Suite Completed.
```

## Troubleshooting

### "FUSE: Device not found"
- FUSE may not be installed or not loaded. Install FUSE 3.x
- On Linux, ensure the fuse kernel module is loaded: `sudo modprobe fuse`

### "Permission denied" when mounting
- You might need to add your user to the fuse group:
  ```bash
  sudo usermod -a -G fuse $USER
  newgrp fuse
  ```
- Or use sudo: `sudo ./mini_unionfs ...`

### "Mount point not empty"
- The mount_point directory must be empty. Create a new one or use an existing empty directory.

### Test script fails
- Ensure the `mini_unionfs` binary is compiled successfully
- Check that FUSE is properly installed
- Run tests with `sudo` if you encounter permission issues with fusermount

## Use Cases

- **Container Images**: Docker uses similar mechanisms for layered images
- **Live USB Systems**: Allowing writes to read-only ISO images
- **Versioning Systems**: Creating snapshots and overlays
- **Development Environments**: Overlaying temporary changes on base systems

## Technical Concepts Demonstrated

- FUSE API and custom filesystem implementation
- Copy-on-Write semantics
- Layer merging and path resolution
- Filesystem metadata handling
- Hash tables for whiteout tracking (using uthash)
