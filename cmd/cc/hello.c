int write(int fd, const void *buf, int count);

int main(void) {
	write(1, "hello from cc!\n", 15);
	return 0;
}
