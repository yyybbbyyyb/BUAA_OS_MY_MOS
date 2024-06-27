#include <args.h>
#include <lib.h>

#define WHITESPACE " \t\r\n"
#define SYMBOLS "<|>&;()#\"`"

// job.c
#define JOB_PATH "jobfile"
#define MAXJOBS 64

struct Job {
    int job_id;
    int env_id;
    char status[10];
    char cmd[1024];
};

struct Job jobs[MAXJOBS];
int next_job_id;
int job_count;

void init_job_data() {
    int fd = open(JOB_PATH, O_CREAT | O_RDWR);
    if (fd < 0) {
        debugf("open jobfile fail\n");
		return;
    }
	debugf("jobfile fd : %d\n", fd);
	next_job_id = 1;
	job_count = 0;
	int next_job_id_tmp;
	int job_count_tmp; 
    write(fd, &next_job_id, sizeof(int));
    write(fd, &job_count, sizeof(int));
	seek(fd, 0);
	read(fd, &next_job_id_tmp, sizeof(int));
    read(fd, &job_count_tmp, sizeof(int));
	if (job_count == job_count_tmp && next_job_id == next_job_id_tmp) {
		debugf("job_data init success!\n");
	} else {
		debugf("read and write init fail\n");
	}
    close(fd);
}

void read_job_data() {
    int fd = open(JOB_PATH, O_RDONLY);
    if (fd < 0) {
        debugf("Failed to open job data file for writing\n");
        return;
    }
    read(fd, &next_job_id, sizeof(int));
    read(fd, &job_count, sizeof(int));
    read(fd, jobs, sizeof(jobs));
    close(fd);
}

void save_job_data() {
    int fd = open(JOB_PATH, O_WRONLY);
    if (fd < 0) {
        debugf("Failed to open job data file for writing\n");
        return;
    }
	//debugf("%d %d %d %s\n", fd, next_job_id, job_count, jobs[0].cmd);
    write(fd, &next_job_id, sizeof(int));
    write(fd, &job_count, sizeof(int));
    write(fd, jobs, sizeof(jobs));
    close(fd);
}

//还有next_job_count等操作

/* Function prototypes */
void add_job(int env_id, char *cmd);
void update_job_status(int env_id, const char *status);
void print_jobs();
void bring_job_to_foreground(int job_id);
void kill_job(int job_id);



void add_job(int env_id, char *cmd) {
	read_job_data();
	if (job_count < MAXJOBS) {
        jobs[job_count].job_id = next_job_id++;
        jobs[job_count].env_id = env_id;
        strcpy(jobs[job_count].status, "Running");
        strcpy(jobs[job_count].cmd, cmd);
        job_count++;
		save_job_data();
    }
}

void update_job_status(int env_id, const char *status) {
	read_job_data();
	for (int i = 0; i < job_count; i++) {
        if (jobs[i].env_id == env_id) {
            strcpy(jobs[i].status, status);
            save_job_data();
			break;
        }
    }
}

void print_jobs() {
	read_job_data();
    for (int i = 0; i < job_count; i++) {
        printf("[%d] %-10s 0x%08x %s\n", jobs[i].job_id, jobs[i].status, jobs[i].env_id, jobs[i].cmd);
    }
}

void bring_job_to_foreground(int job_id) {
	read_job_data();
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].job_id == job_id) {
            if (strcmp(jobs[i].status, "Running") != 0) {
                debugf("fg: (0x%08x) not running\n", jobs[i].job_id);
                return;
            }
            printf("%s", jobs[i].cmd);
            wait(jobs[i].env_id);
            update_job_status(jobs[i].env_id, "Done");
            return;
        }
    }
	//debugf("!!!\n");
    debugf("fg: job (%d) do not exist\n", job_id);
}

void kill_job(int job_id) {
	read_job_data();
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].job_id == job_id) {
            if (strcmp(jobs[i].status, "Running") != 0) {
                debugf("fg: (0x%08x) not running\n", jobs[i].env_id);
                return;
            }
            syscall_env_destroy(jobs[i].env_id);
            update_job_status(jobs[i].env_id, "Done");
            return;
        }
    }
    printf("fg: job (%d) do not exist\n", job_id);
}

//history
#define JOB_PATH ".mosh_history"
#define HISTFILESIZE 20

