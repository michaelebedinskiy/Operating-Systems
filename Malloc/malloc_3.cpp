#include <unistd.h>
#include <cstring>
#include <sys/mman.h>
#include <cstdint>

const size_t MAX_ALLOC = 100000000;
const int MAX_ORDER = 10;
const size_t MIN_BLOCK_SIZE_BYTES = 128;
const size_t INITIAL_ARENA_BLOCKS = 32;
const size_t MMAP_THRESHOLD = 128 * 1024;
const size_t ARENA_SIZE = INITIAL_ARENA_BLOCKS * MMAP_THRESHOLD;


struct MallocMetadata {
    size_t size;
    bool is_free;
    bool is_mmaped;
    int order;
    MallocMetadata* next;
    MallocMetadata* prev;
};

static bool g_is_initialized = false;
static void* g_heap_start = nullptr;
static size_t g_buddy_used_block_count = 0;
static MallocMetadata* g_free_lists[MAX_ORDER + 1] = {nullptr};
static MallocMetadata* g_mmap_list_head = nullptr;

void addToFreeList(MallocMetadata* block);

void initialize_allocator() {
    if (g_is_initialized) return;
    void* current_brk = sbrk(0);
    uintptr_t aligned_addr = ((uintptr_t)current_brk + (ARENA_SIZE - 1)) & ~(ARENA_SIZE - 1);
    size_t alignment_increment = aligned_addr - (uintptr_t)current_brk;
    if (sbrk(alignment_increment) == (void*)-1) {
        return;
    }
    g_heap_start = sbrk(ARENA_SIZE);
    if (g_heap_start != (void*)aligned_addr || g_heap_start == (void*)-1) {
        g_heap_start = nullptr;
        return;
    }

  for (size_t i = 0; i < INITIAL_ARENA_BLOCKS; ++i) {
    MallocMetadata* meta = (MallocMetadata*)((uintptr_t)g_heap_start + i * MMAP_THRESHOLD);
    meta->size = MMAP_THRESHOLD;
    meta->order = MAX_ORDER;
    meta->is_mmaped = false;
    meta->next = nullptr;
    meta->prev = nullptr;
    addToFreeList(meta);
  }
  g_is_initialized = true;
}

void removeFromFreeList(MallocMetadata* block) {
  if (!block || !block->is_free) return;
  if (block->prev) {
    block->prev->next = block->next;
  } else {
      g_free_lists[block->order] = block->next;
  }
  if (block->next) {
    block->next->prev = block->prev;
  }
  block->next = nullptr;
  block->prev = nullptr;
}

void addToFreeList(MallocMetadata* block) {
  if (!block) return;
  block->is_free = true;
  int order = block->order;
  MallocMetadata* current = g_free_lists[order];
  if (!current || block < current) {
    block->next = current;
    block->prev = nullptr;
    if (current) {
        current->prev = block;
    }
    g_free_lists[order] = block;
    return;
  }

    while (current->next && current->next < block) {
        current = current->next;
    }

    block->next = current->next;
    block->prev = current;
    if (current->next) {
        current->next->prev = block;
    }
    current->next = block;
}

MallocMetadata* getBuddy(MallocMetadata* block) {
  uintptr_t block_addr = (uintptr_t)block;
  uintptr_t buddy_addr = block_addr ^ block->size; //XOR trick
  return (MallocMetadata*)buddy_addr;
}

void* mmap_alloc(size_t size) {
  size_t total_size = size + sizeof(MallocMetadata);
  void* block = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (block == MAP_FAILED) {
    return NULL;
  }
  MallocMetadata* meta = (MallocMetadata*)block;
  meta->size = total_size;
  meta->is_free = false;
  meta->is_mmaped = true;
  meta->order = -1; //not part of buddy system

  //add to the front of mmap'd list
  meta->next = g_mmap_list_head;
  meta->prev = nullptr;
  if (g_mmap_list_head) {
    g_mmap_list_head->prev = meta;
  }
  g_mmap_list_head = meta;

  return (void*)(meta + 1);
}

