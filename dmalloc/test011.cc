#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "dmalloc.hh"
// heap_min and heap_max checking, no overlap with other regions.

static int global;

int main() {
    for (int i = 0; i != 100; ++i) {
        size_t sz = rand() % 100;
        char* p = (char*)malloc(sz);
        free(p);
    }
    dmalloc_stats stat;
    get_statistics(&stat);
    
    // Allocations must not overlap with global segment
    int* iptr = &global; // Address of a global
    assert((uintptr_t) iptr + sizeof(int) < stat.heap_min || (uintptr_t) iptr >= stat.heap_max);

    // Allocations must not overlap with stack segment
    dmalloc_stats* statptr = &stat; // Address of a variable on the stack
    assert((uintptr_t) statptr + sizeof(int) < stat.heap_min || (uintptr_t) statptr >= stat.heap_max);

    // Allocations must not overlap with code segment
    int (*mainptr)() = &main; // Address of main function (in the code segment)
    assert((uintptr_t) mainptr + sizeof(int) < stat.heap_min || (uintptr_t) mainptr >= stat.heap_max);
}
