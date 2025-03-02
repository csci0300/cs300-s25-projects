#include "io300_test.h"

#include <bits/getopt_ext.h>
#include <sys/stat.h>

/* Name of the file tested on. */
const char* INPUT_FILENAME = "/tmp/io300_input";

/* Name of file with expected output. */
const char* EXPECTED_FILENAME = "/tmp/io300_expected";

/* Result file (for debugging). */
const char* RESULT_FILENAME = "/tmp/io300_output";

/* Docker container hostname length (for setting seed) */
const int HOSTNAME_LENGTH = 12;

const char POISON_BYTE = 0xbe;

/* The starting size of the generated file. */
size_t file_size = 8192;

/* The maximum size of the generated file. */
size_t max_size = 16384;

/* The number of operations performed on the generated file. */
size_t num_ops = 1000;

/* Whether or not `io300_seek` should be performed. */
int seeking = 0;

/* The functions that perform/test the chosen operations to test. */
perform_func operations[4] = {NULL, NULL, NULL, NULL};

/* The number of operations to test (i.e. the number of non-`NULL` elements in `operations) */
int num_test_ops = 0;

/*
   The size of the buffer used for `io300_read`, or 0 for a random size
   for each iteration.
*/
size_t read_size = 0;

/*
   The size of the buffer used for `io300_write`, or 0 for a random size
   for each iteration.
*/
size_t write_size = 0;

/*
   Nonzero if cleanup should be skipped (for debugging)
 */
int disable_cleanup = 0;

/*
  If there is at least one test with a write
*/
int writing = 0;

/*
  Flag to determine if the tester is using a randomly generated file.
*/
char random_file_mode = 0;

/*
  Pointer to user-provided file
*/
FILE* user_provided_input = NULL;

/*
 * Tests io300 file I/O implementations by creating a randomly generated file
 * (`io300_input`) of a given size, performing a given number of random file I/O
 * operations on it, of the given options, and verifying that those operations
 * were performed correctly.
 *
 * Takes in the following command line arguments:
 *  - A list of functions to test - any combination of "readc", "writec", "read", "write", or "seek".
 *    For "read" or "write", by default a randomly sized buffer of size 1 to `MAX_BLOCK_SIZE` will be
 *    used for each iteration. To specify a fixed size, one can format the argument instead as
 *    "read=<size>" or "write=<size>".
 *  - Optionally, any of the following:
 *     - "-s <size>" to specify the size in bytes of the generated file (defaults to 8192).
 *     - "-m <size>" to specify the maximum size of the file - no writes or seeks will be
 *       performed beyond this boundary (defaults to 16384).
 *     - "-n <ops>" to specify the number of operations performed, not counting seeks (defaults
 *       to 1000).
 *	   - "-i <filename>" to specify the input file: if not provided, defaults to a randomly generated file.
 *     - "--seed <seed>" to specify a custom seed
 *	   - "--no-cleanup" to keep the input and output file around after testing
 *	- Flags "-s" and "-m" are mutually exclusive with "-i".
 *	   - Will exit with an error message if the user uses mutually exclusive flags.
 */