void* smalloc(size_t size) {
    if(size == 0 || size > MAX_ALLOC) {
        return NULL;
    }
    initialize_allocator();

    //challenge 3
    if (size + sizeof(MallocMetadata) >= MMAP_THRESHOLD) {
      return mmap_alloc(size);
    }

    //challenge 0
    size_t required_total_size = size + sizeof(MallocMetadata);
    int required_order = 0;
    //calculate order
    size_t current_block_size = MIN_BLOCK_SIZE_BYTES;
    while (current_block_size < required_total_size) {
        current_block_size <<= 1;
        required_order++;
    }
    if (required_order > MAX_ORDER) return NULL;

    //find the smallest large enough available block
    int order_to_use = -1;
    for (int i = required_order; i <= MAX_ORDER; ++i) {
      if (g_free_lists[i]) {
        order_to_use = i;
        break;
      }
    }
    if (order_to_use < 0) return NULL; //out of memory

    MallocMetadata* block_to_alloc = g_free_lists[order_to_use];
    removeFromFreeList(block_to_alloc);
    g_buddy_used_block_count++;

    //challenge 1
    while (block_to_alloc->order > required_order) {
      block_to_alloc->order--;
      block_to_alloc->size /= 2;
      MallocMetadata* buddy = getBuddy(block_to_alloc);
      buddy->size = block_to_alloc->size;
      buddy->order = block_to_alloc->order;
      buddy->is_mmaped = false;
      buddy->next = nullptr;
      buddy->prev = nullptr;
      buddy->is_free = true;
      addToFreeList(buddy);
    }

    block_to_alloc->is_free = false;
    block_to_alloc->next  = nullptr;
    block_to_alloc->prev  = nullptr;

    return (void*)(block_to_alloc + 1);
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
    if (!p) return;
    MallocMetadata* block_to_free = (MallocMetadata*)p - 1;

    //challenge 3
    if (block_to_free->is_mmaped) {
      if (block_to_free->prev) block_to_free->prev->next = block_to_free->next;
      if (block_to_free->next) block_to_free->next->prev = block_to_free->prev;
      if (g_mmap_list_head == block_to_free) g_mmap_list_head = block_to_free->next;
      munmap(block_to_free, block_to_free->size);
      return;
    }
    g_buddy_used_block_count--;
    //challenge 2
    while (block_to_free->order < MAX_ORDER) {
      MallocMetadata* buddy = getBuddy(block_to_free);
      if (!buddy->is_free || buddy->order != block_to_free->order) break;
      removeFromFreeList(buddy);
      if ((uintptr_t)buddy < (uintptr_t)block_to_free) block_to_free = buddy;
      block_to_free->order++;
      block_to_free->size *= 2;
    }

    addToFreeList(block_to_free);
}

void* srealloc(void* p, size_t size) {
    if(p == NULL) {
        return smalloc(size);
    }
    if (size == 0 || size > MAX_ALLOC) return NULL;

    MallocMetadata* old_meta = (MallocMetadata*)p - 1;
    size_t user_space = old_meta->is_mmaped ? old_meta->size - sizeof(MallocMetadata) : (MIN_BLOCK_SIZE_BYTES << old_meta->order) - sizeof(MallocMetadata);
    if (size <= user_space) {
        return p;
    }

    void* new_p = smalloc(size);
    if (!new_p) return NULL;
    std::memmove(new_p, p, user_space);
    sfree(p);
    return new_p;
}

size_t _num_free_blocks() {
    size_t count = 0;
    for (int i = 0; i <= MAX_ORDER; ++i) {
        for (MallocMetadata* current = g_free_lists[i]; current; current = current->next) {
            count++;
        }
    }
    return count;
}

size_t _num_free_bytes() {
    size_t total_bytes = 0;
    for (int i = 0; i <= MAX_ORDER; ++i) {
        for (MallocMetadata* current = g_free_lists[i]; current; current = current->next) {
            total_bytes += (current->size - sizeof(MallocMetadata));
        }
    }
    return total_bytes;
}

size_t _num_allocated_blocks() {
    if (!g_is_initialized) return 0;

    size_t count = _num_free_blocks() + g_buddy_used_block_count;

    for (MallocMetadata* current = g_mmap_list_head; current; current = current->next) {
        count++;
    }
    return count;
}

size_t _num_allocated_bytes() {
    if (!g_is_initialized) return 0;

    size_t total_bytes = _num_free_bytes();

    size_t total_buddy_blocks = _num_free_blocks() + g_buddy_used_block_count;
    size_t total_buddy_metadata = total_buddy_blocks * sizeof(MallocMetadata);
    total_bytes = ARENA_SIZE - total_buddy_metadata;

    for (MallocMetadata* current = g_mmap_list_head; current; current = current->next) {
        total_bytes += (current->size - sizeof(MallocMetadata));
    }
    return total_bytes;
}

size_t _num_meta_data_bytes() {
  return _num_allocated_blocks() * sizeof(MallocMetadata);
}

size_t _size_meta_data() {
  return sizeof(MallocMetadata);
}
