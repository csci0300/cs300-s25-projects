#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "test_helpers.h"

/* Helper function to clean up old output files
 * left over from a previous test run
 * (You should not need to worry about this)
 */
void cleanup_output_file(char *path) {
    if (access(path, F_OK) == 0) { // If file exists
	if (remove(path) == -1) {  // Remove it
	    perror("Could not clean up old output file");
	    fprintf(stderr, "Please remove output file %s before continuing\n (e.g., `rm %s`)\n",
		    path, path);
	    exit(1);
	}
    }
}
