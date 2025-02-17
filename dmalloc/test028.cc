#include "dmalloc.hh"
#include <cstdio>
#include <cassert>
#include <cstring>
// More boundary write error checks #3.

int main() {
    int* ptr = (int*) malloc(sizeof(int) * 10); // Oops, didn't allocate enough
    fprintf(stderr, "Will free %p\n", ptr);
    for (int i = 0; i < 13; ++i) {
        if (i >= 11) { // Write past end of allocation, but skip some bytes
          ptr[i] = i;
        }
    }
    free(ptr);
    print_statistics();
}

//! Will free ??{0x\w+}=ptr??
//! MEMORY BUG???: detected wild write during free of pointer ??ptr??
//! ???
