#include "types.h"
#include "user.h"

int 
main(int argc, char *argv[]) //argc tells abot the number of arguments and argv is a argument vector
{
	if(argc<2){ //if the count is less than 2 i.e no seconds provided
		printf(1, "Usage: pause <seconds>\n"); //so we print that seconds are to be given
		exit();
	}
	//the second argument is the number of seconds 
	int sec=atoi(argv[1]); //changing the string to integer
	if(sec<=0){ // seconds cannot be negative
		printf(1, "pause: seconds must be positive\n");
		exit();
	}
	printf(1,"Pausing for %d seconds\n",sec);
	sleep(sec*10); //10 ticks = 1 sec
	printf(1,"Done waiting %d seconds\n",sec);
	exit(); //exit
}
