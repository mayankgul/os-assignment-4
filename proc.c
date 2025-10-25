#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "arm.h"
#include "proc.h"
#include "spinlock.h"
#include "pstat.h"

#define RAND_MAX 0x7fffffff
uint rseed = 0;

uint rand()
{
    return rseed = (rseed * 1103515245 + 12345) & RAND_MAX;
}

//
// Process initialization:
// process initialize is somewhat tricky.
//  1. We need to fake the kernel stack of a new process as if the process
//     has been interrupt (a trapframe on the stack), this would allow us
//     to "return" to the correct user instruction.
//  2. We also need to fake the kernel execution for this new process. When
//     swtch switches to this (new) process, it will switch to its stack,
//     and reload registers with the saved context. We use forkret as the
//     return address (in lr register). (In x86, it will be the return address
//     pushed on the stack by the process.)
//
// The design of context switch in xv6 is interesting: after initialization,
// each CPU executes in the scheduler() function. The context switch is not
// between two processes, but instead, between the scheduler. Think of scheduler
// as the idle process.
// //
struct
{
    struct spinlock lock;
    struct proc proc[NPROC];
} ptable;



static struct proc *initproc;
struct proc *proc;

//static int total_tickets =0; //keep track of tickets in lottery scheduler

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void pinit(void)
{
    initlock(&ptable.lock, "ptable");
}

// PAGEBREAK: 32
//  Look in the process table for an UNUSED proc.
//  If found, change state to EMBRYO and initialize
//  state required to run in the kernel.
//  Otherwise return 0.
static struct proc *allocproc(void)
{
    struct proc *p;
    char *sp;

    acquire(&ptable.lock);

    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
        if (p->state == UNUSED)
        {
            goto found;
        }
    }

    release(&ptable.lock);
    return 0;

    found:
    p->state = EMBRYO;
    p->pid = nextpid++;
    // p->tickets = 1;  //giving the value
    // p->boostsleft = 0;
    // p->sleepticks = 0;
    // p->sleepticks_total = 0;
    release(&ptable.lock);

    // Allocate kernel stack.
    if ((p->kstack = alloc_page()) == 0)
    {
        p->state = UNUSED;
        return 0;
    }

    sp = p->kstack + KSTACKSIZE;

    // Leave room for trap frame.
    sp -= sizeof(*p->tf);
    p->tf = (struct trapframe *)sp;

    // Set up new context to start executing at forkret,
    // which returns to trapret.
    sp -= 4;
    *(uint *)sp = (uint)trapret;

    sp -= 4;
    *(uint *)sp = (uint)p->kstack + KSTACKSIZE;

    sp -= sizeof(*p->context);
    p->context = (struct context *)sp;
    memset(p->context, 0, sizeof(*p->context));

    // skip the push {fp, lr} instruction in the prologue of forkret.
    // This is different from x86, in which the harderware pushes return
    // address before executing the callee. In ARM, return address is
    // loaded into the lr register, and push to the stack by the callee
    // (if and when necessary). We need to skip that instruction and let
    // it use our implementation.
    p->context->lr = (uint)forkret + 4;

    if(proc->parent){
        p->tickets = proc->parent->tickets;
    }
    
    else{
        p->tickets = 1; // first process

    }
    
    p->boostsleft = 0;    // no initial boost
    // p->tickets = 0;
    // p->runticks = 0;
    // p->boostsleft = 0;
    // p->is_spawned_thread = 0;   // Default: it's a normal process
    // p->main_thread = p;         // Points to itself by default
    // p->user_stack_base = 0;     // Only set for threads later

    return p;
}

void error_init()
{
    panic("failed to craft first process\n");
}

