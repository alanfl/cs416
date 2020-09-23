#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>

// Place any necessary global variables here
struct timeval start, end;
int iter = 100000;
int count = 1;

void handle_sigfpe(int signum){
	if(count < iter) {
		count++;
		return;
	} else {
		gettimeofday(&end, NULL);
		float cost = ((end.tv_sec - start.tv_sec) * 1000000) + (end.tv_usec - start.tv_usec);
		printf("Exceptions Ocurred: %d\n", iter);
		printf("Total Elapsed Time: %f microseconds\n", cost);
		printf("Average Time Per Exception: %f microseconds\n", (cost/iter));		
		exit(0);
	}
	return;
}

int main(int argc, char *argv[]){
	int x = 5;
	int y = 0;
	int z = 0;
	signal(SIGFPE, handle_sigfpe);
	gettimeofday(&start, NULL);
	z = x / y;
	return 0;

}
