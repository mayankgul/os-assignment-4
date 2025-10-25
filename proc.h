#ifndef PROC_INCLUDE_
#define PROC_INCLUDE_

// Per-CPU state, now we only support one CPU
struct cpu
{
    uchar id;                  // index into cpus[] below
    struct context *scheduler; // swtch() here to enter scheduler
    volatile uint started;     // Has the CPU started?

    int ncli;   // Depth of pushcli nesting.
    int intena; // Were interrupts enabled before pushcli?

    // Cpu-local storage variables; see below
    struct cpu *cpu;
    struct proc *proc; // The currently-running process.
};

extern struct cpu cpus[NCPU];
extern int ncpu;

extern struct cpu *cpu;
extern struct proc *proc;

// PAGEBREAK: 17
//  Saved registers for kernel context switches. The context switcher
//  needs to save the callee save register, as usually. For ARM, it is
//  also necessary to save the banked sp (r13) and lr (r14) registers.
//  There is, however, no need to save the user space pc (r15) because
//  pc has been saved on the stack somewhere. We only include it here
//  for debugging purpose. It will not be restored for the next process.
//  According to ARM calling convension, r0-r3 is caller saved. We do
//  not need to save sp_svc, as it will be saved in the pcb, neither
//  pc_svc, as it will be always be the same value.
//
//  Keep it in sync with swtch.S
//
struct context
{
    // svc mode registers
    uint r4;
    uint r5;
    uint r6;
    uint r7;
    uint r8;
    uint r9;
    uint r10;
    uint r11;
    uint r12;
    uint lr;
};

enum procstate
{
    UNUSED,
    EMBRYO,
    SLEEPING,
    RUNNABLE,
    RUNNING,
    ZOMBIE
};

// Per-process state
struct proc
{
    uint sz;                    // Size of process memory (bytes)
    pde_t *pgdir;               // Page table
    char *kstack;               // Bottom of kernel stack for this process
    enum procstate state;       // Process state
    volatile int pid;           // Process ID
    // volatile int tid;
    struct proc *parent;        // Parent process
    struct trapframe *tf;       // Trap frame for current syscall
    struct context *context;    // swtch() here to run process
    void *chan;                 // If non-zero, sleeping on chan
    int killed;                 // If non-zero, have been killed
    struct file *ofile[NOFILE]; // Open files
    struct inode *cwd;          // Current directory
    char name[16];              // Process name (debugging)
    int syscall_count;          // Number of syscalls made Processes
    int tickets;
    int runticks;
    int boostsleft;
    int sleepticks;             //when process went to sleep
    int sleepticks_total;       //total ticks slep   
    int sleeptarget;
    int sleepleft;              // remaining ticks to sleep

      // Flag: 1 if this is a spawned thread (created by thread_create), 0 otherwise.
    int is_thread; 

    // Points to the main thread of the process group (the original proc that called fork).
    // For a main thread, this points to itself. For a spawned thread, this points to its main thread.
    struct proc *main_thread;

    // The base virtual address of this thread's dedicated user stack page.
    // Needed to free the page on thread_join. Only relevant if is_spawned_thread is 1.
    void *ustack_base;
    int thread_retval;
};

// int settickets(int pid, int n);
// int getpinfo(struct pstat *ps);

// int thread_create(uint *tid_ptr, void (*func)(void*), void *arg);
// void threads_exit(void);
// int thread_join(int tid);

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
#endif