// PAGEBREAK: 32
//  hand-craft the first user process. We link initcode.S into the kernel
//  as a binary, the linker will generate __binary_initcode_start/_size
void userinit(void)
{
    struct proc *p;
    extern char _binary_initcode_start[], _binary_initcode_size[];

    p = allocproc();
    initproc = p;

    if ((p->pgdir = kpt_alloc()) == NULL)
    {
        panic("userinit: out of memory?");
    }

    inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);

    p->sz = PTE_SZ;

    // craft the trapframe as if
    memset(p->tf, 0, sizeof(*p->tf));

    p->tf->r14_svc = (uint)error_init;
    p->tf->spsr = spsr_usr();
    p->tf->sp_usr = PTE_SZ; // set the user stack
    p->tf->lr_usr = 0;

    // set the user pc. The actual pc loaded into r15_usr is in
    // p->tf, the trapframe.
    p->tf->pc = 0; // beginning of initcode.S

    safestrcpy(p->name, "initcode", sizeof(p->name));
    p->cwd = namei("/");

    p->state = RUNNABLE;

    // p->tickets = 1;
    // p->runticks = 0;
    // p->boostsleft = 0;
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n)
{
    uint sz;

    sz = proc->sz;

    if (n > 0)
    {
        if ((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
        {
            return -1;
        }
    }
    else if (n < 0)
    {
        if ((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
        {
            return -1;
        }
    }

    proc->sz = sz;
    switchuvm(proc);

    return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int fork(void)
{
    int i, pid;
    struct proc *np;

    // Allocate process.
    if ((np = allocproc()) == 0)
    {
        return -1;
    }

    // Copy process state from p.
    if ((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0)
    {
        free_page(np->kstack);
        np->kstack = 0;
	// np->tickets = proc->tickets;   // child inherits parent's tickets
        // np->tickets = proc->tickets;
	    np->state = UNUSED;
        return -1;
    }

    np->sz = proc->sz;
    np->parent = proc;
    *np->tf = *proc->tf;

    // Clear r0 so that fork returns 0 in the child.
    np->tf->r0 = 0;

    for (i = 0; i < NOFILE; i++)
    {
        if (proc->ofile[i])
        {
            np->ofile[i] = filedup(proc->ofile[i]);
        }
    }

    np->cwd = idup(proc->cwd);

    pid = np->pid;
    np->state = RUNNABLE;
    safestrcpy(np->name, proc->name, sizeof(proc->name));

    // np->tickets = proc->tickets > 0 ? proc->tickets : 1;
    // np->runticks = 0;
    // np->boostsleft = 0;

    return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void exit(void)
{
    struct proc *p;
    int fd;
    
    if (proc == initproc)
    {
        panic("init exiting");
    }

    // Close all open files.
    for (fd = 0; fd < NOFILE; fd++)
    {
        if (proc->ofile[fd])
        {
            fileclose(proc->ofile[fd]);
            proc->ofile[fd] = 0;
        }
    }

    iput(proc->cwd);
    proc->cwd = 0;

    acquire(&ptable.lock);

    // struct proc *t;
    // // for(t = ptable.proc; t < &ptable.proc[NPROC]; t++) {
    // //     if(t->main_thread == proc && t->is_spawned_thread && t->state != ZOMBIE) {
    // //         sleep(proc, &ptable.lock);
    // //     }
    // // }
    // while (1) {
    // int all_done = 1;
    // for(t = ptable.proc; t < &ptable.proc[NPROC]; t++) {
    //     if(t->main_thread == proc && t->is_spawned_thread && t->state != ZOMBIE) {
    //         all_done = 0;
    //         break;
    //     }
    // }
    // if (all_done)
    //     break;
    // sleep(proc, &ptable.lock);
    // }



    // Parent might be sleeping in wait().
    wakeup1(proc->parent);

    // Pass abandoned children to init.
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
        if (p->parent == proc)
        {
            p->parent = initproc;

            if (p->state == ZOMBIE)
            {
                wakeup1(initproc);
            }
        }
    }

    // Jump into the scheduler, never to return.
    proc->state = ZOMBIE;
    sched();

    panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(void)
{
    struct proc *p;
    int havekids, pid;

    acquire(&ptable.lock);

    for (;;)
    {
        // Scan through table looking for zombie children.
        havekids = 0;

        for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
        {
            if (p->parent != proc || p->is_thread)
                continue;  // skip spawned threads

            havekids = 1;

            if (p->state == ZOMBIE)
            {
                // Found one.
                pid = p->pid;
                free_page(p->kstack);
                p->kstack = 0;
                freevm(p->pgdir);
                p->state = UNUSED;
                p->pid = 0;
                p->parent = 0;
                p->name[0] = 0;
                p->killed = 0;
                release(&ptable.lock);

                return pid;
            }
        }

        // No point waiting if we don't have any children.
        if (!havekids || proc->killed)
        {
            release(&ptable.lock);
            return -1;
        }

        // Wait for children to exit.  (See wakeup1 call in proc_exit.)
        sleep(proc, &ptable.lock); // DOC: wait-sleep
    }
}


// PAGEBREAK: 42
//  Per-CPU process scheduler.
//  Each CPU calls scheduler() after setting itself up.
//  Scheduler never returns.  It loops, doing:
//   - choose a process to run
//   - swtch to start running that process
//   - eventually that process transfers control
//       via swtch back to the scheduler.
/* Put this at file scope (outside any other function) */
struct proc* hold_lottery(int total_tickets) {
    if (total_tickets <= 0)
        return 0;

    int winning_ticket = rand() % total_tickets;
    int current = 0;

    struct proc *p;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->state != RUNNABLE)
            continue;

        int eff = p->tickets;
        if (p->boostsleft > 0) eff *= 2;  // same effective tickets logic
        current += eff;

        if (current > winning_ticket) {
            return p; // winner found
        }
    }

    return 0;
}









/* Replace your existing scheduler() with this */
void scheduler(void)
{
    struct proc *p;

    for (;;) {
        sti();                      // enable interrupts on this processor

        acquire(&ptable.lock);

        /* compute total effective tickets for this round */
        int total = 0;
        for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            if (p->state != RUNNABLE)
                continue;
            int eff = p->tickets;
            if (p->boostsleft > 0) eff *= 2; // boosted priority
            total += eff;
        }

        /* Choose a process by lottery */
        if(total>0){
            struct proc *winner = hold_lottery(total);
            if (winner != 0) {
                proc = winner;
                switchuvm(winner);
                winner->state = RUNNING;
                winner->runticks++;
                if(winner->boostsleft>0){
                    winner->boostsleft--;
                }
                /* switch to the chosen process */
                swtch(&cpu->scheduler, proc->context);

                /* coming back here after process yielded/exited/slept */
                // switchuvm(0);
                proc = 0;
            }
        }

        release(&ptable.lock);
    }
}


// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void sched(void)
{
    int intena;

    // show_callstk ("sched");

    if (!holding(&ptable.lock))
    {
        panic("sched ptable.lock");
    }

    if (cpu->ncli != 1)
    {
        panic("sched locks");
    }

    if (proc->state == RUNNING)
    {
        panic("sched running");
    }

    if (int_enabled())
    {
        panic("sched interruptible");
    }

    intena = cpu->intena;
    swtch(&proc->context, cpu->scheduler);
    cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void)
{
    acquire(&ptable.lock); // DOC: yieldlock
    proc->state = RUNNABLE;
    sched();
    release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void forkret(void)
{
    static int first = 1;

    // Still holding ptable.lock from scheduler.
    release(&ptable.lock);

    if (first)
    {
        // Some initialization functions must be run in the context
        // of a regular process (e.g., they call sleep), and thus cannot
        // be run from main().
        first = 0;
        initlog();
    }

    // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan, struct spinlock *lk)
{
    // show_callstk("sleep");

    if (proc == 0)
    {
        panic("sleep");
    }

    if (lk == 0)
    {
        panic("sleep without lk");
    }

    // Must acquire ptable.lock in order to change p->state and then call
    // sched. Once we hold ptable.lock, we can be guaranteed that we won't
    // miss any wakeup (wakeup runs with ptable.lock locked), so it's okay
    // to release lk.
    if (lk != &ptable.lock)
    {                          // DOC: sleeplock0
        acquire(&ptable.lock); // DOC: sleeplock1
        release(lk);
    }

    // Go to sleep.
    proc->chan = chan;
    // proc->sleepticks = ticks;   // record global ticks when process goes to sleep
    proc->state = SLEEPING;
    sched();

    // Tidy up.
    proc->chan = 0;

    // Reacquire original lock.
    if (lk != &ptable.lock)
    { // DOC: sleeplock2
        release(&ptable.lock);
        acquire(lk);
    }
}

// PAGEBREAK!
//  Wake up all processes sleeping on chan. The ptable lock must be held.
static void wakeup1(void *chan) {
    struct proc *p;

    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if(p->state != SLEEPING || p->chan != chan)
            continue;

        if(chan == &ticks) {
            // Wake only when requested sleep time has elapsed
            if(ticks >= p->sleeptarget) {
                // Give boosts = requested sleep duration
                p->boostsleft += p->sleepticks;

                p->state = RUNNABLE;

                // Reset sleep info
                p->sleepticks = 0;
                p->sleeptarget = 0;
            }
        } else {
            // For I/O channels, wake immediately
            p->state = RUNNABLE;
        }
    }
}

// Wake up all processes sleeping on chan.
void wakeup(void *chan)
{
    acquire(&ptable.lock);
    wakeup1(chan);
    release(&ptable.lock);
}

// Kill the process with the given pid. Process won't exit until it returns
// to user space (see trap in trap.c).
int kill(int pid)
{
    struct proc *p;

    acquire(&ptable.lock);

    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
        if (p->pid == pid)
        {
            p->killed = 1;

            // Wake process from sleep if necessary.
            if (p->state == SLEEPING)
            {
                p->state = RUNNABLE;
            }

            release(&ptable.lock);
            return 0;
        }
    }

    release(&ptable.lock);
    return -1;
}

// PAGEBREAK: 36
//  Print a process listing to console.  For debugging. Runs when user
//  types ^P on console. No lock to avoid wedging a stuck machine further.
void procdump(void)
{
    static char *states[] = {
        [UNUSED] "unused",
        [EMBRYO] "embryo",
        [SLEEPING] "sleep ",
        [RUNNABLE] "runble",
        [RUNNING] "run   ",
        [ZOMBIE] "zombie"};

    struct proc *p;
    char *state;

    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
        if (p->state == UNUSED)
        {
            continue;
        }

        if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
        {
            state = states[p->state];
        }
        else
        {
            state = "???";
        }

        cprintf("%d %s %s\n", p->pid, state, p->name);
    }

    show_callstk("procdump: \n");
}

// I am adding ps system call to display process info with system count
int ps(void)
{

    struct proc *p;
    //
    static char *states[] = {
        [UNUSED] "UNUSED",
        [EMBRYO] "EMBRYO",
        [SLEEPING] "SLEEPING",
        [RUNNABLE] "RUNNABLE",
        [RUNNING] "RUNNING",
        [ZOMBIE] "ZOMBIE"};
    acquire(&ptable.lock);
    cprintf("PID\tPPID\tNAME\t\tSTATE\t\tSYSTEMCALL\n");
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
        if (p->state == UNUSED)
            continue;
        cprintf("%d\t%d\t%s\t\t%s\t\t%d\n", p->pid, p->parent ? p->parent->pid : 0, p->name, states[p->state], p->syscall_count);
    }
    release(&ptable.lock);
    return 0;
}

int settickets(int pid, int n)
{
    struct proc *p;
    int ok=0;

    if (n <= 0)
        return -1;

    acquire(&ptable.lock);

    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
        if (p->pid == pid)
        {
            p->tickets = n;
            ok=1;
            break;
        }
    }

    release(&ptable.lock);

    return ok ? 0: -1;
}

int getpinfo(struct pstat *ps)
{
    struct proc *p;
    int i = 0;
    if (ps == 0)
        return -1;

    acquire(&ptable.lock);

    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++, i++)
    {
        ps->inuse[i] = (p->state != UNUSED);
        ps->pid[i] = p->pid;
        ps->tickets[i] = p->tickets;
        ps->runticks[i] = p->runticks;
        ps->boostsleft[i] = p->boostsleft;
    }
    release(&ptable.lock);
    return 0;
}


