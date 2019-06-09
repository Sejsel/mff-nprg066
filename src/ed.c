#include <ctype.h>
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

void print_error_message(char *message, bool verbose) {
	puts("?");
	if (verbose) {
		puts(message);
	}
}

void free_text(text *text) {
	line *l = text->firstLine;
	while (l != NULL) {
		line *old = l;
		l = l->next;
		free(old->text);
		free(old);
	}

	free(text);
}

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
		line *line = calloc(1, sizeof(*line));
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

void write_text(text *text, char *filename, char **error, bool verbose) {
	FILE *file = fopen(filename, "w+");
	if (file == NULL) {
		if (errno > 0) {
			fprintf(stderr, "%s: %s\n", filename, strerror(errno));

			*error = "Cannot open output file";
			print_error_message(*error, verbose);
			return;
		}
	}

	line *line = text->firstLine;
	size_t printed = 0;
	while (line != NULL) {
		int result = fprintf(file, "%s", line->text);
		if (result < 0) {
			fprintf(stderr, "%s\n", strerror(errno));
			break;
		}
		printed += result;
		line = line->next;
	}

	fclose(file);

	printf("%zu\n", printed);
}

bool ensure_no_suffix(char **error, int length, int pos, bool verbose) {
	if (length > pos + 1) {
		*error = "Invalid command suffix";
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

void print_range_numbered(text *text, size_t from, size_t to) {
	line *line = text->firstLine;
	for (size_t i = 1; i <= to; i++) {
		if (i >= from) {
			printf("%zu\t%s", i, line->text);
		}
		line = line->next;
	}
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

/*
 * Returns true if filename is set. Can be NULL if there was an error while parsing and nothing
 * should be written in that case.
 * Returns false if the original filename should be written to, filename is not changed then
 */
bool parse_write_command(char *command, int pos, char **error, bool verbose, char **filename) {
	int length = strlen(command);
	int pureLength = length - 1; // There is a newline at the end which we want to ignore

	if (pos != pureLength && !isspace(command[pos])) {
		// The character after the w has to be whitespace or else this is an invalid suffix.
		// This is a bit of a hack, as there are extra characters,
		// this will always trigger and throw the invalid suffix error.
		ensure_no_suffix(error, length, pos, verbose);

		*filename = NULL;
		return true;
	}

	char *name = NULL;
	if (pos == pureLength) {
		// There is nothing following the command, the original name is used
		return false;
	} else {
		int maxFilenameLength = pureLength - pos - 1;
		name = calloc(maxFilenameLength + 1, sizeof(*name));
		// This properly ignores all leading spaces to the filename but
		// preserves those within and after the filename just like GNU ed
		sscanf(&command[pos], " %[^\n]s\n", name);

		if (strlen(name) == 0) {
			// The command only has leading spaces, so instead the original filename is used
			free(name);
			return false;
		}

		*filename = name;
		return true;
	}
}

int handle_input(text *text, char *initialError, char *originalFilename) {
	char *lastError = NULL;
	bool verbose = false;
	size_t currentLine = text->lineCount;

	char *input = NULL;

	while (true) {
		if (input != NULL) {
			free(input);
		}

		if ((input = read_line(stdin)) == NULL) {
			break;
		}

		int length = strlen(input);
		int pos;
		int lineFrom;
		int lineTo;
		char command;
		char rangeCharFrom;
		char rangeCharTo;
		bool rangeSet = false;
		if (sscanf(input, "%d,%d%c%n", &lineFrom, &lineTo, &command, &pos) == 3) {
			// Address range
			rangeSet = true;
		} else if (sscanf(input, "%c,%c%c%n", &rangeCharFrom, &rangeCharTo, &command, &pos) == 3 &&
		           (rangeCharFrom == '.' || rangeCharFrom == '$') &&
		           (rangeCharTo == '.' || rangeCharTo == '$')) {
			// Address range (using two special characters)
			lineFrom = rangeCharFrom == '.' ? currentLine : text->lineCount;
			lineTo = rangeCharTo == '.' ? currentLine : text->lineCount;
			rangeSet = true;
		} else if (sscanf(input, "%c,%d%c%n", &rangeCharFrom, &lineTo, &command, &pos) == 3 &&
		           (rangeCharFrom == '.' || rangeCharFrom == '$')) {
			// Address range (ending with a special character)
			lineFrom = rangeCharFrom == '.' ? currentLine : text->lineCount;
			rangeSet = true;
		} else if (sscanf(input, "%d,%c%c%n", &lineFrom, &rangeCharTo, &command, &pos) == 3 &&
		           (rangeCharTo == '.' || rangeCharTo == '$')) {
			// Address range (starting with a special character)
			lineTo = rangeCharTo == '.' ? currentLine : text->lineCount;
			rangeSet = true;
		} else if (sscanf(input, "%d%c%n", &lineFrom, &command, &pos) == 2) {
			// Address
			lineTo = lineFrom;
			rangeSet = true;
		} else if (sscanf(input, ".%c%n", &command, &pos) == 1) {
			// Address (current)
			lineFrom = currentLine;
			lineTo = lineFrom;
			rangeSet = true;
		} else if (sscanf(input, "$%c%n", &command, &pos) == 1) {
			// Address (end of file)
			lineFrom = text->lineCount;
			lineTo = lineFrom;
			rangeSet = true;
		} else if (sscanf(input, "%c%n", &command, &pos) == 1) {
			// No address
			lineFrom = currentLine;
			lineTo = currentLine;
		}

		if (lineFrom < 0) {
			lineFrom = currentLine + lineFrom;
		}
		if (lineTo < 0) {
			lineTo = currentLine + lineTo;
		}

		if (command == 'H' || command == 'h') {
			if (!ensure_no_suffix(&lastError, length, pos, verbose)) continue;
			if (rangeSet) {
				if (!ensure_range_valid(&lastError, lineFrom, lineTo, text, verbose)) continue;
				if (!ensure_no_range_set(&lastError, rangeSet, verbose)) continue;
			}

			if (command == 'H') {
				verbose = !verbose;
			}

			if (verbose || command == 'h') {
				if (lastError != NULL) {
					puts(lastError);
				} else if (initialError != NULL) {
					puts(initialError);
				}
			}
		} else if (command == 'p' || command == '\n') {
			if (command == '\n' && !rangeSet) {
				lineFrom++;
				lineTo++;
			}

			if (!ensure_no_suffix(&lastError, length, pos, verbose)) continue;
			if (!ensure_range_valid(&lastError, lineFrom, lineTo, text, verbose)) continue;

			print_range(text, lineFrom, lineTo);
			currentLine = lineTo;
		} else if (command == 'n') {
			if (!ensure_no_suffix(&lastError, length, pos, verbose)) continue;
			if (!ensure_range_valid(&lastError, lineFrom, lineTo, text, verbose)) continue;

			print_range_numbered(text, lineFrom, lineTo);
			currentLine = lineTo;
		} else if (command == 'w') {
			if (rangeSet) {
				if (!ensure_range_valid(&lastError, lineFrom, lineTo, text, verbose)) continue;
				if (!ensure_no_range_set(&lastError, rangeSet, verbose)) continue;
			}

			char *filename;
			bool useFilename = parse_write_command(input, pos, &lastError, verbose, &filename);
			if (useFilename && filename != NULL) {
				write_text(text, filename, &lastError, verbose);
				free(filename);
			} else {
				// The original filename is used
				if (originalFilename == NULL) {
					lastError = "No current filename";
					print_error_message(lastError, verbose);
					continue;
				}
				write_text(text, originalFilename, &lastError, verbose);
			}
		} else if (command == 'q') {
			if (!ensure_no_suffix(&lastError, length, pos, verbose)) continue;
			if (rangeSet) {
				if (!ensure_range_valid(&lastError, lineFrom, lineTo, text, verbose)) continue;
				if (!ensure_no_range_set(&lastError, rangeSet, verbose)) continue;
			}

			break;
		} else {
			if (!ensure_range_valid(&lastError, lineFrom, lineTo, text, verbose)) continue;

			lastError = "Unknown command";
			print_error_message(lastError, verbose);
		}
	}

	if (input != NULL) {
		free(input);
	}

	return lastError != NULL;
}

int main(int argc, char **argv) {
	for (int i = 0; i < argc; i++) {
		if (argv[i][0] == '-') {
			warnx("illegal option -- %s", (argv[i] + 1));
			puts("usage: ed [file]");
			return 1;
		}
	}

	text *text = NULL;
	char *initialError = NULL;
	char *filename = NULL;

	if (argc > 1) {
		filename = argv[1];

		FILE *file = fopen(filename, "r");
		if (file == NULL) {
			fprintf(stderr, "%s: %s\n", filename, strerror(errno));
		}

		if (file != NULL) {
			errno = 0;
			text = read_text(file);
			if (errno > 0) {
				fprintf(stderr, "%s\n", strerror(errno));
				free_text(text);
				text = NULL;
			}
			if (text != NULL) {
				printf("%zu\n", text->characterCount);
			}
			fclose(file);
		}

		if (text == NULL) {
			initialError = "Cannot open input file";
		}
	}

	if (text == NULL) {
		text = calloc(1, sizeof(*text));
	}

	int error = handle_input(text, initialError, filename);

	// No need to bother with this, but it makes it easier to check if there is any leak
	free_text(text);

	return error;
}
