#include <stdlib.h>
#include <stdio.h>
void thread();
int main(int argc, char** argv){
	if (argc > 6){
		printf("usage: ssfs <disk file name> thread1ops.txt thread2ops.txt thread3ops.txt");
		exit(0);
	}
	int i;
	String filename;
	for (i = 2; i < argc; i++){
		filename = argv[i];
		fopen();
		
	}
}

void threadops(FILE* inputFile){
	while (){

	}
}