char history[100][1024];

int history_num;

void add_history(char *s) {
	strcpy(history[history_num], s);
	history_num++;
	if (history_num > 99) {
		user_halt("history_num to many!");
	}
}

void print_history() {
	for (int i = history_num > 20 ? history_num - 20 : 0; i < history_num; i++) {
		debugf("%s\n", history[i]);
	}
}

/* Overview:
 *   Parse the next token from the string at s.
 *
 * Post-Condition:
 *   Set '*p1' to the beginning of the token and '*p2' to just past the token.
 *   Return:
 *     - 0 if the end of string is reached.
 *     - '<' for < (stdin redirection).
 *     - '>' for > (stdout redirection).
 *     - '|' for | (pipe).
 *     - 'w' for a word (command, argument, or file name).
 *
 *   The buffer is modified to turn the spaces after words into zero bytes ('\0'), so that the
 *   returned token is a null-terminated string.
 */
int _gettoken(char *s, char **p1, char **p2, char **buf) {
	*p1 = 0;
	*p2 = 0;
	if (s == 0) {
		return 0;
	}

	while (strchr(WHITESPACE, *s)) {
		*s++ = 0;
	}
	if (*s == 0) {
		return 0;
	}

	if (*s == '`') {
		*s = 0;
		char *start = ++s;
		while (*s && *s != '`') s++;
		if (*s == '`') {
			*s++ = 0;
			int p[2];
			pipe(p);
			int child = fork();
			if (child == 0) {
				dup(p[1], 1);
				close(p[1]);
				close(p[0]);
				runcmd(start);
				exit(0, 0);
			} else {
				close(p[1]);
				char buffer[1024];
				int n = 0;
				debugf("father there1\n");
				while(1) {
					int bytes_read = read(p[0], buffer + n, sizeof(buffer) - n - 1);
					if (bytes_read <= 0) {
						break;
					}
					n += bytes_read;
				}
				buffer[n] = 0;
            	close(p[0]); 
				wait(child);
				memcpy(buf, buffer, strlen(buffer) + 1);
				*p1 = 0;
				*p2 = s;
				return 'w';
			}
		}
	}


	if (*s == '"') {
		char *start = ++s;
		while (*s && *s != '"') s++;
		if (*s == '"') {
			*s++ = 0;
			*p1 = start;
			*p2 = s;
			return 'w';
		}
	}

	if (*s == '&' && *(s + 1) == '&') {
		*p1 = s;
		s += 1;
		*s++ = 0;
		*p2 = s;
		return '&&';
	} else if (*s == '|' && *(s + 1) == '|') {
		*p1 = s;
		s += 1;
		*s++ = 0;
		*p2 = s;
		return '||';
	} else if (*s == '>' && *(s + 1) == '>') {
		*p1 = s;
		*s++ = 0;
		*s++ = 0;
		*p2 = s;
		return '>>';
	}

	if (strchr(SYMBOLS, *s)) {
		int t = *s;
		*p1 = s;
		*s++ = 0;
		*p2 = s;
		return t;
	}

	*p1 = s;
	while (*s && !strchr(WHITESPACE SYMBOLS, *s)) {
		s++;
	}
	*p2 = s;
	return 'w';
}

int gettoken(char *s, char **p1) {
	static int c, nc;
	static char *np1, *np2;
	static char buffer[1024];

	if (s) {
		nc = _gettoken(s, &np1, &np2, &buffer);
		return 0;
	}
	c = nc;
	if (np1 == 0) {
		*p1 = buffer;
	} else {
		*p1 = np1;
	}
	
	nc = _gettoken(np2, &np1, &np2, &buffer);
	return c;
}

#define MAXARGS 128

