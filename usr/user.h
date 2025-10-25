struct stat;

// system calls
int fork(void);
int exit(void) __attribute__((noreturn));
int wait(void);
int pipe(int *);
int write(int, void *, int);
int read(int, void *, int);
int close(int);
int kill(int);
int exec(char *, char **);
int open(char *, int);
int mknod(char *, short, short);
int unlink(char *);
int fstat(int fd, struct stat *);
int link(char *, char *);
int mkdir(char *);
int chdir(char *);
int dup(int);
int getpid(void);
char *sbrk(int);
int sleep(int);
int uptime(void);
int ps(void); // calling ps() so that compilar know there exist ps command
int settickets(int pid, int n_tickets);
void srand(uint seed);
struct pstat;
int getpinfo(struct pstat *ps);

typedef uint pte_t;
uint pgpte(void *va);
int ugetpid(void);
void kpt(void);



int thread_create(uint* thread, void* (*start_routine)(void*), void* arg);
int thread_join(uint thread);
int thread_exit();

int waitpid(int);

int barrier_init(int);
int barrier_check(void);

int sleepChan(int);
int getChannel(void);
int sigChan(int);
int sigOneChan(int);

int xchg(volatile int *addr, int newval);

struct lock{
    volatile int lockvar;
    int isInitiated;
};

struct condvar{
    int var;
    int isInitiated;
};  

struct semaphore{
    int ctr;
    struct lock l;
    struct condvar cv;
    int isInitiated;
};



// ulib.c
int stat(char *, struct stat *);
char *strcpy(char *, char *);
void *memmove(void *, void *, int);
char *strchr(const char *, char c);
int strcmp(const char *, const char *);
void printf(int, char *, ...);
char *gets(char *, int max);
uint strlen(char *);
void *memset(void *, int, uint);
void *malloc(uint);
void free(void *);
int atoi(const char *);


void initiateLock(struct lock* l);
void acquireLock(struct lock* l);
void releaseLock(struct lock* l);
void initiateCondVar(struct condvar* cv);
void condWait(struct condvar* cv, struct lock* l);
void broadcast(struct condvar* cv);
void semInit(struct semaphore* s, int initVal);
void semUp(struct semaphore* s);
void semDown(struct semaphore* s);  


void signal(struct condvar *cv);