int main(int argc, char* argv[]) {
    if (argc == 1) {
	usage(argv);
	exit(1);
    }

    // Process command arguments
    int opt;
    // Use student's container hostname as default seed
    size_t seed;
    char hostname[HOSTNAME_LENGTH];
    FILE* hostname_fp = fopen("/etc/hostname", "r");
    if (hostname_fp != NULL) {
	fread(hostname, HOSTNAME_LENGTH, 1, hostname_fp);
	// Parses hostname as hex number. Grading server will always overwrite this value with custom random seed.
	seed = strtoul(hostname, NULL, 16);
    } else {
	fprintf(stderr, "Hostname file not found, using random seed instead\n");
	seed = time(NULL);
    }

    static struct option long_options[] = {
	{"file-size", required_argument, NULL, 's'},
	{"max-size", required_argument, NULL, 'm'},
	{"num-operations", required_argument, NULL, 'n'},
	{"seed", required_argument, NULL, 'r'},
	{"input-file", required_argument, NULL, 'i'},
	{"no-cleanup", no_argument, &disable_cleanup, 1},
	{"debug", no_argument, &disable_cleanup, 1},
    };

    while ((opt = getopt_long(argc, argv, "s:m:n:r:i:", long_options, NULL)) !=
	   -1) {
	switch (opt) {
	    case 0:
	    case 1:
		// Unused (for disable cleanup)
		break;
	    case 's':
		random_file_mode = 1;
		if (interpret_size(optarg, &file_size, "file size", true) < 0) {
		    exit(1);
		}
		break;
	    case 'm':
		random_file_mode = 1;
		if (interpret_size(optarg, &max_size, "maximum file size",
				   false) < 0) {
		    exit(1);
		}
		break;
	    case 'n':
		if (interpret_size(optarg, &num_ops, "number of operations",
				   false) < 0) {
		    exit(1);
		}
		break;
	    case 'r':
		if (interpret_size(optarg, &seed, "random seed", false) < 0) {
		    exit(1);
		}
		break;
	    case 'i':
		user_provided_input = fopen(optarg, "r");
		if (!user_provided_input) {
		    perror("fopen");
		    exit(1);
		}
		break;
	    default: /* '?' */
		fprintf(stderr, "Unrecognized option %c\n", opt);
		usage(argv);
		exit(1);
	}
    }

    // Check if user inputted mutually exclusive flags
    if (random_file_mode && user_provided_input) {
	fprintf(stderr,
		"Error:  you cannot use the -i option alongside either -s or -m.\n"
		"If you want to test with a known file, remove the -s and -m options"
		" and use -i instead.\n");
	exit(1);
    }

    /* Seed random number generator */
    // fprintf(stderr, "Using %ld as random seed\n", seed);
    srand(seed);

    if (max_size < file_size) {
	fprintf(stderr,
		"Error: maximum file size should be at least the starting file "
		"size\n");
	exit(1);
    }

    if (optind == argc) {
	fprintf(stderr, "Error: need to specify operations to test\n");
	exit(1);
    }

    for (int i = optind; i < argc; i++) {
	if (!strcmp(argv[i], "seek")) {
	    seeking = 1;
	} else if (!strcmp(argv[i], "readc")) {
	    operations[num_test_ops] = perform_readc;
	    num_test_ops++;
	} else if (!strcmp(argv[i], "writec")) {
	    operations[num_test_ops] = perform_writec;
	    num_test_ops++;
	    writing = 1;
	} else if (!strcmp(argv[i], "read")) {
	    operations[num_test_ops] = perform_read;
	    num_test_ops++;
	} else if (!strncmp(argv[i], "read=", 5)) {
	    if (interpret_size(&argv[i][5], &read_size, "reading buffer size",
			       false) < 0)
		exit(1);
	    if (read_size > MAX_BLOCK_SIZE) {
		fprintf(stderr,
			"Error: specified buffer size for `io300_read` was "
			"larger than maximum (%d)\n",
			MAX_BLOCK_SIZE);
		exit(1);
	    }
	    operations[num_test_ops] = perform_read;
	    num_test_ops++;
	} else if (!strcmp(argv[i], "write")) {
	    operations[num_test_ops] = perform_write;
	    num_test_ops++;
	    writing = 1;
	} else if (!strncmp(argv[i], "write=", 6)) {
	    if (interpret_size(&argv[i][6], &write_size, "writing buffer size",
			       false) < 0) {
		exit(1);
	    }
	    if (read_size > MAX_BLOCK_SIZE) {
		fprintf(stderr,
			"Error: specified buffer size for `io300_write` was "
			"larger than maximum (%d)\n",
			MAX_BLOCK_SIZE);
		exit(1);
	    }
	    writing = 1;
	    operations[num_test_ops] = perform_write;
	    num_test_ops++;
	} else {
	    fprintf(stderr, "Error: invalid command line argument \"%s\"\n",
		    argv[i]);
	    exit(1);
	}
    }

    if (num_test_ops == 0) {
	fprintf(stderr, "Error: no specified file operations to test\n");
	usage(argv);
	exit(1);
    }

    /* File contents generation */
    // If the user has provided a file, get the size parameters from the provided file.
    // Set max_size to double the input file's size, in accordance with default max_size being 2x default file_size.
    if (user_provided_input) {
	struct stat file_data;
	fstat(fileno(user_provided_input), &file_data);
	file_size = file_data.st_size;
	max_size = file_size * 2;
    }

    unsigned char file_contents[max_size];

    // If provided, generate the file from the user-provided input.
    if (user_provided_input) {
	fill_buf_from_user(RESULT_FILENAME, file_size, file_contents);
    } else {
	// Generate a random file to run the tests
	if (generate_file(RESULT_FILENAME, file_size, file_contents) < 0) {
	    exit(1);
	}
    }

    // Clear the file contents between file_size and max_size
    for (size_t i = file_size; i < max_size; i++) file_contents[i] = 0;

    // Write input data to file (for debugging)
    if (disable_cleanup) {
	write_contents(INPUT_FILENAME, file_size, file_contents);
    }

    // Perform operations
    struct io300_file* out = io300_open(RESULT_FILENAME, "\e[0;32mout\e[0m");
    if (out == NULL) {
	fprintf(stderr, "io300_open: error opening file");
	cleanup(file_contents);
	exit(1);
    }

    if (test_operations(out, file_contents) < 0) {
	cleanup(file_contents);
	exit(1);
    }

    if (io300_close(out) != 0) {
	fprintf(stderr, "io300_close: error closing file");
	cleanup(file_contents);
	exit(1);
    }

    // Testing correctness
    if (verify_contents(RESULT_FILENAME, file_contents, file_size) < 0) {
	cleanup(file_contents);
	exit(1);
    }

    cleanup(file_contents);
    return 0;
}

