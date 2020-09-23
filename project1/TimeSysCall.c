#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

// Place any necessary global variables here
struct timeval start, end;
int iter = 100000;

int main(int argc, char *argv[]){

	gettimeofday(&start, NULL);
	for(int i = 0; i < iter; i++) {
		getpid();
	}
	gettimeofday(&end, NULL);

	float cost = ((end.tv_sec - start.tv_sec) * 1000000) + (end.tv_usec - start.tv_usec);

	printf("Syscalls Performed: %d\n", iter);
	printf("Total Elapsed Time: %d\n", cost);
	printf("Average Time Per Syscall: %d\n", (cost/iter));

	return 0;

}
