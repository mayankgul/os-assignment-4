// Add necessary header files here
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "mmu.h"

#define N (8 * (1 << 20))

// Mappings from RISC-V terms to ARM
#ifndef PGSIZE
#define PGSIZE PTE_SZ
#endif

#ifndef MAXVA
#define MAXVA UADDR_SZ
#endif

#ifndef PTE_V
#define PTE_V PTE_TYPE
#endif

#ifndef PTE_R
#define PTE_R (AP_KUR << 4)
#endif

#ifndef PTE_W
#define PTE_W (1 << 4)
#endif

#ifndef PTE2PA
#define PTE2PA(pte) (PTE_ADDR(pte))
#endif

#ifndef PTE_FLAGS
#define PTE_FLAGS(pte) ((pte) & ((1 << PTE_SHIFT) - 1))
#endif

// Superpage round up (used by superpg_test())
#ifndef SUPERPGSIZE
#define SUPERPGSIZE (512 * PGSIZE)
#endif

#ifndef SUPERPGROUNDUP
#define SUPERPGROUNDUP(a) (((a) + SUPERPGSIZE - 1) & ~(SUPERPGSIZE - 1))
#endif

void print_pt();
void print_kpt();
void ugetpid_test();
void superpg_test();

int main(int argc, char *argv[])
{
  print_pt();
  ugetpid_test();
  print_kpt();
  superpg_test();
  printf(1, "pttest: all tests succeeded\n");
  exit();
}

char *testname = "";

void err(char *why)
{
  printf(1, "pttest: %s failed: %s, pid=%d\n", testname, why, getpid());
  exit();
}

void print_pte(uint va)
{
  pte_t pte = (pte_t)pgpte((void *)va);
  printf(1, "va 0x%x pte 0x%x pa 0x%x perm 0x%x\n", va, pte, (uint)PTE2PA(pte), (uint)PTE_FLAGS(pte));
}

void print_pt()
{
  printf(1, "print_pt starting\n");
  for (uint i = 0; i < 10; i++)
  {
    print_pte(i * PGSIZE);
  }
  uint top = MAXVA / PGSIZE;
  for (uint i = top - 10; i < top; i++)
  {
    print_pte(i * PGSIZE);
  }
  printf(1, "print_pt: OK\n");
}

void ugetpid_test()
{
  printf(1, "ugetpid_test starting\n");
  testname = "ugetpid_test";

  for (int i = 0; i < 64; i++)
  {
    int pid = fork();
    if (pid < 0)
    {
      err("fork failed");
    }
    else if (pid == 0)
    {
      if (getpid() != ugetpid())
        err("mismatched PID");
      exit();
    }
    else
    {
      wait();
    }
  }
  printf(1, "ugetpid_test: OK\n");
}

void print_kpt()
{
  printf(1, "print_kpt starting\n");
  kpt(); // Implement in vm.c to access kernel pagetable
  printf(1, "print_kpt: OK\n");
}

void supercheck(uint s)
{
  pte_t last_pte = 0;

  for (uint p = s; p < s + 512 * PGSIZE; p += PGSIZE)
  {
    pte_t pte = (pte_t)pgpte((void *)p);
    if (pte == 0)
      err("no pte");
    if ((uint)last_pte != 0 && pte != last_pte)
    {
      err("pte different");
    }
    if ((pte & PTE_V) == 0 || (pte & PTE_R) == 0 || (pte & PTE_W) == 0)
    {
      err("pte wrong");
    }
    last_pte = pte;
  }

  for (int i = 0; i < 512; i += PGSIZE)
  {
    *(int *)(s + i) = i;
  }

  for (int i = 0; i < 512; i += PGSIZE)
  {
    if (*(int *)(s + i) != i)
      err("wrong value");
  }
}

void superpg_test()
{
  int pid;

  printf(1, "superpg_test starting\n");
  testname = "superpg_test";

  char *end = sbrk(N);
  if (end == 0 || end == (char *)0xffffffffffffffff)
    err("sbrk failed");

  uint s = SUPERPGROUNDUP((uint)end);
  supercheck(s);
  if ((pid = fork()) < 0)
  {
    err("fork");
  }
  else if (pid == 0)
  {
    supercheck(s);
    exit();
  }
  else
  {
    int status = 0;
    wait();
    if (status != 0)
    {
      exit();
    }
  }
  printf(1, "superpg_test: OK\n");
}