/*
 * Prints a message detailing how to use this program, for the given list of command
 * line arguments `argv`.
 */
void usage(char* argv[]) {
    fprintf(stderr,
	    "Usage: %s <commands> [-s <file-size>] [-m <max-file-size>] [-n "
	    "<num-ops>] [-i <input-filename>] [--seed <random seed>] "
	    "[--no-cleanup] \n",
	    argv[0]);
    fprintf(stderr, "Arguments:\n");
    fprintf(stderr, "  --input-file     (-i)  Use this file as input, instead of generating randomly\n");
    fprintf(stderr, "  --seed           (-r)  Set random seed\n");
    fprintf(stderr, "  --file-size      (-s)  Size of random test file\n");
    fprintf(stderr, "  --max-size       (-m)  Maximum test file size\n");
    fprintf(stderr, "  --num-operations (-n)  Number of operations of <commands> to use\n");
    fprintf(stderr, "  --no-cleanup           Don't delete output files after test (for debugging)\n");
    fprintf(stderr, "  --debug                Same as --no-cleanup\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Commands:\n");
    fprintf(stderr, "  readc          : Test `io300_readc`\n");
    fprintf(stderr, "  writec         : Test `io300_writec`\n");
    fprintf(stderr,
	    "  read[=<size>]  : Test `io300_read`, with optionally a fixed "
	    "given buffer size\n");
    fprintf(stderr,
	    "  write[=<size>] : Test `io300_write`, with optionally a fixed "
	    "given buffer size\n");
}

/*
 * For a given valid string, interprets the string as a `size_t` and stores
 * the value in `size_ptr`, with a description of the size's purpose/type contained
 * in the string `type`.
 * On success, returns 0; on failure, prints an appropriate error message and returns -1,
 * with the contents of `size_ptr` unchanged.
 */
int interpret_size(char* str, size_t* size_ptr, char* type, bool allow_zero) {
    char* endptr;
    long const size = strtol(str, &endptr, 10);
    if (*endptr != '\0') {
	fprintf(stderr, "Error: invalid %s \"%s\" provided.\n", type, str);
	return -1;
    }
    if (size < 0 || (size == 0 && !allow_zero)) {
	fprintf(stderr,
		"Error: invalid %s \"%s\" provided, must be positive value.\n",
		type, str);
	return -1;
    }

    *size_ptr = (size_t)size;
    return 0;
}

