#include "types.h"
#include "stat.h"
#include "user.h"

#define NLOOPS 10000000

void worker(int tickets, char name) {
  settickets(getpid(), tickets);

  int count = 0;
  for(int i=0; i<NLOOPS; i++) {
    count++;
    if(i % 2000000 == 0) {
      printf(1, "%c running, count=%d\n", name, count);
    }
  }

  printf(1, "%c finished with count=%d\n", name, count);
  exit();
}

int main(void) {
  if(fork() == 0) worker(1, 'A');   // process A: 1 ticket
  if(fork() == 0) worker(2, 'B');   // process B: 2 tickets
  if(fork() == 0) worker(4, 'C');   // process C: 4 tickets

  // parent waits for all
  while(wait() > 0);
  exit();
}

