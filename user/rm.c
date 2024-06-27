#include <lib.h>

int flag[256];

void usage(void) {
    printf("usage: rm [-r] [-f] <file|dir>\n");
    exit(1, 1);
}

void remove_item(char *path) {
    int r;
    struct Stat st;
    
    r = stat(path, &st);
    
    if (r < 0) {
        printf("rm: cannot remove '%s': No such file or directory\n", path);
    } else {
        if (st.st_isdir) {
            printf("rm: cannot remove '%s': Is a directory\n", path);
        } else {
            remove(path);
        }
    }
}

void remove_recursive(char *path) {
    int r;
    struct Stat st;
    
    r = stat(path, &st);
    if (r < 0) {
        if (!flag['f']) {
            printf("rm: cannot remove '%s': No such file or directory\n", path);
        }
        return;
    }

    if (st.st_isdir) {
        remove(path);
    } else {
        remove_item(path);
    }
}

int main(int argc, char **argv) {
    
    ARGBEGIN {
	default:
		usage();
	case 'f':
	case 'r':
		flag[(u_char)ARGC()]++;
		break;
	}
	ARGEND

    if (argc == 0) {
        usage();
        return -1;
    }

    if (argc == 1) {
        if (flag['r']) {
            remove_recursive(argv[0]);
        } else {
            remove_item(argv[0]);
        }
    } else {
        printf("touch: too much argv!\n");
    }
    return 0;

}
