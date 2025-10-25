#include "types.h"
#include "stat.h"
#include "user.h"


int main(int argc,char * argv[]){
    int t=uptime();
    printf(1,"Uptime : %d ticks (~%d seconds)\n",t,t/10);
    exit();
}
