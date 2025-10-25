#include "types.h"
#include "stat.h"
#include "user.h"

int main(void) {
  int pid = fork();
  if(pid == 0) {
    settickets(getpid(), 1);
    for(int i=0; i<5; i++) {
      printf(1, "Child sleeping...\n");
      sleep(10);  // sleep 10 ticks
    }
    exit();
  } else {
    settickets(getpid(), 5);
    for(int i=0; i<200; i++) {
      printf(1, "Parent working...\n");
    }
    wait();
    exit();
  }
}

