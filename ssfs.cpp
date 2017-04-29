#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <string>
#include <map>
#include <iostream>

using namespace std;

struct iNodes{
	string name;
	int size;
	int direct[12];
	int indirect;
	int indirect2x;
} iNode;

void* threadops(void* commandFile);

class DiskController{
    

};

int main(int argc, char** argv){
	int s;
	if (argc > 6 || argc < 3){
		fprintf(stderr, "usage: ssfs <disk file name> thread1ops.txt thread2ops.txt thread3ops.txt\n");
		exit(0);
	}
	int i;
	char *filename;
	for (i = 2; i < argc; i++){
		pthread_t p;
		filename = argv[i];
        cout << "filename is " << filename << endl;
		FILE *commandFile = fopen(filename, "r");
		//send new thread to threadops
		void* v = (void*)commandFile;
		s = pthread_create(&p,NULL,threadops,v);
	}
}

void* threadops(void* commandFile){
	FILE* f = (FILE*)commandFile;
	
	//while (fscanf(f, "%s") != EOF){
		
	//}
}