// thread_create(uint *tid_ptr, void (*func)(void*), void *arg)
// int
// thread_create(uint *tid_ptr, void (*func)(void*), void *arg)
// {
//     struct proc *np;
//     struct proc *curproc = proc;
//     uint new_stack_base, new_stack_top;
//     uint sp;
//     struct proc *mthr = curproc->main_thread; // Get the main process struct

//     // 1. Allocate process structure (gets ptable.lock inside)
//     if ((np = allocproc()) == 0) {
//         return -1;
//     }
    
//     // Acquire lock again as allocproc releases it
//     acquire(&ptable.lock);

//     // 2. Share memory and page tables
//     np->pgdir = mthr->pgdir; 
    
//     // Threads share file descriptors and CWD. Copy them from the main thread/group leader.
//     for(int i = 0; i < NOFILE; i++) {
//         if(mthr->ofile[i]) {
//             np->ofile[i] = filedup(mthr->ofile[i]);
//         }
//     }
//     np->cwd = idup(mthr->cwd);
    
//     // 3. Set up thread-specific fields
//     np->parent = mthr; // The main thread is the logical parent for joining
//     np->is_spawned_thread = 1;
//     np->main_thread = mthr;
//     safestrcpy(np->name, mthr->name, sizeof(mthr->name));

