#include <lib.h>

void usage(void) {
	printf("usage: touch [file...]\n");
	exit(1, 1);
}

void touch(char* path) {
    int fd;
    fd = open(path, O_RDONLY);
    if (fd > 0) {
        close(fd);
        return;
    }
    fd = open(path, O_CREAT);
    if (fd < 0) {
        printf("touch: cannot touch '%s': No such file or directory\n", path);
    } else {
        close(fd);
    }
}

int main(int argc, char **argv) {

	ARGBEGIN {
	default:
		usage();
	}
	ARGEND

    if (argc == 0) {
        usage();
        return -1;
    } else if (argc == 1) {
        touch(argv[0]);
    } else {
        printf("touch: too much argv!\n");
    }
    return 0;
}