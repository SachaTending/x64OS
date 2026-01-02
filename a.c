#include <stdio.h>

int main(int argc, char **argv) {
	setvbuf(stdout, NULL, _IONBF, 0);
	printf("argc: %d\n", argc);
	printf("argv: 0x%lx\n", argv);
	for (int i=0;i<argc;i++) {
		printf("argv[%d]=%s\n", i, argv[i]);
	}
	while(1);
	return 0;
}