int parsecmd(char **argv, int *rightpipe, int *logic_op, int *last_logic_op, int *status, int *background) {
	int argc = 0;
	while (1) {
		char *t;
		int fd, r;
		int left = -1;
		int p[2];
		int c = gettoken(0, &t);
		switch (c) {
		case 0:
			//debugf("argc: %d, argvs: %s, %s\n", argc, argv[0], argv[1]);
			return argc;
		case 'w':
			if (argc >= MAXARGS) {
				debugf("too many arguments\n");
				exit(0, 0);
			}
			argv[argc++] = t;
			break;
		case '<':
			if (gettoken(0, &t) != 'w') {
				debugf("syntax error: < not followed by word\n");
				exit(0, 0);
			}
			// Open 't' for reading, dup it onto fd 0, and then close the original fd.
			// If the 'open' function encounters an error,
			// utilize 'debugf' to print relevant messages,
			// and subsequently terminate the process using 'exit'.
			/* Exercise 6.5: Your code here. (1/3) */

			
			if ((r = open(t, O_RDONLY)) < 0) {
				debugf("syntax error: < followed the word: %s cannot open\n", t);
				int c = gettoken(0, &t);
				if (c == '&&' || c == '||') {
					*logic_op = c;	
					left = fork();
					if (left > 0) {
						int tmp_status = wait_return(left);
						//debugf("i get aaaaaaaaa good return: %d\n", tmp_status);
						*logic_op = 0;
						*last_logic_op = c;
						*status = tmp_status;
						//dup(0, 1);
						//debugf("last_lo is : %d\n", *last_logic_op);
						return parsecmd(argv, rightpipe, logic_op, last_logic_op, status, background);
					} else {
						close_all();
						env = &envs[ENVX(syscall_getenvid())];
						//debugf("i[%08x] will sent 1 to [%08x] for jump\n", env->env_id, env->env_parent_id);
						ipc_send(env->env_parent_id, 1, 0, 0);
						if (rightpipe) {
							wait(rightpipe);
						}
						exit(0, 0);
					}
					break;
				}
				exit(0, 0);
			}
			fd = r;
			dup(fd, 0);
			close(fd);
			break;

		case '>':
			if (gettoken(0, &t) != 'w') {
				debugf("syntax error: > not followed by word\n");
				exit(0, 0);
			}
			//debugf("Redirecting stdout to %s\n", t); // 增加调试信息
			// Open 't' for writing, create it if not exist and trunc it if exist, dup
			// it onto fd 1, and then close the original fd.
			// If the 'open' function encounters an error,
			// utilize 'debugf' to print relevant messages,
			// and subsequently terminate the process using 'exit'.
			/* Exercise 6.5: Your code here. (2/3) */
			if ((r = open(t, O_WRONLY)) < 0) {
				if ((r = open(t, O_WRONLY | O_CREAT | O_TRUNC)) < 0) {
					debugf("syntax error: > followed the word: %s cannot open\n", t);
					exit(0, 0);
				}
			}
			fd = r;
			dup(fd, 1);
			close(fd);
			break;

		case '>>':
			if (gettoken(0, &t) != 'w') {
				debugf("syntax error: > not followed by word\n");
				exit(0, 0);
			}
			// Open 't' for writing, create it if not exist and trunc it if exist, dup
			// it onto fd 1, and then close the original fd.
			// If the 'open' function encounters an error,
			// utilize 'debugf' to print relevant messages,
			// and subsequently terminate the process using 'exit'.
			/* Exercise 6.5: Your code here. (2/3) */
			if ((r = open(t, O_WRONLY | O_CREAT | O_APPEND)) < 0) {
				debugf("syntax error: > followed the word: %s cannot open\n", t);
				exit(0, 0);
			}
			fd = r;			
			dup(fd, 1);
			close(fd);
			break;

		case '|':
			/*
			 * First, allocate a pipe.
			 * Then fork, set '*rightpipe' to the returned child envid or zero.
			 * The child runs the right side of the pipe:
			 * - dup the read end of the pipe onto 0
			 * - close the read end of the pipe
			 * - close the write end of the pipe
			 * - and 'return parsecmd(argv, rightpipe)' again, to parse the rest of the
			 *   command line.
			 * The parent runs the left side of the pipe:
			 * - dup the write end of the pipe onto 1
			 * - close the write end of the pipe
			 * - close the read end of the pipe
			 * - and 'return argc', to execute the left of the pipeline.
			 */
			/* Exercise 6.5: Your code here. (3/3) */
			pipe(p);
			*rightpipe = fork();
			if (*rightpipe == 0) {
				dup(p[0], 0);
				close(p[0]);
				close(p[1]);
				return parsecmd(argv, rightpipe, logic_op, last_logic_op, status, background);
			} else if (*rightpipe > 0) {
				dup(p[1], 1);
				close(p[1]);
				close(p[0]);
				return argc;
			}
			break;
		case ';':
			left = fork();
			if (left > 0) {
				wait(left);
				return parsecmd(argv, rightpipe, logic_op, last_logic_op, status, background);
			} else {
				return argc;
			}
			break;
		case '#':
			return argc;
		case '&&':
			*logic_op = '&&';	
			left = fork();
			if (left > 0) {
				int tmp_status = wait_return(left);
				//debugf("i get aaaaaaaaa good return: %d\n", tmp_status);
				*logic_op = 0;
				*last_logic_op = '&&';
				*status = tmp_status;
				//dup(0, 1);
				return parsecmd(argv, rightpipe, logic_op, last_logic_op, status, background);
			} else {
				//debugf("argc: %d, argvs: %s, %s\n", argc, argv[0], argv[1]);
				return argc;
			}
			break;
        case '||':
            *logic_op = '||';	
			left = fork();
			if (left > 0) {
				int tmp_status = wait_return(left);
				//debugf("i get aaaaaaaaa good return: %d\n", tmp_status);
				*logic_op = 0;
				*last_logic_op = '||';
				*status = tmp_status;
				return parsecmd(argv, rightpipe, logic_op, last_logic_op, status, background);
			} else {
				return argc;
			}
			break;
		case '&':
           	if (argc == 0) {
				debugf("syntax error: & not followed by word\n");
				exit(0, 0);
		  	}
			*background = 1;
			return argc;
		}
	}
	return argc;
}

