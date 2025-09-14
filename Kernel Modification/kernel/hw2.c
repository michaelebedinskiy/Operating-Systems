#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h> 
#include <linux/sched.h>
#include <linux/sched/signal.h> 
#include <linux/pid.h>
#include <linux/capability.h>
#include <linux/rcupdate.h>
#include <linux/sched/task.h>

asmlinkage long sys_hello(void) {
 printk("Hello, World!\n");
 return 0;
}

asmlinkage long sys_set_sec(int sword, int midnight, int clamp, int duty, int isolate) {
	if (sword < 0 || midnight < 0 || clamp < 0 || duty < 0 || isolate < 0) {
		return -EINVAL;
	}
	if (!capable(CAP_SYS_ADMIN)) {
		return -EPERM;
	}
	unsigned char clr = (sword > 0) * 1 + (midnight > 0) * 2 + (clamp > 0) * 4 + (duty > 0) * 8 + (isolate > 0) * 16;
	current->clearance = clr;
	return 0;
}

asmlinkage long sys_get_sec(char clr) {
	int value = 0;
	switch(clr) {
		case 's':
			value = 1;
			break;
		case 'm':
			value = 2;
			break;
		case 'c':
			value = 4;
			break;
		case 'd':
			value = 8;
			break;
		case 'i':
			value = 16;
			break;
	}
	if (value == 0) {
		return -EINVAL;
	}
	if ((value & current->clearance) > 0) {
		return 1;
	}
	return 0;
}

asmlinkage long sys_check_sec(pid_t pid, char clr) {
	unsigned char clearances[] = {'s', 'm', 'c', 'd', 'i'};
	unsigned char pows[] = {1,2,4,8,16};
	int i;
	struct task_struct *procPcb;
	for(i = 0; i < sizeof(clearances) / sizeof(unsigned char); i++) {
		if(clearances[i] == (unsigned char) clr) {
			break;
		}
	}
	if(i == 5) {
		return -EINVAL;
	}
	
	rcu_read_lock();
	procPcb = pid_task(find_vpid(pid), PIDTYPE_PID);
	if(procPcb) {
		get_task_struct(procPcb);
	}
	rcu_read_unlock();
	if(!procPcb) {
		return -ESRCH;
	}
	if(!(pows[i] & current->clearance)) {
		put_task_struct(procPcb);
		return -EPERM;
	}
	
	int result = (procPcb->clearance & pows[i]) > 0;
	put_task_struct(procPcb);
	return result;
}

asmlinkage long sys_flip_sec_branch(int height, char clr) {
	
	unsigned char clearances[] = {'s', 'm', 'c', 'd', 'i'};
	unsigned char pows[] = {1,2,4,8,16};
	int i;
	if (height < 1) {
		return -EINVAL;
	}
	for(i = 0; i < 5; i++) {
		if(clearances[i] == (unsigned char) clr) {
			break;
		}
	}
	if(i == 5) {
		return -EINVAL;
	}
	if(!(current->clearance & pows[i])) {
		return -EPERM;
	}
	struct task_struct *p;
	struct task_struct *next_p;    
	long count = 0;
	rcu_read_lock();
	p = rcu_dereference(current->real_parent);
	rcu_read_unlock();
	while (p && height > 0) {
		get_task_struct(p);
        p->clearance ^= pows[i];
        count += ((p->clearance & pows[i]) != 0);
        rcu_read_lock();
        next_p = rcu_dereference(p->real_parent);
        rcu_read_unlock();
        
        put_task_struct(p);
        p = next_p;
		height -= 1;
	}
	
	return count;
}

