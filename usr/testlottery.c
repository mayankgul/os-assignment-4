#include "types.h"
#include "stat.h"
#include "user.h"
//#include "proc.h"

int main(void) {
  int pid = fork();
  if(pid == 0) {
    settickets(getpid(), 10);  // child gets 10 tickets
    for(int i=0; i<1000000; i++) ; // busy loop
    printf(1, "Child finished\n");
  } else {
    settickets(getpid(), 1);   // parent gets 1 ticket
    for(int i=0; i<1000000; i++) ;
    printf(1, "Parent finished\n");
    wait();
  }
  exit();
}