int my_isspace(char c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r');
}

int my_isdigit(char c) {
    return (c >= '0' && c <= '9');
}

int my_atoi(const char *str) {
    // Skip leading whitespaces
    while (my_isspace(*str)) {
        str++;
    }

    // Check for optional sign
    int sign = 1;
    if (*str == '-' || *str == '+') {
        if (*str == '-') {
            sign = -1;
        }
        str++;
    }

    // Convert the digits
    int result = 0;
    while (my_isdigit(*str)) {
        result = result * 10 + (*str - '0');
        str++;
    }

    return result * sign;
}

void runcmd(char *s) {  

	char buffer[256];
	if ( *(s + strlen(s) - 1) == '&') {
		strcpy(buffer, s);
	} 
	gettoken(s, 0);

	//debugf("hello1\n"); 
	char *argv[MAXARGS];
	int rightpipe = 0;
	int logic_op = 0;
	int last_logic_op = 0;
	int background = 0;
	int status;
	int argc = parsecmd(argv, &rightpipe, &logic_op, &last_logic_op, &status, &background);
	//debugf("argv: %s, last_logic: %d, status: %d\n", argv[0], last_logic_op, status);
	if (argc == 0) {
		return;
	}
	argv[argc] = 0;

	if (strcmp(argv[0], "history") == 0) {
		//debugf("i am jobs\njobscount: %d\n", job_count);
		print_history();
		exit(0, 0);
	}

	if (strcmp(argv[0], "jobs") == 0) {
		//debugf("i am jobs\njobscount: %d\n", job_count);
		print_jobs();
		exit(0, 0);
	}

	if (strcmp(argv[0], "fg") == 0) {
		if (argc < 2) {
			printf("fg: job id required\n");
			exit(0, 0);
		}
		int job_id = my_atoi(argv[1]);
		//debugf("%d\n", job_id);
		bring_job_to_foreground(job_id);
		exit(0, 0);
	}

	if (strcmp(argv[0], "kill") == 0) {
        if (argc < 2) {
            printf("kill: job id required\n");
            return;
        }
        int job_id = my_atoi(argv[1]);
        kill_job(job_id);
        return;
    }

	int child = -1;
	//debugf("hello2\n");
	debugf("argv0: %s and argc: %d and pipe: %d\n", argv[0], argc, rightpipe);
	if ((last_logic_op == '&&' && status != 0) ||
		(last_logic_op == '||' && status == 0)) {
		if (logic_op == '&&' || logic_op == '||') {
			env = &envs[ENVX(syscall_getenvid())];
			//debugf("i[%08x] will sent 1 to [%08x] for jump\n", env->env_id, env->env_parent_id);
			ipc_send(env->env_parent_id, status, 0, 0);
		} 
		if (rightpipe) {
			wait(rightpipe);
		}
		exit(0, 0);
	} else {
		//debugf("i run to this\n");
		child = spawn(argv[0], argv);
	}
	
	close_all();
	//debugf("i %s run to this2\n", argv[0]);
	if (child >= 0) {
		//debugf("i run to this3\n");
		if (background) {
			//debugf("backcount: %d, i %d: %s will add\n", job_count, child, s);
			add_job(child, buffer);
			//debugf("backcount: %d\n", job_count);
			int status = wait_return(child);
			update_job_status(child, "Done");
		} else {
			int status = wait_return(child);
			if (logic_op == '&&' || logic_op == '||') {
				env = &envs[ENVX(syscall_getenvid())];
				//debugf("i[%08x] will sent to [%08x]\n", env->env_id, env->env_parent_id);
				ipc_send(env->env_parent_id, status, 0, 0);
			}
			debugf("i am %s, return : %d\n", argv[0], status);
		}
	} else {
		debugf("spawn %s: %d\n", argv[0], child);
		if (logic_op == '&&' || logic_op == '||') {
			env = &envs[ENVX(syscall_getenvid())];
			//debugf("i[%08x] will sent to [%08x]\n", env->env_id, env->env_parent_id);
			ipc_send(env->env_parent_id, status, 0, 0);
		}
	}
	
	if (rightpipe) {
		//debugf("i %s will wait you\n", argv[0]);
		wait(rightpipe);
		//debugf("i run\n");
	}

	exit(0, 0);
}


