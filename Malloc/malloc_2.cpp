#include <unistd.h>
#include <cstring>

const size_t MAX_ALLOC = 100000000;

struct MallocMetadata {
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
};

MallocMetadata* blocklist = NULL;

void* smalloc(size_t size) {
    if(size > MAX_ALLOC || size == 0) {
        return NULL;
    }
    // search for a free block
    MallocMetadata* current = blocklist;
    MallocMetadata* last = NULL;
    while (current) {
        if (current->is_free && current->size >= size) {
          current->is_free = false;
          return (void*)(current + 1);
        }
        last = current;
        current = current->next;
    }

    //not found - allocate a new block
    void* new_block_ptr = sbrk(size + sizeof(MallocMetadata));
    if (new_block_ptr == (void*)-1) return NULL;

    MallocMetadata* new_meta = (MallocMetadata*)new_block_ptr;
    new_meta->is_free = false;
    new_meta->size = size;
    new_meta->next = NULL;
    new_meta->prev = last;

    //add the block to the end of the list
    if (last) {
      last->next = new_meta;
    } else {
      blocklist = new_meta;
    }

    return (void*)(new_meta + 1);
}

void* scalloc(size_t num, size_t size){
    if (num == 0 || size == 0) {
        return NULL;
    }
    if (num > 0 && size > MAX_ALLOC / num) {
        return NULL;
    }
    void* ret = smalloc(num * size);
    if(ret != NULL) {
        std::memset(ret, 0, num * size);
    }
    return ret;
}

void sfree(void* p) {
    if(p == NULL) return;
    MallocMetadata* block_meta = (MallocMetadata*)p - 1;
    block_meta->is_free = true;
}

void* srealloc(void* p, size_t size) {
    if(p == NULL) {
        return smalloc(size);
    }

    MallocMetadata* old_meta = (MallocMetadata*)p - 1;
    if(size <= old_meta->size) {
        return p;
    }
    void* new_p = smalloc(size);
    if(new_p) {
        std::memmove(new_p, p, old_meta->size);
        sfree(p);
    }
    return new_p;
}

size_t _num_free_blocks() {
    size_t count = 0;
    MallocMetadata* current = blocklist;
    while (current) {
      if (current->is_free) {
        count++;
      }
      current = current->next;
    }
    return count;
}

size_t _num_free_bytes() {
  size_t total = 0;
  MallocMetadata* current = blocklist;
  while (current) {
    if (current->is_free) {
      total += current->size;
    }
    current = current->next;
  }
  return total;
}

size_t _num_allocated_blocks() {
    size_t count = 0;
    MallocMetadata* current = blocklist;
    while (current) {
      count++;
      current = current->next;
    }
    return count;
}

size_t _num_allocated_bytes() {
    size_t total = 0;
    MallocMetadata* current = blocklist;
    while (current) {
      total += current->size;
      current = current->next;
    }
    return total;
}

size_t _num_meta_data_bytes() {
  return _num_allocated_blocks() * sizeof(MallocMetadata);
}

size_t _size_meta_data() {
  return sizeof(MallocMetadata);
}
