#include <env.h>
#include <lib.h>
#include <mmu.h>

void exit(int status, int need) {
	// After fs is ready (lab5), all our open files should be closed before dying.
#if !defined(LAB) || LAB >= 5
	close_all();
#endif
	
	
	//debugf("[%08x] env %d status %d need\n", syscall_getenvid(), status, need);
	//env = &envs[ENVX(syscall_getenvid())];
	//debugf("i[%08x] will sent to [%08x]\n", env->env_id, env->env_parent_id);

	if (need) {
		//debugf("1\n");
		env = &envs[ENVX(syscall_getenvid())];
		//debugf("2\n");
		ipc_send(env->env_parent_id, status, 0, 0);
		//debugf("3\n");
	}
	syscall_env_destroy(0);

	user_panic("unreachable code");
}

const volatile struct Env *env;
extern int main(int, char **);

void libmain(int argc, char **argv) {
	// set env to point at our env structure in envs[].
	env = &envs[ENVX(syscall_getenvid())];

	// call user main routine
	int status = main(argc, argv);
	//debugf("[%08x] return: %d\n", env->env_id, status);
	// exit gracefully
	exit(status, 1);
	//exit(0, 0);
}
