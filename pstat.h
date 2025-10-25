#ifndef _PSTAT_H_
#define _PSTAT_H_

#include "param.h"

struct pstat
{
    int inuse[NPROC];
    int pid[NPROC];
    int tickets[NPROC];
    int runticks[NPROC];
    int boostsleft[NPROC];
};

#endif