/*
 * Generates a file at the given file path composed of random bytes
 * of the given size, and fills the given buffer with said bytes.
 * (Assumes the `buffer` is at least `size` bytes long)
 *
 * Returns 0 on success; on failure prints an appropriate error message
 * and returns -1.
 */
int generate_file(const char* path, size_t size, unsigned char* buffer) {
    FILE* out = fopen(path, "w");
    if (out == NULL) {
	perror("fopen");
	return -1;
    }

    for (size_t i = 0; i < size; i++) {
	buffer[i] = rand() % UCHAR_MAX;
	if (fputc(buffer[i], out) == EOF) {
	    perror("fputc");
	    return -1;
	}
    }

    if (fclose(out) != 0) {
	perror("fclose");
	return -1;
    }

    return 0;
}

/*
 * Generates a file at the given file path composed of the bytes from the user-provided
 * input file (global variable), and fills the given buffer with those bytes.
 * (Assumes the `buffer` is at least `size` bytes long)
 *
 * Returns 0 on success; on failure prints an appropriate error message
 * and returns -1.
 */
int fill_buf_from_user(const char* path, size_t size, unsigned char* buffer) {
    FILE* out = fopen(path, "w");
    if (out == NULL) {
	perror("fopen");
	return -1;
    }

    // Copy over the entire file into the buffer, then into the specified output file.
    fread(buffer, size, 1, user_provided_input);
    fwrite(buffer, size, 1, out);

    if (fclose(out) != 0) {
	perror("fclose");
	return -1;
    }

    return 0;
}

/*
 * Writes contents buffer to file
 *
 * Returns 0 on success; on failure prints an appropriate error message
 * and returns -1.
 */
int write_contents(const char* path, size_t size, unsigned char* buffer) {
    FILE* out = fopen(path, "w");
    if (out == NULL) {
	perror("fopen");
	return -1;
    }

    for (size_t i = 0; i < size; i++) {
	if (fputc(buffer[i], out) == EOF) {
	    perror("fputc");
	    return -1;
	}
    }

    if (fclose(out) != 0) {
	perror("fclose");
	return -1;
    }

    return 0;
}

/*
 * Tests the operations as specified by the command line arguments - on
 * the given file, performs the specified operations (stored in `operations`)
 * `num_ops` times, with buffer sizes given by `read_size` and `write_size`.
 * Writes and seeks are not performed beyond `max_size` and if `seeking` is set,
 * then all operations are preceded by a call to `io300_seek`.
 * Any operations are also replicated on the contents of `file_contents`, with
 * size `max_size` and starts with the contents of the opened `file`.
 *
 * Returns 0 if the verification succeeds; otherwise prints an appropriate
 * error message and returns -1.
 */
int test_operations(struct io300_file* file, unsigned char* contents) {
    size_t loc = 0;
    for (size_t i = 0; i < num_ops; i++) {
	// seek if needed
	if (seeking) {
	    loc = rand() % max_size;
	    if (io300_seek(file, loc) < 0) {
		fprintf(stderr, "Error: `io300_seek` failed\n");
		return -1;
	    }
	}

	// randomly choose an operation to test
	if (operations[rand() % num_test_ops](file, &loc, contents) < 0)
	    return -1;
    }
    return 0;
}

/*
 * Performs/tests `io300_readc` using the given opened file, using/updating a pointer to
 * the location of file read/write head `loc_ptr` and the current contents of the
 * file.
 *
 * Returns 0 if the test succeeds, and on failure prints an appropriate error message
 * and returns -1.
 */
