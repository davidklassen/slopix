#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	if (argc != 3) {
		printf("usage: cmp file1 file2\n");
		return 2;
	}

	FILE *f1 = fopen(argv[1], "r");
	if (!f1) {
		printf("cmp: %s: cannot open\n", argv[1]);
		return 2;
	}

	FILE *f2 = fopen(argv[2], "r");
	if (!f2) {
		printf("cmp: %s: cannot open\n", argv[2]);
		fclose(f1);
		return 2;
	}

	long offset = 0;
	long line = 1;

	for (;;) {
		int c1 = fgetc(f1);
		int c2 = fgetc(f2);

		if (c1 == EOF && c2 == EOF) {
			break;
		}

		if (c1 == EOF) {
			printf("cmp: EOF on %s after byte %ld\n", argv[1], offset);
			fclose(f1);
			fclose(f2);
			return 1;
		}

		if (c2 == EOF) {
			printf("cmp: EOF on %s after byte %ld\n", argv[2], offset);
			fclose(f1);
			fclose(f2);
			return 1;
		}

		if (c1 != c2) {
			printf("%s %s differ: byte %ld, line %ld\n", argv[1], argv[2], offset + 1, line);
			fclose(f1);
			fclose(f2);
			return 1;
		}

		offset++;
		if (c1 == '\n') {
			line++;
		}
	}

	fclose(f1);
	fclose(f2);
	return 0;
}
