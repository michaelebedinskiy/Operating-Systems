#include <unistd.h>
#define MAX_ALLOC 100000000

void* smalloc(size_t size) {
  if (size == 0 || size > MAX_ALLOC) return NULL;
  void* block = sbrk(size);
  if (block == (void*)-1) return NULL;
  return block;
}
