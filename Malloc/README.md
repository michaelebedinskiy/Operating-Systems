# Operating Systems – Homework Exercise 4 🖥️

## Overview
This project implements **Homework Exercise 4** for the Operating Systems course (234123). The focus is on creating a **custom memory allocation library** in C++, replacing the standard `malloc()`, `free()`, `calloc()`, and `realloc()` functions. The library handles dynamic memory manually using `sbrk()` and, for large allocations, `mmap()`.

## Assignment Goal
The main goal is to provide a fully functional memory management unit that:

- Allocates and frees memory blocks of varying sizes.
- Minimizes fragmentation and efficiently reuses freed memory.
- Supports zero-initialized allocations (`scalloc`) and resizing of blocks (`srealloc`).
- Maintains metadata for each block to track size, usage, and pointers for linked-list management.
- Provides statistics on heap usage and metadata overhead.

## Implementation Details
The project is divided into three main parts:

### Part 1 – Naïve Malloc
- File: `malloc_1.cpp`
- Implements `smalloc(size_t size)` to allocate memory using `sbrk()`.
- No support for freeing or zero-initialized allocation.
- Returns `NULL` for invalid sizes or if memory cannot be allocated.

### Part 2 – Basic Malloc
- File: `malloc_2.cpp`  
- Adds metadata to manage allocated blocks.

Implements:
- `smalloc(size_t size)` – allocates memory, reusing freed blocks if possible.  
- `scalloc(size_t num, size_t size)` – allocates zero-initialized memory.  
- `sfree(void* p)` – releases a block.  
- `srealloc(void* oldp, size_t size)` – resizes a block, copying data as needed.  

Statistics functions:
- `_num_free_blocks()`, `_num_free_bytes()`  
- `_num_allocated_blocks()`, `_num_allocated_bytes()`  
- `_num_meta_data_bytes()`, `_size_meta_data()`  

### Part 3 – Better Malloc
File: `malloc_3.cpp`  

Implements a buddy memory allocator to reduce fragmentation:  
- Divides memory into blocks of power-of-2 sizes.  
- Splits and merges buddy blocks dynamically.  
- Uses `mmap()` for large allocations (≥128 KB).  
- Improves memory utilization and ensures efficient allocation of free blocks.  
- Statistics functions updated to accurately reflect heap and metadata usage.  