void readline(char *buf, u_int n) {
	int r;
	for (int i = 0; i < n; i++) {
		if ((r = read(0, buf + i, 1)) != 1) {
			if (r < 0) {
				debugf("read error: %d\n", r);
			}
			exit(0, 0);
		}
		if (buf[i] == '\b' || buf[i] == 0x7f) {
			if (i > 0) {
				i -= 2;
			} else {
				i = -1;
			}
			if (buf[i] != '\b') {
				printf("\b");
			}
		}
		if (buf[i] == '\r' || buf[i] == '\n') {
			buf[i] = 0;
			return;
		}
	}
	debugf("line too long\n");
	while ((r = read(0, buf, 1)) == 1 && buf[0] != '\r' && buf[0] != '\n') {
		;
	}
	buf[0] = 0;
}

char buf[1024];

void usage(void) {
	printf("usage: sh [-ix] [script-file]\n");
	exit(1, 1);
}

int main(int argc, char **argv) {
	int r;
	int interactive = iscons(0);
	int echocmds = 0;
	printf("\n:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
	printf("::                                                         ::\n");
	printf("::                 MOS Shell 2024 by yyb                   ::\n");
	printf("::                                                         ::\n");
	printf(":::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
	
	// printf("num: %d\n", argc);
	// for (size_t i = 0; i < argc; i++)
	// {
	// 	printf("%s\n", argv[i]);
	// }

	ARGBEGIN {
	case 'i':
		interactive = 1;
		break;
	case 'x':
		echocmds = 1;
		break;
	default:
		usage();
	}
	ARGEND

	init_job_data();

	history_num = 0;

	if (argc > 1) {
		usage();
	}
	if (argc == 1) {
		// 如果提供了脚本文件作为参数，打开该文件作为标准输入
		close(0);	// 关闭标准输入
		if ((r = open(argv[0], O_RDONLY)) < 0) {
			user_panic_return("open %s: %d", argv[0], r);
		}
		user_assert(r == 0);	// 确保文件描述符是 0（标准输入）
	}
	
	// 主循环，读取和执行命令
	for (;;) {
		if (interactive) {
			printf("\n$ ");
		}
		readline(buf, sizeof buf);
		add_history(buf);
		if (buf[0] == '#') {
			continue;
		}
		if (echocmds) {
			printf("# %s\n", buf);
		}
		if ((r = fork()) < 0) {
			user_panic_return("fork: %d", r);
		}
		if (r == 0) {
			runcmd(buf);
		} else {
			syscall_yield();
			if (buf[strlen(buf) - 1] != '&') {
				wait(r);
			}
			//debugf("[%08x] env finish__big\n", r);
		}
	}
	return 0;
}
