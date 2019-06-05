#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct text_line line;
typedef struct text text;

struct text {
	line *firstLine;
	size_t lineCount;
	size_t characterCount;
};

struct text_line {
	char *text;
	line *prev;
	line *next;
};

char *read_line(FILE *stream) {
	bool read = false;
	size_t currentSize = 16;
	size_t spaceAvailable = currentSize;
	char *buffer = malloc(currentSize * sizeof(*buffer));
	char *start = buffer;

	while (fgets(start, spaceAvailable, stream)) {
		if (strchr(start, '\n') != NULL) {
			read = true;
			break;
		} else {
			size_t oldSize = currentSize;
			currentSize *= 2;
			spaceAvailable = currentSize - oldSize + 1; // The + 1 is the previous NUL character
			buffer = realloc(buffer, currentSize * sizeof(*buffer));
			start = &buffer[currentSize - spaceAvailable];
		}
	}

	if (!read) {
		free(buffer);
		return NULL;
	}

	return buffer;
}

text *read_text(FILE *file) {
	line *firstLine = NULL;
	line *lastLine = NULL;

	int lineCount = 0;
	int characterCount = 0;
	char *fileLine;
	while ((fileLine = read_line(file)) != NULL) {
		line *line = malloc(sizeof(*line));
		line->text = fileLine;
		line->prev = lastLine;
		if (line->prev != NULL) {
			line->prev->next = line;
		}

		if (firstLine == NULL) {
			firstLine = line;
		}

		lineCount++;
		characterCount += strlen(line->text);

		lastLine = line;
	}

	text *text = malloc(sizeof(*text));
	text->firstLine = firstLine;
	text->lineCount = lineCount;
	text->characterCount = characterCount;

	return text;
}

int handle_input(text *text) {
	size_t inputBufferSize = 100;
	char *buffer = malloc(inputBufferSize * sizeof(*buffer));

	int error = 0;

	while (fgets(buffer, inputBufferSize, stdin)) {
		if (!strcmp(buffer, "q\n")) {
			break;
		} else {
			if (text == NULL) {
				error = 1;
			}

			puts("?");
		}
	}

	return error;
}

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

	char *filename = argv[1];
	FILE *file = fopen(filename, "r+");
	if (file == NULL) {
		printf("%s: %s\n", filename, strerror(errno));
	}

	text *text = NULL;
	if (file != NULL) {
		text = read_text(file);
	}

	int error = handle_input(text);

	return error;
}
