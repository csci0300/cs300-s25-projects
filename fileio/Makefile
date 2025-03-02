# CC stands for C Compiler.
CC           := gcc

# Should sanitizers be enabled? Default is 1.
# Options are 0 or 1.
# You should run with SAN=0 when you are running under gdb or strace.
#
# To choose one, you can edit the variable below, or specify its value on the
# command line.
#    $ make check -B SAN=0
#
SAN ?= 1
ifeq ($(SAN),1)
SANS_ENABLED ?= -fsanitize=address -fsanitize=undefined
endif

# Flags to be passed to the C compiler.
# -ggdb3 says to embed as much debugging information as possible
# -Wall -Wextra says to enable all standard, and some extra warnings
# -Wshadow explicitly makes shadowing a local variable a warning
# -Werror changes all warnings to compilation errors
# -std=gnu11 says which version of the C language our source program is written in
# -fsigned-char specifies that `char`s are signed by default
CFLAGS       += -ggdb3 -Wall -Wextra -Wshadow -Werror -std=gnu11 -fsigned-char $(SANS_ENABLED) 

# These programs use your IO library. We will run them to make sure your code is
# working correctly
TEST_PROGRAMS := io300_test byte_cat diabolical_byte_cat reverse_byte_cat block_cat reverse_block_cat random_block_cat stride_cat rot13

# Which implementation of io300_file do we want to use with our test programs?
# Options are student | naive | stdio
#
# To choose one, you can edit the variable below, or specify its value on the
# command line.
#    $ make -B IMPL=stdio
#
IMPL ?= student

ifeq ($(IMPL), student)
	STUDENT_FLAGS = -Dfread=DO_NOT_USE_STDIO_fread -Dfwrite=DO_NOT_USE_STDIO_fwrite -Dfputc=DO_NOT_USE_STDIO_fputc -Dfgetc=DO_NOT_USE_STDIO_fgetc
	STUDENT_FLAGS = -Dfputs=DO_NOT_USE_STDIO_fputs -Dfgets=DO_NOT_USE_STDIO_fgets
endif

all: $(TEST_PROGRAMS) impl.o

impl.o: impl/$(IMPL).c
	$(CC) $(CFLAGS) $(STUDENT_FLAGS) $^ -c -o $@

test_helpers.o: test_programs/test_helpers.c
	$(CC) $(CFLAGS) $(STUDENT_FLAGS) $^ -c -o $@

$(TEST_PROGRAMS): %: test_programs/%.c impl.o test_helpers.o
	$(CC) $(CFLAGS) $^ -o $@

# What random seed do we want to use? Default is -1 (random)
# Value can be any unsigned int, in base 10 (full range: 0-4294967295) 
#
# To choose one, you can edit the variable below, or specify its value on the
# command line.
#    $ make -B SEED=487
#
SEED ?= -1

# This (optionally) passes the random seed defined by SEED to the Python testing script.
RANDARG := 

ifneq ($(SEED), -1)
	RANDARG = --seed $(SEED)
endif
check: $(TEST_PROGRAMS)
	./test_scripts/correctness_test.py $(RANDARG) all

check-basic: $(TEST_PROGRAMS)
	./test_scripts/correctness_test.py $(RANDARG) basic

check-e2e: $(TEST_PROGRAMS)
	./test_scripts/correctness_test.py $(RANDARG) e2e


perf:
	./test_scripts/performance_test.py

clean:
	rm -f -- $(TEST_PROGRAMS) impl.o

check_testdata:
	dd if=/dev/urandom of=/tmp/check_testdata bs=4096 count=20
	@echo "80 KiB test file generated in /tmp/check_testdata"

perf_testdata:
	dd if=/dev/urandom of=/tmp/perf_testdata bs=1M count=1
	@echo "1 MiB test file generated in /tmp/perf_testdata"

.PHONY: all clean perf check check_testdata perf_testdata