//     // 4. Allocate new user stack page (must increase the process size)
//     // The new stack is allocated at the current end of the process memory (mthr->sz)
//     new_stack_base = mthr->sz;
//     new_stack_top = new_stack_base + PTE_SZ;

//     // Use allocuvm to grow the memory address space by one page and map it
//     // Note: Use mthr->pgdir for allocuvm since all threads see the same page table.
//     if ((np->sz = allocuvm(mthr->pgdir, new_stack_base, new_stack_top)) == 0) {
//         // Cleanup if allocuvm fails
//         free_page(np->kstack);
//         np->kstack = 0;
//         np->state = UNUSED;
//         release(&ptable.lock);
//         return -1;
//     }

//     // Update the main thread's size (and the size of all other threads in the group)
//     mthr->sz = np->sz;
    
//     // Store the base address of this thread's new stack page for later cleanup (thread_join)
//     np->user_stack_base = new_stack_base;

//     // 5. Copy the trapframe from the current thread (or main thread)
//     *np->tf = *curproc->tf;
    
//     // 6. Set up the execution context to start at 'func' with 'arg'
//     // sp = new_stack_top; // Start stack pointer at the high end of the new page

//     // // PUSH 1: Push a fake return address (address of thread_exit, or 0) onto the stack. 
//     // // This is the function's return address. When func returns, it will jump here.
//     // // Since we don't have a direct user-space address for thread_exit, 
//     // // we push 0 and rely on user code to call exit/thread_exit.
//     // // However, if we MUST provide a safe return, we can push a stub that calls thread_exit.
//     // // For now, let's push 0, as the test case explicitly calls exit/thread_exit.
    
