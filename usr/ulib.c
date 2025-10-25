#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h"

char*
strcpy(char *s, char *t)
{
    char *os;
    
    os = s;
    while((*s++ = *t++) != 0)
        ;
    return os;
}

int
strcmp(const char *p, const char *q)
{
    while(*p && *p == *q)
        p++, q++;
    return (uchar)*p - (uchar)*q;
}

uint
strlen(char *s)
{
    int n;
    
    for(n = 0; s[n]; n++)
        ;
    return n;
}

void*
memset(void *dst, int v, uint n)
{
	uint8	*p;
	uint8	c;
	uint32	val;
	uint32	*p4;

	p   = dst;
	c   = v & 0xff;
	val = (c << 24) | (c << 16) | (c << 8) | c;

	// set bytes before whole uint32
	for (; (n > 0) && ((uint)p % 4); n--, p++){
		*p = c;
	}

	// set memory 4 bytes a time
	p4 = (uint*)p;

	for (; n >= 4; n -= 4, p4++) {
		*p4 = val;
	}

	// set leftover one byte a time
	p = (uint8*)p4;

	for (; n > 0; n--, p++) {
		*p = c;
	}

	return dst;
}

char*
strchr(const char *s, char c)
{
    for(; *s; s++)
        if(*s == c)
            return (char*)s;
    return 0;
}

char*
gets(char *buf, int max)
{
    int i, cc;
    char c;
    
    for(i=0; i+1 < max; ){
        cc = read(0, &c, 1);
        if(cc < 1)
            break;
        buf[i++] = c;
        if(c == '\n' || c == '\r')
            break;
    }
    buf[i] = '\0';
    return buf;
}

int
stat(char *n, struct stat *st)
{
    int fd;
    int r;
    
    fd = open(n, O_RDONLY);
    if(fd < 0)
        return -1;
    r = fstat(fd, st);
    close(fd);
    return r;
}

int
atoi(const char *s)
{
    int n;
    
    n = 0;
    while('0' <= *s && *s <= '9')
        n = n*10 + *s++ - '0';
    return n;
}

void*
memmove(void *vdst, void *vsrc, int n)
{
    char *dst, *src;
    
    dst = vdst;
    src = vsrc;
    while(n-- > 0)
        *dst++ = *src++;
    return vdst;
}

// Atomic exchange function for user space
int
xchg(volatile int *addr, int newval)
{
  int result;
  __asm__ volatile(
    "swp %0, %1, [%2]"
    : "=&r" (result)
    : "r" (newval), "r" (addr)
    : "memory");
  return result;
}


void initiateLock(struct lock* l) {
    if(!l) return;
    l->lockvar=0;
    l->isInitiated=1;
}

void acquireLock(struct lock* l) {
    if(!l || !l->isInitiated) return;
    while(xchg(&l->lockvar,1)!=0){
        //spin
    }
}

void releaseLock(struct lock* l) {
    if(!l || !l->isInitiated) return;
    //memory barrier if needed
    xchg(&l->lockvar,0);
}

void initiateCondVar(struct condvar* cv) {
    if(!cv) return;
    int ch=getChannel();
    if(ch<0) { cv->isInitiated=0; return;}
    cv->var=ch;
    cv->isInitiated=1;
}

void condWait(struct condvar* cv, struct lock* l) {
    if(!cv || !cv->isInitiated) return;
    if(!l || !l->isInitiated) return;

    releaseLock(l);
    sleepChan(cv->var);
    acquireLock(l);
}

void broadcast(struct condvar* cv) {
    if(!cv || !cv->isInitiated) return;
    sigChan(cv->var);
}

void signal(struct condvar* cv) {
    if(!cv || !cv->isInitiated) return;
    sigOneChan(cv->var);
}

void semInit(struct semaphore* s, int initVal) {
    if(!s) return;

    initiateLock(&s->l); //initalize the lock
    initiateCondVar(&s->cv); //initialize the conidtional variable
    s->ctr=initVal; // set the initial count
    s->isInitiated=1; //mark semaphore as initialized
}

void semUp(struct semaphore* s) {
    if(!s || !s->isInitiated) return;

    acquireLock(&s->l); //acquire lock to modify ctr
    s->ctr++; //increment counter
    if(s->ctr<=0){ //if there are threads waiting
        signal(&s->cv); //wake up exactly one thread
    }
    releaseLock(&s->l); //release lock
}

void semDown(struct semaphore* s) {
    if(!s || !s->isInitiated) return;

    acquireLock(&s->l); //acquire lock to modify ctr
    s->ctr--; //decrement counter
    if(s->ctr<0){ //if no rescources left
        condWait(&s->cv,&s->l); //wait on the CV
    }
    releaseLock(&s->l); //release lock after decrement 
}



// int getChannel(void) {
//   return syscall(SYS_getChannel);
// }

// int sleepChan(int ch) {
//   return syscall(SYS_sleepChan, ch);
// }

// int sigChan(int ch) {
//   return syscall(SYS_sigChan, ch);
// }

// int sigOneChan(int ch) {
//   return syscall(SYS_sigOneChan, ch);
// }