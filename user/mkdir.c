#include <lib.h>

int flag[256];

void usage(void) {
	printf("usage: touch [file...]\n");
	exit(1, 1);
}

void mkdir(char *path) {
    int fd;
    fd = open(path, O_RDONLY);
    if (fd > 0) {
        close(fd);
        if (!flag['p']) {
            printf("mkdir: cannot create directory '%s': File exists\n", path);
        }
        return;
    }
    fd = open(path, O_MKDIR);
    if (fd < 0) {
        if (!flag['p']) {
            printf("mkdir: cannot create directory '%s': No such file or directory\n", path);
        } else {
            char parentPath[128];
            strcpy(parentPath, path);
            char *slash = strrchr(parentPath, '/');
            if (slash) {
                *slash = '\0';
                if (strlen(parentPath) > 0) {
                    mkdir(parentPath);
                }
            }
            fd = open(path, O_MKDIR);
            if (fd < 0) {
                printf("wrong in create parent dir\n");
            } else {
                close(fd);
            }
        }
    } else {
        close(fd);
    }
}

int main(int argc, char **argv) {
    ARGBEGIN {
	case 'p':
		flag[(u_char)ARGC()]++;
		break;
	}
	ARGEND

    if (argc == 0) {
        usage();
        return -1;
    } else if (argc == 1) {
        mkdir(argv[0]);
    } else {
        printf("touch: too much argv!\n");
    }

    return 0;
}