//     // // uint ra = 0;
//     // sp -= sizeof(uint); 

//     // uint ra = (uint)threads_exit; // safe return for user thread

//     // if (copyout(np->pgdir, sp, (char*)&ra, sizeof(ra)) < 0) {
//     //     goto fail;
//     // }

//     // // Set the new thread's stack pointer and program counter (entry point)
//     // np->tf->sp_usr = sp;
//     // np->tf->pc = (uint)func;
//     //     // *** FIX: Pass the single argument (arg) via register R0 (ARM EABI convention) ***
//     // np->tf->r0 = (uint)arg;
    
//     sp = new_stack_top;        // top of allocated stack page
//     sp -= sizeof(uint);              // reserve space for return address
//     uint ra = (uint)threads_exit;    // safe return address if func returns
//     copyout(np->pgdir, sp, (char*)&ra, sizeof(ra));
//     np->tf->sp_usr = sp;
//     np->tf->pc = (uint)func;
//     np->tf->r0 = (uint)arg;         // pass argument



//     // 7. Write the PID (TID) back to the user-provided address
//     int tid = np->pid;
//     if (copyout(curproc->pgdir, (uint)tid_ptr, (char*)&tid, sizeof(tid)) < 0) {
//         goto fail;
//     }

//     // 8. Set the new thread to runnable
//     np->state = RUNNABLE;

//     release(&ptable.lock);
//     return tid;

// fail:
//     // Rollback changes on failure
//     deallocuvm(np->pgdir, new_stack_base, new_stack_top);
//     mthr->sz = new_stack_base; // Rollback size
//     free_page(np->kstack);
//     np->kstack = 0;
//     np->state = UNUSED;
//     release(&ptable.lock);
//     return -1;
// }


