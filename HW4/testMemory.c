/*
 *
 *
 *	
 *	Source:
 *	https://www.csee.umbc.edu/courses/undergraduate/CMSC421/fall02/burt/projects/howto_add_systemcall.html
 *
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
 
#include <sys/syscall.h>
 
#ifndef __NR_claimedMemory
#define __NR_claimedMemory 353
#endif
#ifndef __NR_freeMemory
#define __NR_freeMemory 354
#endif

int main(){
	int i;
	float fragmentation;
	long usage = syscall(__NR_claimedMemory);
	long free = syscall(__NR_freeMemory);

	printf("Executing 10 tests now...\n");

	for (i=0; i<10; i++){
		fragmentation = (float)free / (float)usage;
		printf("--------------------------------------------------------\n");
		printf("The Claimed memory is: %lu\n", usage);
		printf("The Free memory is: %lu\n", free);
		printf("The Fragmentation suffered is: %f\n", fragmentation);
		printf("--------------------------------------------------------\n");
		sleep(1);
	}	


}
