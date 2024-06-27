#include <env.h>
#include <lib.h>
void wait(u_int envid) {
	const volatile struct Env *e;

	e = &envs[ENVX(envid)];
	while (e->env_id == envid && e->env_status != ENV_FREE) {
		syscall_yield();
	}
}

int wait_return(u_int envid) {
	const volatile struct Env *e;

	e = &envs[ENVX(envid)];
	
	u_int who = 0;
	//debugf("i[%08x] can recv from [%08x]\n",syscall_getenvid(), envid);
	int status = ipc_recv(&who, 0, 0);
	if (who == envid) {
	 	//debugf("recv [%08x]'s : %d\n", envid, status);
		while (e->env_id == envid && e->env_status != ENV_FREE) {
			syscall_yield();
		}
	 	return status;
	} else {
		debugf("i: [%08x] recv not my son: [%08x], i want get [%08x]\n", e->env_id, who, envid);
		return -1;
	}
	
	
}