int thread_create(uint *tid_ptr, void (*func)(void*), void *arg)
{
    struct proc *np;
    uint sp, stack_addr;

    // Allocate process struct for new thread
    if ((np = allocproc()) == 0)
        return -1;

    // Share address space (page table)
    np->pgdir = proc->pgdir;
    np->sz = proc->sz;

    // Set mainthread/main process pointer
    np->main_thread = proc->is_thread ? proc->main_thread : proc;
    np->is_thread = 1;

    // Prepare trapframe for new thread based on parent
    *np->tf = *proc->tf;

    // Allocate one user page aligned at the next page boundary
    stack_addr = PGROUNDUP(proc->sz);
    if (allocuvm(np->pgdir, stack_addr, stack_addr + PGSIZE) == 0) {
        np->state = UNUSED;
        return -1;
    }
    np->ustack_base = (void*)stack_addr;
    np->sz = stack_addr + PGSIZE;
    proc->sz += PGSIZE; // Ensure parent tracks new memory too

    // Initialize user stack pointer (top of stack page)
    sp = stack_addr + PGSIZE;

    // Place the argument value (pointer in parent’s address space)
    sp -= sizeof(void*);
    *(uint*)sp = (uint)arg;


    // Update thread trapframe
    np->tf->sp_usr = sp;
    np->tf->lr_usr = 0;                // no return to caller
    np->tf->pc = (uint)func;           // entry point
    np->tf->spsr = spsr_usr();         // enter user mode cleanly
    np->tf->r0 = (uint)arg;            // thread function receives arg in r0


    // All open files & cwd
    for (int i = 0; i < NOFILE; i++) {
        if (proc->ofile[i])
            np->ofile[i] = filedup(proc->ofile[i]);
    }
    np->cwd = idup(proc->cwd);

    // Ready
    np->parent = proc;
    np->state = RUNNABLE;

    // Assign thread id out
    if (copyout(proc->pgdir, (uint)tid_ptr, (void*)&(np->pid), sizeof(np->pid)) < 0)
        return -1;

    safestrcpy(np->name, proc->name, sizeof(np->name));
    return np->pid;
}

void thread_exit(void) {
    int fd;

    if (!proc->is_thread)
        return; // Only threads call thread_exit, else act as no-op

    // Close files
    for (fd = 0; fd < NOFILE; fd++) {
        if (proc->ofile[fd]) {
            fileclose(proc->ofile[fd]);
            proc->ofile[fd] = 0;
        }
    }
    iput(proc->cwd);
    proc->cwd = 0;

    acquire(&ptable.lock);
    // Wake up main thread if it's waiting in join
    wakeup1(proc->main_thread);

    // Mark as ZOMBIE, scheduler will free stack in join
    proc->state = ZOMBIE;
    sched();
    panic("zombie exit");
}

int thread_join(uint tid) {
    struct proc *p;
    int found = 0;

    acquire(&ptable.lock);
    for (;;) {
        found = 0;
        for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            if (p->pid == tid && p->main_thread == proc && p->is_thread) {
                found = 1;
                if (p->state == ZOMBIE) {
                    // Free stack
                    if (p->ustack_base)
                        deallocuvm(proc->pgdir, (uint)p->ustack_base + PGSIZE, (uint)p->ustack_base);

                    free_page(p->kstack);
                    p->state = UNUSED;
                    p->pid = 0;
                    p->parent = 0;
                    p->main_thread = 0;
                    p->name[0] = 0;
                    p->killed = 0;
                    release(&ptable.lock);
                    return tid;
                }
            }
        }
        if (!found || proc->killed) {
            release(&ptable.lock);
            return -1;
        }
        // Wait for thread to exit
        sleep(proc, &ptable.lock);
    }
}
















// int thread_create(uint *tid_ptr, void (*func)(void*), void *arg)
// {
//     struct proc *np;
//     struct proc *curproc = proc;
//     struct proc *mthr = curproc->main_thread; // main thread
//     uint new_stack_base, new_stack_top;
//     uint sp;

//     // 1. Allocate process structure
//     if ((np = allocproc()) == 0)
//         return -1;

//     acquire(&ptable.lock); // allocproc released it

//     // 2. Share memory
//     np->pgdir = mthr->pgdir;

//     // 3. Share file descriptors and cwd
//     for(int i = 0; i < NOFILE; i++)
//         if (mthr->ofile[i])
//             np->ofile[i] = filedup(mthr->ofile[i]);
//     np->cwd = idup(mthr->cwd);

//     // 4. Thread metadata
//     np->parent = mthr;
//     np->is_spawned_thread = 1;
//     np->main_thread = mthr;
//     safestrcpy(np->name, mthr->name, sizeof(mthr->name));

//     // 5. Allocate user stack (1 page)
//     new_stack_base = mthr->sz;
//     new_stack_top = new_stack_base + PTE_SZ;
//     if (allocuvm(mthr->pgdir, new_stack_base, new_stack_top) == 0)
//         goto fail;