int perform_readc(struct io300_file* file, size_t* loc_ptr,
		  unsigned char* contents) {
    //fprintf(stderr, "TRACE: call io300_readc(), ptr at %ld\n", *loc_ptr);
    int byte_read = io300_readc(file);
    //fprintf(stderr, "TRACE: io300_readc() returned 0x%02x, ptr at %ld\n", (unsigned char)byte_read, *loc_ptr);

    if (byte_read < 0) {
	if (*loc_ptr < file_size) {
	    fprintf(
		stderr,
		"Error: `io300_readc` returned -1 when not at end of file:  \n"
		"  expected EOF at %ld but returned -1 at %ld\n",
		file_size, *loc_ptr);
	    return -1;
	}
    } else {
	if (*loc_ptr == file_size) {
	    fprintf(stderr,
		    "Error: `io300_readc` did not return -1 at end of file\n"
		    "  (expected EOF at offset %ld)\n",
		    *loc_ptr);
	    return -1;
	}
	if (byte_read != contents[*loc_ptr]) {
	    fprintf(stderr,
		    "Error: byte returned by `io300_readc` did not match "
		    "at position %ld:  expected 0x%02x, got 0x%02x\n",
		    *loc_ptr, (unsigned char)(contents[*loc_ptr]),
		    (unsigned char)(byte_read));
	    return -1;
	}
	(*loc_ptr)++;
    }
    return 0;
}

/*
 * Performs/tests `io300_writec` using the given opened file, using/updating a pointer to
 * the location of file read/write head `loc_ptr` and the current contents of the
 * file.
 *
 * Returns 0 if the test succeeds, and on failure prints an appropriate error message
 * and returns -1.
 */
int perform_writec(struct io300_file* file, size_t* loc_ptr,
		   unsigned char* contents) {
    if (*loc_ptr < max_size) {
	contents[*loc_ptr] = rand() % UCHAR_MAX;
	if (io300_writec(file, contents[*loc_ptr]) == -1) {
	    fprintf(stderr, "Error: `io300_writec` returned -1\n");
	    return 1;
	}
	(*loc_ptr)++;
	file_size = MAX(file_size, *loc_ptr);
    }
    return 0;
}

/*
 * Performs/tests `io300_read` using the given opened file, using/updating a pointer to
 * the location of file read/write head `loc_ptr` and the current contents of the
 * file.
 *
 * Returns 0 if the test succeeds, and on failure prints an appropriate error message
 * and returns -1.
 */
int perform_read(struct io300_file* file, size_t* loc_ptr,
		 unsigned char* contents) {
    int buffer_size =
	read_size > 0 ? (int)read_size : (rand() % MAX_BLOCK_SIZE) + 1;

    unsigned char buffer[buffer_size];
    memset(buffer, POISON_BYTE, buffer_size); // Fill the buffer with a known value

    int bytes_read = io300_read(file, (char*)buffer, buffer_size);
    if (bytes_read > buffer_size) {
	fprintf(stderr,
		"Error: `io300_read` read more bytes into buffer than its "
		"length (got %d, file size was %d)\n",
		bytes_read, buffer_size);
	return -1;
    } else if (bytes_read < 0) {
	fprintf(stderr,
		"Error: `io300_read` returned an error before end of file "
		"return value %d, position %ld\n",
		bytes_read, *loc_ptr);
	return -1;
    } else if (bytes_read == 0) {
	if (*loc_ptr < file_size) {
	    fprintf(stderr,
		    "Error: `io300_read` returned 0 bytes read before "
		    "expected end of file (got EOF at position %ld, expected "
		    "at %ld)\n",
		    *loc_ptr, file_size);
	    return -1;
	}
    } else {
	if (*loc_ptr == file_size) {
	    fprintf(stderr,
		    "Error: `io300_read` did not return 0 bytes read at end of "
		    "file:  expected EOF at position %ld\n",
		    *loc_ptr);
	    return -1;
	}
	if (*loc_ptr + bytes_read > file_size) {
	    fprintf(stderr,
		    "Error: `io300_read` returned more bytes read than in rest "
		    "of file:  read at position %ld returned %d bytes, but "
		    "file size is %ld\n",
		    *loc_ptr, bytes_read, file_size);
	    return -1;
	}

	for (int j = 0; j < bytes_read; j++) {
	    if (buffer[j] != contents[*loc_ptr + j]) {
		fprintf(stderr,
			"Error: bytes returned by `io300_read` did not match "
			"at position %ld:  expected 0x%02x, got 0x%02x\n",
			*loc_ptr + j, (unsigned char)contents[*loc_ptr + j],
			(unsigned char)buffer[j]);
		return -1;
	    }
	}
	*loc_ptr += bytes_read;
    }
    return 0;
}

