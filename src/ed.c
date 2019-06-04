#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char ** argv) {
	if (argc == 1) {
		errx(1, "no file provided");
	}

	for (int i = 0; i < argc; i++) {
		if (argv[i][0] == '-') {
			warnx("illegal option -- %s", (argv[i] + 1));
			puts("usage: ed file");
			return 1;
		}
	}

	char* filename = argv[1];
	FILE *file = fopen(filename, "r+");
	if (file == NULL) {
		printf("%s: %s\n", filename, strerror(errno));
	}

	size_t inputBufferSize = 100;
	char *buffer = malloc(inputBufferSize * sizeof(*buffer));

	int error = 0;

	while (fgets(buffer, inputBufferSize, stdin)) {
		if (!strcmp(buffer, "q\n")) {
			break;
		} else {
			if (file == NULL) {
				error = 1;
			}

			puts("?");
		}
	}

	return error;
}