//     np->user_stack_base = new_stack_base;
//     mthr->sz = new_stack_top; // update main thread size

//     // 6. Copy trapframe
//     *np->tf = *curproc->tf;

//     // 7. Set up user stack and program counter
//     // 7. Set up user stack and program counter
//     sp = np->user_stack_base + PTE_SZ;
//     sp -= sizeof(uint);         // reserve for return address
//     sp &= ~7;                   // 8-byte alignment
//     uint ra = (uint)threads_exit; // safe return
//     if (copyout(np->pgdir, sp, (char*)&ra, sizeof(ra)) < 0)
//         goto fail;

//     np->tf->sp_usr = sp;
//     np->tf->pc     = (uint)func;
//     np->tf->r0     = (uint)arg;
//     np->tf->r1 = np->tf->r2 = np->tf->r3 = 0;
//     // np->tf->r14_usr = 0;

//     // 8. Write TID back to user space
//     int tid = np->pid;
//     if (copyout(curproc->pgdir, (uint)tid_ptr, (char*)&tid, sizeof(tid)) < 0)
//         goto fail;

//     // 9. Make thread runnable
//     np->state = RUNNABLE;
//     release(&ptable.lock);

//     return tid;

// fail:
//     // Rollback on failure
//     deallocuvm(np->pgdir, new_stack_base, new_stack_top);
//     mthr->sz = new_stack_base;
//     free_page(np->kstack);
//     np->kstack = 0;
//     np->state = UNUSED;
//     release(&ptable.lock);
//     return -1;
// }





// // thread_exit()
// // void
// // threads_exit(void)
// // {
// //     struct proc *curproc = proc;

// //     // If this syscall is called from a main thread, the behaviour should be similar to a no-op.
// //     if (curproc->is_spawned_thread == 0) {
// //         // Main thread called thread_exit. No-op.
// //         return;
// //     }
    
// //     // A thread's exit does NOT free the shared page directory (pgdir).
    
// //     acquire(&ptable.lock);
    
// //     // Notify parent (main thread) that this thread is ready to be joined (reaped)
// //     // We use the thread's PID as the channel to wake up on.
// //     wakeup((void*)(uint)curproc->pid);

// //     // Jump into the scheduler, never to return.
// //     curproc->state = ZOMBIE;
// //     sched();

// //     // Should never reach here
// //     panic("zombie thread_exit");
// // }


// void
// threads_exit(void)
// {
//     struct proc *curproc = proc;

//     // If this syscall is called from a main thread, do nothing
//     if (curproc->is_spawned_thread == 0) {
//         return;
//     }

//     // Mark as ZOMBIE
//     curproc->state = ZOMBIE;

//     // Wake up the main thread that may be waiting in thread_join(tid)
//     wakeup((void*)(uint)curproc->pid);

//     // Yield to scheduler
//     sched();

//     panic("zombie thread_exit");
// }




// // thread_join(uint tidw)
// int thread_join(int tid)
// {
//     struct proc *p;
//     struct proc *curproc = proc;

//     if (curproc->is_spawned_thread)
//         return -1; // only main threads can join

//     if (curproc->pid == tid)
//         return -1; // cannot join self

//     acquire(&ptable.lock);

//     for (;;)
//     {
//         int found = 0;

//         for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
//         {
//             if (p->pid == tid && p->is_spawned_thread && p->main_thread == curproc)
//             {
//                 found = 1;
//                 if (p->state == ZOMBIE)
//                 {
//                     // Free user stack page
//                     if (p->user_stack_base != 0)
//                         deallocuvm(curproc->pgdir, p->user_stack_base, p->user_stack_base + PTE_SZ);

//                     // Free kernel stack
//                     free_page(p->kstack);
//                     p->kstack = 0;

//                     // Reset proc fields
//                     p->state = UNUSED;
//                     p->pid = 0;
//                     p->parent = 0;
//                     p->main_thread = 0;
//                     p->is_spawned_thread = 0;
//                     p->user_stack_base = 0;
//                     p->name[0] = 0;
//                     p->killed = 0;

//                     release(&ptable.lock);
//                     return tid;
//                 }
//             }
//         }

//         if (!found || curproc->killed)
//         {
//             release(&ptable.lock);
//             return -1;
//         }

//         sleep((void*)(uint)tid, &ptable.lock); // wait for thread exit
//     }
// }
