#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <stdbool.h>
#include <string>

#define DEFAULT_FILE_NAME "DISK";

void printUsageAndExit(){
    fprintf(stderr, "ssfs_mkdsk <num blocks> <block size> <disk file name>\n");
    exit(EXIT_FAILURE);            
}

/*
 * Write block size, num blocks, and initialize the inode map
 */
void setupDisk(FILE *diskFile, int numBlocks, int blockSize){
    char buffer[blockSize];
    
    //Write numBlocks and blockSize to buffer
    int bytesWritten = 0;
    memcpy(buffer, &numBlocks, sizeof(int));
    bytesWritten += sizeof(int);
    memcpy(buffer + bytesWritten, &blockSize, sizeof(int));
    bytesWritten += sizeof(int);

    int inodesLeft = 256;
    int8_t inodeInt = -1;
    bool stuffLeftInBuffer = false;

    //Set all 256 inodes to -1 to indicate they have no file in disk right now
    while(inodesLeft > 0){
        memcpy(buffer + bytesWritten, &inodeInt, sizeof(int8_t)); 
        bytesWritten += sizeof(int8_t);
        inodesLeft -= 1;
        stuffLeftInBuffer = true;

        //Buffer full - write to disk
        if(bytesWritten == blockSize){//block size guaranteed to be a power of 2 
            fwrite(buffer, blockSize, 1, diskFile); //Write buffer to disk
            memset(buffer, 0, blockSize); //clear buffer
            bytesWritten = 0;
            stuffLeftInBuffer = false;
        }
    }
    
    //Write what was left in the buffer if it wasn't full
    if(stuffLeftInBuffer)
        fwrite(buffer, bytesWritten, 1, diskFile);
    
    //Set up free block bitmap
    int8_t blockMap[numBlocks];
    int8_t EMPTY_BLOCK = -1;
    bytesWritten = 0;

    for(int i = 0; i < numBlocks; i++){
        memcpy(blockMap + bytesWritten, &EMPTY_BLOCK, sizeof(int8_t)); 
        bytesWritten += sizeof(int8_t);
    }

    fwrite(blockMap, numBlocks, 1, diskFile);
}

bool isPowerOfTwo(int num){
    return (num & -num) == num;
}

int main(int argc, char *argv[]){

    int numBlocks;
    int blockSize;
    std::string fileName;

    if(argc > 4 || argc < 3){
        fprintf(stderr, "Wrong number of arguments\n");
        printUsageAndExit();
    }

    numBlocks = atoi(argv[1]);
    if(numBlocks < 1024 || numBlocks > 128 * 1024 || !isPowerOfTwo(numBlocks)){
        fprintf(stderr, "The number of blocks must be a power of two between 1024 and 128K inclusive\n"); 
        printUsageAndExit();
    }

    blockSize = atoi(argv[2]);
    if(blockSize < 128 || blockSize > 512 || !isPowerOfTwo(blockSize)){
        fprintf(stderr, "The block size must be a power of two between 128 and 512 inclusive\n");
        printUsageAndExit();
    }

    if(argc == 4)
        fileName = argv[3];
    else
        fileName = DEFAULT_FILE_NAME; // use "DISK" if user does not supply file name

    FILE *diskFile = fopen(fileName.c_str(), "w");
    ftruncate(fileno(diskFile), numBlocks * blockSize);
    setupDisk(diskFile, numBlocks, blockSize);
    fclose(diskFile); 
    
    return 0;
}
