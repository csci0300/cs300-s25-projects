#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../io300.h"

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MAX_BLOCK_SIZE 30

typedef int (*perform_func)(struct io300_file* file, size_t* loc_ptr,
                            unsigned char* contents);

void usage(char* argv[]);

int interpret_size(char* str, size_t* ptr, char* type, bool allow_zero);

int generate_file(const char* path, size_t size, unsigned char* buffer);
int fill_buf_from_user(const char* path, size_t size, unsigned char* buffer);
int test_operations(struct io300_file* file, unsigned char* file_contents);

int perform_readc(struct io300_file* file, size_t* loc_ptr,
                  unsigned char* contents);
int perform_writec(struct io300_file* file, size_t* loc_ptr,
                   unsigned char* contents);
int perform_read(struct io300_file* file, size_t* loc_ptr,
                 unsigned char* contents);
int perform_write(struct io300_file* file, size_t* loc_ptr,
                  unsigned char* contents);

int verify_contents(const char* path, unsigned char* contents, size_t size);

int write_contents(const char* path, size_t size, unsigned char* buffer);

void cleanup(unsigned char* contents);