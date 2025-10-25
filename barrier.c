/*----------xv6 sync lab----------*/
#include "types.h"
#include "arm.h"
#include "spinlock.h"
#include "defs.h"
#include "barrier.h"
#include "param.h"
#include "proc.h"
//define any variables needed here


struct barrier {
    int N;              // Number of processes to wait for
    int count;          // Number of processes that have arrived
    struct spinlock lock;
};

struct barrier bar; // global barrier 

int
barrier_init(int n)
{
  //to be done
  initlock(&bar.lock,"barrier");
  acquire(&bar.lock);
  bar.N=n;
  bar.count=0;
  release(&bar.lock);
  return 0;
}

int
barrier_check(void)
{
  acquire(&bar.lock);
  bar.count++;
  if(bar.count<bar.N){
    sleep(&bar,&bar.lock);
  }
  else{
    wakeup(&bar);
  }
  release(&bar.lock);
  //to be done
  return 0;
}

/*----------xv6 sync lock end----------*/