/*
 * Performs/tests `io300_write` using the given opened file, using/updating a pointer to
 * the location of file read/write head `loc_ptr` and the current contents of the
 * file.
 *
 * Returns 0 if the test succeeds, and on failure prints an appropriate error message
 * and returns -1.
 */
int perform_write(struct io300_file* file, size_t* loc_ptr,
		  unsigned char* contents) {
    int buffer_size =
	write_size > 0 ? (int)write_size : (rand() % MAX_BLOCK_SIZE) + 1;
    if (*loc_ptr + buffer_size > max_size) buffer_size = max_size - *loc_ptr;
    for (int j = 0; j < buffer_size; j++)
	contents[*loc_ptr + j] = rand() % UCHAR_MAX;

    int bytes_written =
	io300_write(file, (char*)&contents[*loc_ptr], buffer_size);
    if (bytes_written > buffer_size) {
	fprintf(stderr,
		"Error: `io300_write` wrote more bytes from buffer than its "
		"length\n");
	return -1;
    } else if (bytes_written < buffer_size) {
	fprintf(stderr,
		"Error: `io300_write` did not write all bytes from buffer\n");
	return -1;
    } else if (bytes_written < 0) {
	fprintf(stderr, "Error: `io300_write` returned an error\n");
	return -1;
    }
    *loc_ptr += bytes_written;
    file_size = MAX(file_size, *loc_ptr);
    return 0;
}

/*
 * Verifies that the file contents for the file located at `path`
 * the same as that contained in `contents`, whose size is given by `size`.
 * Used to verify implementations of `io300_writec` and `io300_write`.
 *
 * Returns 0 if the verification succeeds; otherwise prints an appropriate
 * error message and returns -1.
 */
int verify_contents(const char* path, unsigned char* contents, size_t size) {
    FILE* in = fopen(path, "r");
    if (in == NULL) {
	perror("fopen");
	return -1;
    }

    size_t loc = 0;
    while (1) {
	int byte_read = fgetc(in);
	if (byte_read == EOF) {
	    if (loc != size) {
		fprintf(
		    stderr,
		    "Error: result file %s shorter than expected, reached end "
		    "of file unexpectedly:  got EOF at position %ld, "
		    "expected size %ld\n",
		    path, loc, size);
		return -1;
	    }
	    break;
	}
	if (loc == size) {
	    fprintf(
		stderr,
		"Error: result file %s longer than expected, did not reach end "
		"of file at expected location:  got EOF at position %ld, "
		"expected size %ld\n",
		path, loc, size);
	    return -1;
	}
	if (byte_read != contents[loc]) {
	    fprintf(stderr,
		    "Error: byte in result file %s did not match expected "
		    "byte at position %ld:  expected 0x%02x, got 0x%02x\n",
		    path, loc, (unsigned char)(contents[loc]),
		    (unsigned char)(byte_read));
	    return -1;
	}

	loc++;
    }

    if (fclose(in) != 0) {
	perror("fclose");
	return -1;
    }

    return 0;
}

/* Cleans up the test by destroying the input file. */
void cleanup(unsigned char* contents) {
    if (disable_cleanup) {
	fprintf(stderr, "Input file saved to %s\n", INPUT_FILENAME);
	if (writing) {
	    write_contents(EXPECTED_FILENAME, file_size, contents);
	    fprintf(stderr, "Expected output written to %s\n",
		    EXPECTED_FILENAME);
	    fprintf(stderr, "Result file preserved at %s\n", RESULT_FILENAME);
	} else {
	    fprintf(
		stderr,
		"No write functions were tested, not writing result file.\n");
	}

	return;
    }

    if (unlink(RESULT_FILENAME) < 0) {
	perror("unlink");
	exit(1);
    }
}
