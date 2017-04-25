#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#define DEFAULT_FILE_NAME "DISK"

void printUsageAndExit(){
    fprintf(stderr, "ssfs_mkdsk <num blocks> <block size> <disk file name>\n");
    exit(EXIT_FAILURE);            
}

int main(int argc, char *argv[]){

    int numBlocks;
    int blockSize;
    char *fileName;

    if(argc > 4 || argc < 3){
        fprintf(stderr, "Wrong number of arguments\n");
        printUsageAndExit();
    }

    numBlocks = atoi(argv[1]);
    if(numBlocks < 1024 || numBlocks > 128 * 1024){
        fprintf(stderr, "The number of blocks must be between 1024 and 128K inclusive\n"); 
        printUsageAndExit();
    }

    blockSize = atoi(argv[2]);
    if(blockSize < 128 || blockSize > 512){
        fprintf(stderr, "The block size must be between 128 and 512 inclusive\n");
        printUsageAndExit();
    }

    if(argc == 4)
        fileName = argv[3];
    else
        fileName = DEFAULT_FILE_NAME; // use "DISK" if user does not supply file name

    FILE *diskFile = fopen(fileName, "w");
    ftruncate(fileno(diskFile), numBlocks * blockSize);
    fclose(diskFile); 
    
    return 0;
}
