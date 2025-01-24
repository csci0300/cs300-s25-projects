CC = gcc
FLAGS = -ggdb3 -Wall -Wextra -Wshadow -std=gnu11 -Wno-unused-parameter -Wno-unused-but-set-variable -Werror -fsigned-char

# Linking ncurses works differently on Linux and Mac. Detect
# OS to account for this
HOST_SYSTEM = $(shell uname | cut -f 1 -d_)
SYSTEM ?= $(HOST_SYSTEM)
ifeq ($(SYSTEM),Darwin)
LIBS = -lncurses
FLAGS += -D_XOPEN_SOURCE_EXTENDED
else
LIBS = $(shell ncursesw6-config --libs)
FLAGS += $(shell ncursesw6-config --cflags)
endif

FILES = $(wildcard src/*.c) $(wildcard src/*.h)
OBJS = src/game.o src/game_setup.o src/render.o src/common.o src/linked_list.o src/mbstrings.o
BINS = snake autograder

TEST_COUNT = 53
TESTS = $(shell seq 1 1 $(TEST_COUNT))

# How verbose should test output be? 0 gives default output, 1 gives
# each step of the board as the snake moves.
# Options are 0 or 1
#
# To choose one, you can edit the variable below, or specify its value on the
# command line.
#    $ make check -B V=1
#
V ?= 0
ifeq ($(V),1)
FLAGS += -DVERBOSE
endif

# Should address sanitizer be enabled? Default is 1.
# Options are 0 or 1.
# You should run with ASAN=0 when you are running under gdb.
#
# To choose one, you can edit the variable below, or specify its value on the
# command line.
#    $ make check -B ASAN=0
#
ASAN ?= 1
ifeq ($(ASAN),1)
FLAGS += -fsanitize=address
endif

# disable address sanitizer if we are targeting check-gdb-<n>
ifeq ($(findstring check-gdb,$(MAKECMDGOALS)),check-gdb)
FLAGS := $(filter-out -fsanitize=address, $(FLAGS))
endif

# shell commands to make a temp file that keeps track of whether the build was last run with ASAN enabled
# this will auto-handle ASAN changes.
# e.g. if previously the build was run with ASAN=1, and now ASAN=0, it will clean before building.
# .snake-build-status is a temp file that keeps track of whether the build was last run with ASAN enabled
$(shell \
  if [ -f .snake-build-status ]; then \
    read -r ASAN_STATUS < .snake-build-status; \
    if [ "$$ASAN_STATUS" != "ASAN=$(if $(findstring -fsanitize=address,$(FLAGS)),1,0)" ]; then \
      rm -f $(BINS) $(OBJS) .snake-build-status; \
    fi; \
  fi; \
  echo "ASAN=$(if $(findstring -fsanitize=address,$(FLAGS)),1,0)" > .snake-build-status; \
)

all: check-in-container $(BINS)

# wildcard rule for compiling object file from source and header
src/%.o: src/%.c src/%.h
	$(CC) $(FLAGS) -c $< -o $@

autograder: $(OBJS) test/autograder.c
	$(CC) $(FLAGS) $^ $(LIBS) -o $@ -lm

snake: $(OBJS) src/snake.c
	$(CC) $(FLAGS) $^ $(LIBS) -o $@ -lm

check: check-in-container autograder
	python3 test/autograder.py $(TESTS)

# this target supports running individual tests (for example, `check-3`)
# and ranges of tests (for example, `check-5-10`).
check-%: autograder
	python3 test/autograder.py $(shell echo -n "$(patsubst check-%, %, $@)" | awk -F '-' '{if (NF==2) print $$1 " 1 " $$2; else print $$1 " 1 " $$1}' | xargs seq)

# run a test under gdb
check-gdb-%: autograder
	sudo echo ""
	DEBUG=1 python3 test/autograder.py "$(patsubst check-gdb-%, %, $@)" & sleep 0.5 && sudo gdb -p `pgrep autograder`

format:
	clang-format -style=file -i $(FILES)

clean:
	rm -f $(BINS)
	rm -f ${OBJS}
	rm -f .snake-build-status

# target to check if you are in the container
check-in-container:
	@if [ ! -e /usr/bin/cs300-docker-version ]; then \
		echo "ERROR: NOT INSIDE COURSE CONTAINER"; \
		exit 1; \
	fi

.PHONY: all clean format echo check check-in-container

