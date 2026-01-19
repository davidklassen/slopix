#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define INITRAMFS_MAGIC 0x4D415253

static uint32_t align4(uint32_t n) {
	return (n + 3) & ~3;
}

static void write_padding(FILE *out, uint32_t len) {
	uint32_t padded = align4(len);
	for (uint32_t i = len; i < padded; i++) {
		fputc(0, out);
	}
}

int main(int argc, char **argv) {
	if (argc < 3) {
		fprintf(stderr, "Usage: mkramfs <output> <file1> [file2 ...]\n");
		return 1;
	}

	FILE *out = fopen(argv[1], "wb");
	if (!out) {
		perror("fopen output");
		return 1;
	}

	uint32_t magic = INITRAMFS_MAGIC;
	uint32_t count = argc - 2;

	fwrite(&magic, sizeof(magic), 1, out);
	fwrite(&count, sizeof(count), 1, out);

	for (int i = 2; i < argc; i++) {
		FILE *in = fopen(argv[i], "rb");
		if (!in) {
			perror(argv[i]);
			fclose(out);
			return 1;
		}

		fseek(in, 0, SEEK_END);
		uint32_t data_len = ftell(in);
		fseek(in, 0, SEEK_SET);

		const char *name = strrchr(argv[i], '/');
		name = name ? name + 1 : argv[i];

		char *dot = strrchr(name, '.');
		uint32_t name_len = dot ? (uint32_t)(dot - name) : strlen(name);

		fwrite(&name_len, sizeof(name_len), 1, out);
		fwrite(&data_len, sizeof(data_len), 1, out);
		fwrite(name, 1, name_len, out);
		write_padding(out, name_len);

		char *buf = malloc(data_len);
		fread(buf, 1, data_len, in);
		fwrite(buf, 1, data_len, out);
		write_padding(out, data_len);

		free(buf);
		fclose(in);
	}

	fclose(out);
	return 0;
}
