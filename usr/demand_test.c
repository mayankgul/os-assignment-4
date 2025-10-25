/* user/demand_test.c */
#include "types.h"
#include "stat.h"
#include "user.h"

#define PGSIZE 4096

int
main(int argc, char *argv[])
{
  int npages = 40;
  int i;
  char *base;

  // Grow the process heap by npages pages
  base = sbrk(npages * PGSIZE);
  if (base == (char*)-1) {
    printf(2, "sbrk failed\n");
    exit();
  }

  printf(1, "demand_test: sbrked %d pages starting at 0x%x\n", npages, base);

  // At this point, pages are not necessarily mapped yet (lazy).
  // Touch the first byte of each page — this should trigger a page fault
  // for on-demand allocation, which our kernel should handle.
  for (i = 0; i < npages; i++) {
    char *p = base + i * PGSIZE;
    p[0] = (char)i;                  // write -> triggers page fault if not mapped
    if (i % 8 == 0) printf(1, "touched page %d at va 0x%x\n", i, p);
  }

  // Verify contents
  int sum = 0;
  for (i = 0; i < npages; i++) sum += base[i * PGSIZE];

  printf(1, "sum of first bytes = %d (should be predictable)\n", sum);
  printf(1, "on-demand paging test: done\n");
  exit();
}
