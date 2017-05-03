#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <string>
#include <fstream>
#include <map>
#include <iostream>
#include <queue>

#define INODE_START 2 * sizeof(int) 
#define FREE_BLOCK_START 258 * sizeof(int)

using namespace std;


struct command{
	string commandName;
	string fileName;
	string unixFileName;
	char charParameter;
	int startByte;
	int numBytes;
};


struct iNode{
	string name;
	int size;
	int direct[12];
	int indirect;
	int indirect2x;
};

void* diskOp(void* commandFile);

class DiskController{
        FILE *diskFile;
        int blockSize;
        int numBlocks;
        int startingByte;
        map<string, int>inodeIndexMap;
        int findStartingByte();

    public:
        DiskController(FILE *diskFile);
        void create(string fileName);
        void read(int idx);
};

DiskController::DiskController(FILE *diskFile){
    this->diskFile = diskFile;

    int blockSize;
    int numBlocks;

    int result;
    result = fread(&numBlocks, sizeof(int), 1, diskFile);
    if(result != 1){
        perror("fread error: ");
        exit(EXIT_FAILURE);
    }
    this->numBlocks = numBlocks;

    result = fread(&blockSize, sizeof(int), 1, diskFile);
    if(result != 1){
        perror("fread error: ");
        exit(EXIT_FAILURE);
    }
    this->blockSize = blockSize;

    this->startingByte = this->findStartingByte();
}

void DiskController::create(string fileName){
    if(fseek(this->diskFile,FREE_BLOCK_START,SEEK_SET) != 0){
        perror("fseek failed: ");
        exit(EXIT_FAILURE);
    }
    int8_t block;
    int blockIndex = -1;
    do{
        int result = fread(&block, sizeof(int8_t), 1, this->diskFile);
        if(result != 1){
            perror("fread error: ");
            exit(EXIT_FAILURE);
        }
        blockIndex++;
    }while(block != -1);

    int blockAddress = FREE_BLOCK_START + blockIndex * sizeof(int8_t); 
    fseek(this->diskFile,blockAddress,SEEK_SET);
    int8_t usingBlock = 1;
    fwrite(&usingBlock, sizeof(int8_t), 1, this->diskFile);
    
    fseek(this->diskFile,INODE_START,SEEK_SET);
    int inodeBlock;
    int inodeIndex = -1;
    do{
        int result = fread(&inodeBlock, sizeof(int), 1, this->diskFile);
        if(result != 1){
            perror("fread error: ");
            exit(EXIT_FAILURE);
        }
        inodeIndex++;
    }while(inodeBlock != -1);

    int inodeAddress = INODE_START + inodeIndex * sizeof(int); 
    int freeBlockAddress = this->startingByte + blockIndex * this->blockSize;
    fseek(this->diskFile,inodeAddress,SEEK_SET);
    fwrite(&freeBlockAddress, sizeof(int), 1, this->diskFile);

    fseek(this->diskFile, freeBlockAddress, SEEK_SET);
    iNode *inode = new iNode();
    inode->name = fileName;
    inode->size = 0;
    fwrite(inode, sizeof(inode), 1, this->diskFile);

	//TODO add filename and index to map
    inodeIndexMap[fileName] = inodeIndex;
    cout << "just mapped " << fileName << " to " << inodeIndexMap[fileName] << endl;

}

void DiskController::read(int idx){
 	int inodeAddress = INODE_START + idx * sizeof(int);

    if(fseek(this->diskFile,inodeAddress,SEEK_SET) != 0){
        perror("fseek failed: ");
        exit(EXIT_FAILURE);
    }
    int blockAddress;
	int result = fread(&blockAddress, sizeof(int), 1, this->diskFile);
	if(result != 1){
		perror("fread error: ");
		exit(EXIT_FAILURE);
	}

	if(fseek(this->diskFile,blockAddress,SEEK_SET) != 0){
        perror("fseek failed: ");
        exit(EXIT_FAILURE);
    }

	iNode *inode = new iNode();
	result = fread(inode, sizeof(iNode), 1, this->diskFile);
	if(result != 1){
		perror("fread error: ");
		exit(EXIT_FAILURE);
	}

	cout << "FileName: " << inode->name << endl; 
	cout << "FileSize: " << inode->size << endl; 

}

//PLACEHOLDER FOR NOW. STILLS NEEDS TO BE IMPLEMENTED
int DiskController::findStartingByte(){
    return FREE_BLOCK_START + this->numBlocks * sizeof(int8_t);
}

DiskController *diskController;   //Making global so it can be accessed by all threads
queue<struct command> waitingCommands;

int main(int argc, char** argv){

	if (argc > 6 || argc < 3){
		fprintf(stderr, "usage: ssfs <disk file name> thread1ops.txt thread2ops.txt thread3ops.txt\n");
		exit(EXIT_FAILURE);
	}
    
    char *diskFileName = argv[1];

    FILE *diskFile = fopen(diskFileName, "rb+"); 
    if(diskFile == NULL){
        perror("Error opening disk file: ");
		exit(EXIT_FAILURE);
    }

    diskController = new DiskController(diskFile);
    diskController->create("test");
    diskController->create("Eric");
	diskController->read(0);
    diskController->read(1);

//Commented this out because it was getting stuck while parsing. Didn't change anything here
/*
	int i;
	char *filename;
	pthread_t threads[4];
	for (i = 2; i < argc; i++){
		filename = argv[i];
		//send new thread to threadops
		void* v = (void*)filename;
		pthread_create(&threads[i-2],NULL,diskOp,v);
	}
	for (int i = 0; i < argc-2; i++){
		pthread_join(threads[i], NULL);
	}
*/
}

void* diskOp(void* commandFileName){
	char* cstring = (char*)commandFileName;
	printf("%s\n", cstring);
	ifstream commandFile(cstring);
	string commandString;
	while (!commandFile.eof()){
		struct command c;
		commandFile >> c.commandName;
		//prevents reading last line twice
		if (!commandFile.eof()){
			if (c.commandName.compare("CAT") == 0){
				commandFile >> c.fileName;
				waitingCommands.push(c);
			} else if (c.commandName.compare("CREATE") == 0){
				commandFile >> c.fileName;
				waitingCommands.push(c);
			} else if (c.commandName.compare("IMPORT") == 0){
				commandFile >> c.fileName;
				commandFile >> c.unixFileName;
				waitingCommands.push(c);
 			} else if (c.commandName.compare("DELETE") == 0){
				commandFile >> c.fileName;
				waitingCommands.push(c);
			} else if (c.commandName.compare("WRITE") == 0){
				commandFile >> c.fileName;
				commandFile >> c.charParameter;
				commandFile >> c.startByte;
				commandFile >> c.numBytes;
				waitingCommands.push(c);
			} else if (c.commandName.compare("READ") == 0){
				commandFile >> c.fileName;
				commandFile >> c.startByte;
				commandFile >> c.numBytes;
				waitingCommands.push(c);
			} else if (c.commandName.compare("LIST") == 0){
				waitingCommands.push(c);
			} else if (c.commandName.compare("SHUTDOWN") == 0){
				waitingCommands.push(c);
			}
		}
	}	
	return NULL;
}






