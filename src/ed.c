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

void print_error_message(char *message, bool verbose) {
	puts("?");
	if (verbose) {
		puts(message);
	}
}

bool ensure_no_suffix(char **error, int length, int pos, bool verbose) {
	if (length > pos + 1) {
		*error = "Unknown command suffix";
		print_error_message(*error, verbose);
		return false;
	}

	return true;
}

bool ensure_range_valid(char **error, size_t from, size_t to, text *text, bool verbose) {
	if (from < 1 || to > text->lineCount || from > to) {
		*error = "Invalid address";
		print_error_message(*error, verbose);
		return false;
	}
	return true;
}

bool ensure_no_range_set(char **error, bool rangeSet, bool verbose) {
	if (rangeSet) {
		*error = "Unexpected address";
		print_error_message(*error, verbose);
		return false;
	}
	return true;
}

void print_range(text *text, size_t from, size_t to) {
	line *line = text->firstLine;
	for (size_t i = 1; i <= to; i++) {
		if (i >= from) {
			fputs(line->text, stdout);
		}
		line = line->next;
	}
}

int handle_input(text *text) {
	char *lastError = NULL;
	bool verbose = false;
	int currentLine = text->lineCount;

	char *input;

	while ((input = read_line(stdin)) != NULL) {
		int length = strlen(input);
		int pos;
		int lineFrom;
		int lineTo;
		char command;
		bool rangeSet = false;
		if (sscanf(input, "%d,%d%c%n", &lineFrom, &lineTo, &command, &pos) == 3) {
			// Address range
			//printf("Read command, range: %d to %d, command: %c (pos: %d)\n", lineFrom, lineTo, command, pos);
			rangeSet = true;
		} else if (sscanf(input, "%d%c%n", &lineFrom, &command, &pos) == 2) {
			// Address
			//printf("Read command, line: %d, command: %c (pos: %d)\n", lineFrom, command, pos);
			rangeSet = true;
			lineTo = lineFrom;
		} else if (sscanf(input, "%c%n", &command, &pos) == 1) {
			// No address
			//printf("Read command, command: %c (pos: %d)\n", command, pos);
			lineFrom = currentLine;
			lineTo = currentLine;
		} else {
			// Should never happen as there will always be at least a newline or NULL is returned
		}

		free(input);

		if (command == 'H') {
			if (!ensure_no_suffix(&lastError, length, pos, verbose)) continue;
			if (rangeSet) {
				if (!ensure_range_valid(&lastError, lineFrom, lineTo, text, verbose)) continue;
				if (!ensure_no_range_set(&lastError, rangeSet, verbose)) continue;
			}

			verbose = !verbose;
			if (verbose && lastError != NULL) {
				puts(lastError);
			} 
		} else if (command == 'h') {
			if (!ensure_no_suffix(&lastError, length, pos, verbose)) continue;
			if (rangeSet) {
				if (!ensure_range_valid(&lastError, lineFrom, lineTo, text, verbose)) continue;
				if (!ensure_no_range_set(&lastError, rangeSet, verbose)) continue;
			}

			if (lastError != NULL) {
				puts(lastError);
			}
		} else if (command == 'p') {
			if (!ensure_no_suffix(&lastError, length, pos, verbose)) continue;
			if (!ensure_range_valid(&lastError, lineFrom, lineTo, text, verbose)) continue;

			print_range(text, lineFrom, lineTo);
			currentLine = lineTo;
		} else if (command == '\n') {
			if (!rangeSet) {
				lineFrom++;
				lineTo++;
			}

			if (!ensure_no_suffix(&lastError, length, pos, verbose)) continue;
			if (!ensure_range_valid(&lastError, lineFrom, lineTo, text, verbose)) continue;

			print_range(text, lineFrom, lineTo);
			currentLine = lineTo;
		} else {
			lastError = "Unknown command";
			print_error_message(lastError, verbose);
		}
	}

	return lastError != NULL;
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
	} else {
		text = calloc(1, sizeof(*text));
	}

	if (text != NULL) {
		printf("%zu\n", text->characterCount);
	}

	int error = handle_input(text);

	return error;
}
