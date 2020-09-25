#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>

// Place any necessary global variables here
struct timeval start, end;
int iter = 5000;

int main(int argc, char *argv[]){

	gettimeofday(&start, NULL);

	for(int i = 0; i < iter; i++) {
		if(fork() == 0) {
			exit(0);
		} else {
			wait(NULL);
		}
	}

	gettimeofday(&end, NULL);
	float cost = ((end.tv_sec - start.tv_sec) * 1000000) + (end.tv_usec - start.tv_usec);

	printf("Forks Performed: %d\n", iter);
	printf("Total Elapsed Time: %f microseconds\n", cost);
	printf("Average Time Per Fork: %f microseconds\n", (cost/iter));

	return 0;

}
