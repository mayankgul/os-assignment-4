#include "types.h"
#include "arm.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "pstat.h"
#include "spinlock.h"
#include "barrier.h"
extern uint rseed;
extern int pgpte_kernel(struct proc *, void *);
extern void kpt(void);



// added this for error
// Add these at the top
// int thread_create(uint *tid_ptr, void (*func)(void*), void *arg);
// void threads_exit(void);
// int thread_join(int tid);

//

static int nextchan = 3; // start from a small number > 0 (avoid 0 which sometimes used)
extern struct {
    struct spinlock lock;
    struct proc proc[NPROC];
} ptable;

extern int settickets(int pid, int n);

int sys_fork(void)
{
    return fork();
}

int sys_exit(void)
{
    exit();
    return 0; // not reached
}

int sys_wait(void)
{
    return wait();
}

int sys_kill(void)
{
    int pid;

    if (argint(0, &pid) < 0)
    {
        return -1;
    }

    return kill(pid);
}

int sys_getpid(void)
{
    return proc->pid;
}

int sys_sbrk(void)
{
    int addr;
    int n;

    if (argint(0, &n) < 0)
    {
        return -1;
    }

    addr = proc->sz;

    if (growproc(n) < 0)
    {
        return -1;
    }

    return addr;
}

int sys_sleep(void) {
    int n;
    uint ticks0;

    if(argint(0, &n) < 0)
        return -1;

    acquire(&tickslock);
    ticks0 = ticks;

    // Set your custom tracking fields
    acquire(&ptable.lock);
    proc->sleepticks = n;
    proc->sleeptarget = ticks0 + n;
    release(&ptable.lock);

    while(ticks - ticks0 < n){
        if(proc->killed){
            release(&tickslock);
            return -1;
        }
        sleep(&ticks, &tickslock);
    }
    release(&tickslock);

    return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int sys_uptime(void)
{
    uint xticks;

    acquire(&tickslock);
    xticks = ticks;
    release(&tickslock);

    return xticks;
}

// return how

int sys_ps(void)
{

    return ps();
}

int sys_settickets(void)
{
    int pid, n;

    if (argint(0, &pid) < 0 || argint(1, &n) < 0)
        return -1;

    return settickets(pid, n);
}

int sys_srand(void)
{
    uint seed;

    if (argint(0, (int *)&seed) < 0)
        return -1;

    rseed = seed;
    return 0;
}

int sys_getpinfo(void)
{
    struct pstat kps;
    uint uva; // user virtual address (32-bit)

    if (argint(0, (int *)&uva) < 0)
        return -1;

    if (getpinfo(&kps) < 0)
        return -1;

    if (copyout(proc->pgdir, uva, (char *)&kps, sizeof(kps)) < 0)
        return -1;

    return 0;
}

int sys_pgpte(void)
{
    int va;

    if (argint(0, &va) < 0)
        return -1;

    return pgpte_kernel(proc, (void *)(uint)va);
}

int sys_ugetpid(void)
{
    return proc->pid;
}

int sys_kpt(void)
{
    kpt();
    return 0;
}

// adding sleepn
// int sys_sleepn(void) {
//   int n;
//   if(argint(0, &n) < 0) return -1;
//   acquire(&tickslock);
//   uint start = ticks;
//   release(&tickslock);
//
//   acquire(&ptable.lock);
//   proc->sleepticks = start;
//   proc->sleepticks_total = n;
//   sleep(proc, &ptable.lock);   // standard sleep
//   release(&ptable.lock);
//
//  return 0;
//}


//// New code goes here


extern int thread_create(uint *tid_ptr, void* (*func)(void*), void *arg);
extern void thread_exit(void);
extern int thread_join(uint tid);

// thread_create(uint*, void* (*func)(void*), void* arg)
int sys_thread_create(void) {
    uint tid_ptr, func, arg;
    if (argint(0, (int*)&tid_ptr) < 0)
        return -1;
    if (argint(1, (int*)&func) < 0)
        return -1;
    if (argint(2, (int*)&arg) < 0)
        return -1;
    return thread_create((uint*)tid_ptr, (void* (*)(void*))func, (void*)arg);
}

int sys_thread_exit(void) {
    thread_exit();
    return 0; // Will not reach here if thread handled correctly
}

// thread_join(uint)
int sys_thread_join(void) {
    uint tid;
    if (argint(0, (int*)&tid) < 0)
        return -1;
    return thread_join(tid);
}

// int thread_create(uint *tid_ptr, void (*func)(void*), void *arg);
// int sys_thread_create(void){
//     uint *tid_ptr;  // Pointer to user space where TID will be stored
//     void *func;     // Function pointer (entry point)
//     void *arg;      // Argument pointer for the function

//     // Retrieve arguments as pointers (4-byte addresses on 32-bit ARM)
//     if(argptr(0, (char**)&tid_ptr, sizeof(uint)) < 0) return -1;
//     if(argptr(1, (char**)&func, sizeof(void*)) < 0) return -1;
//     if(argptr(2, (char**)&arg, sizeof(void*)) < 0) return -1;

//     // Call the kernel implementation
//     return thread_create(tid_ptr, func, arg);
// }

// // void thread_exit(void);
// int sys_thread_exit(void){
//     // Call the kernel implementation
//     threads_exit();
//     return 0; // Not reached, but added for completeness
// }

// // int thread_join(int tid);
// int sys_thread_join(void){
//     int tid;

//     // Retrieve the thread ID to wait for
//     if(argint(0, &tid) < 0) {
//         return -1;
//     }

//     // Call the kernel implementation
//     return thread_join(tid);
// }


int sys_barrier_init(void)
{
    int n;
    if(argint(0,&n)<0){
        return -1;
    }
    return barrier_init(n);
}

int sys_barrier_check(void)
{
    return barrier_check();
}

int sys_waitpid(void)
{
  return -1;
}

int sys_sleepChan(void) {
    int ch;
    if(argint(0,&ch)<0) return -1;
    acquire(&ptable.lock);
    sleep((void*)(uint)ch,&ptable.lock);
    release(&ptable.lock);
    return 0;
}

int sys_getChannel(void) {
    int ch;
    acquire(&ptable.lock);
    ch=nextchan++;
    release(&ptable.lock);
    return ch;
}

int sys_sigChan(void) {
    int ch;
    if(argint(0,&ch)<0) return -1;
    acquire(&ptable.lock);
    wakeup((void*)(uint)ch);
    release(&ptable.lock);
    return 0;
}

int sys_sigOneChan(void) {
    int ch;
    if(argint(0,&ch)<0) return -1;
    acquire(&ptable.lock);

    struct proc *p;
    for(p=ptable.proc; p<&ptable.proc[NPROC];p++){
        if(p->state==SLEEPING && p->chan ==(void*)(uint)ch){
            p->state=RUNNABLE;
            p->chan=0;
            break;
        }
    }
    release(&ptable.lock);
    return 0;
}
