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

//Needed to change to name to a char[] because C++ stores strings as pointers to heap space - this was messing up write and reads
//Moved named[] to the bottom because of padding issues
struct iNode{
	int direct[12];
	int indirect;
	int indirect2x;
	int size;
	char name[32];
};

struct iNodeWithAddress{
    iNode *inode;
    int address;
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
        iNodeWithAddress* fileNameToInode(string fileName);
        int getFirstFreeBlock();
		void read(string fileName, int startByte, int numBytes);
        void write(string fileName, char letter, int startByte, int numBytes); 
        void import(string unixFileName);
		void updateINode(iNode* inode);
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
    strcpy(inode->name, fileName.c_str());
    //inode->name = fileName;
    inode->size = 0;
    
    //Mark all data blocks and indirect blocks as unused
    for(int i = 0; i < 12; i++){
        inode->direct[i] = -1;
    }
    inode->indirect = -1;
    inode->indirect2x = -1;
    
    //Useing iNode because we want the size of the struct!
    fwrite(inode, sizeof(iNode), 1, this->diskFile);
    cout << "writing to block " << freeBlockAddress << endl;

    this->inodeIndexMap[fileName] = inodeIndex; //Add new file to map
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

//Move file pointer to the inode location of the filename and return the inode struct
iNodeWithAddress* DiskController::fileNameToInode(string fileName){
    if(this->inodeIndexMap.find(fileName) == this->inodeIndexMap.end()){//File not yet created 
        fprintf(stderr, "Error: file does not exist\n");
        return NULL;
    }

    int index = this->inodeIndexMap[fileName];
 	int inodeAddress = INODE_START + index * sizeof(int);

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
	
/*
	if(fseek(this->diskFile,blockAddress,SEEK_SET) != 0){
        perror("fseek failed: ");
        exit(EXIT_FAILURE);
    }
*/
	
    iNodeWithAddress *inodeWithAddress = new iNodeWithAddress();
    inodeWithAddress->inode = inode;
    inodeWithAddress->address = blockAddress;
	cout << "File \"" << fileName << "\" inode at block address: " << blockAddress << " (block " << (blockAddress - this->startingByte)/ this->blockSize << ")" << endl;

    return inodeWithAddress;
}

//Move file pointer to the first free block and sets that block as used in the free block byte map
//returns the byte of the block so it can be added to the inode
int DiskController::getFirstFreeBlock(){
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

    int freeBlockAddress = this->startingByte + blockIndex * this->blockSize;
    if(fseek(this->diskFile, freeBlockAddress, SEEK_SET) != 0){
        perror("fseek failed: ");
        exit(EXIT_FAILURE);
    }
    return freeBlockAddress;
}


void DiskController::read(string fileName, int startByte, int numBytes){
	cout << "\nIN READ" << endl;
	int endByte = startByte + numBytes;
	iNodeWithAddress *inodeWithAddress = this->fileNameToInode(fileName);
    iNode *inode = inodeWithAddress->inode;
	if(endByte > inode->size){
        fprintf(stderr, "Error: trying to read to byte %d in a file of size %d\n", startByte, inode->size);
    }
	else{
		int bytesLeft = numBytes;
		int blockIdx = startByte/this->blockSize;
		int blockByte = startByte%this->blockSize;
		int blockAddress = (blockIdx > 12? -1: inode->direct[blockIdx]) + blockByte;//TODO indirect and dIndirect blocks
	
		cout << "iNode size: " << inode->size << endl;
		// cout << "iNode direct: [";
		// for(int i=0; i<12; i++) {
			// if (i > 0) cout << ",";
			// cout << inode->direct[i];
		// }
		// cout << "]" << endl;
		cout << "iNode block " << blockIdx << " at blockByte "<< blockByte << endl;
		cout << "Reading first block at address " << blockAddress << " (block " << (blockAddress - this->startingByte)/ this->blockSize << ")" << endl;
		
		// ***Read by block
		// while(bytesLeft > 0) {
			// if(fseek(this->diskFile, blockAddress, SEEK_SET) != 0){
				// perror("fseek failed: ");
				// exit(EXIT_FAILURE);
			// }
			// int size = bytesLeft < this->blockSize? bytesLeft:this->blockSize - blockByte;
			// char *str = (char *) calloc(size, sizeof(char));
			// int result = fread(str, sizeof(char), size, this->diskFile);
			// if(result != size){
				// perror("fread error: ");
				// exit(EXIT_FAILURE);
			// }
			// cout << str;
			// free(str);
			// bytesLeft-=size;
			// blockIdx++;
			// blockAddress = blockIdx > 12? -1: inode->direct[blockIdx];//TODO indirect and dIndirect blocks
		// }
		
		// ***Read by char
		if(fseek(this->diskFile, blockAddress, SEEK_SET) != 0){
			perror("fseek failed: ");
			exit(EXIT_FAILURE);
		}
		while(bytesLeft > 0) {
			char c;
			int result = fread(&c, sizeof(char), 1, this->diskFile);
			if(result != 1){
				perror("fread error: ");
				exit(EXIT_FAILURE);
			}
			cout << c;
			blockByte++;
			bytesLeft--;
			if (blockByte == this->blockSize) {
				blockAddress = ++blockIdx > 12? -1: inode->direct[blockIdx];//TODO indirect and dIndirect blocks
				blockByte = 0;
				if(fseek(this->diskFile, blockAddress, SEEK_SET) != 0){
					perror("fseek failed: ");
					exit(EXIT_FAILURE);
				}
			}
		}
	}
	
	cout << endl;
}

//NOT FINISHED
void DiskController::write(string fileName, char letter, int startByte, int numBytes){
	cout << "\nIN WRITE" << endl;
    iNodeWithAddress *inodeWithAddress = this->fileNameToInode(fileName);
    if(inodeWithAddress == NULL)
        return;

    iNode *inode = inodeWithAddress->inode;
    if(inode->size < startByte){
        fprintf(stderr, "Error: trying to write to byte %d in a file of size %d\n", startByte, inode->size);
        return;
    }

    //NOT DONE *** what if part overwrite and part append?
    if(inode->size >= startByte + numBytes){ //Enough space to overwrite file 
		cout << "Overwrite File" << endl;
        // int freeBlockAddress = this->getFirstFreeBlock();
        // cout << "First free block is " << freeBlockAddress << endl;
    }

    else{ //Need to append
        cout << "Append file" << endl;
        int numBytesLeft = numBytes;
        int freeBlockAddress = this->getFirstFreeBlock();
        cout << "First free block at address " << freeBlockAddress << " (block " << (freeBlockAddress - this->startingByte)/ this->blockSize << ")" << endl;

        while(numBytesLeft > 0){
			// Find free block and add free block to inode
            for(int i = 0; i < 12; i++){
                if(inode->direct[i] == -1){
                    inode->direct[i] = freeBlockAddress; 
                    cout << "wrote new block address" << endl;
                    break;
                }
            }         
            //TODO if all direct blocks were full use the indirect block
            
            if(numBytesLeft >= this->blockSize){ //Can fill an entire block
				// NOTE: fwrite(ptr, size, count, stream) where count is the size of the array that ptr points to
                // fwrite(&letter, sizeof(char), this->blockSize, this->diskFile);
                cout << "Filling entire block " << endl;
				for (int i=0; i<this->blockSize; i++) {
					fwrite(&letter, sizeof(char), 1, this->diskFile);
				}
                numBytesLeft -= this->blockSize;
                inode->size += blockSize;
            }

            else{ 
                cout << "In the else writing " << numBytesLeft << " bytes" <<  endl;
				// NOTE: fwrite(ptr, size, count, stream) where count is the size of the array that ptr points to
                // fwrite(&letter, sizeof(char), numBytesLeft, this->diskFile);
				for (int i=0; i<numBytesLeft; i++) {
					fwrite(&letter, sizeof(char), 1, this->diskFile);
				}
                inode->size += numBytesLeft;
                numBytesLeft = 0;
            }
            freeBlockAddress = this->getFirstFreeBlock(); //Get the next free block address
        }
        
		// Update iNode 
		int iNodeAddress = inodeWithAddress->address;
		if(fseek(this->diskFile, iNodeAddress, SEEK_SET) != 0){
			perror("fseek failed: ");
			exit(EXIT_FAILURE);
		}
        fwrite(inode, sizeof(iNode), 1, this->diskFile);
    }
}

void DiskController::import(string unixFileName){
    if(this->inodeIndexMap.find(unixFileName) == this->inodeIndexMap.end()){//File does not exist already
        cout << "File not found "<< endl;
    }
    else{  //File already exists - overwrite 
        int index = this->inodeIndexMap[unixFileName];
        cout << "File exists and index is " << index << endl;
    }

}

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
    diskController->write("test", 'a', 0, 278);
    diskController->read("test", 118, 129);

//    diskController->read(1);
//    diskController->import("test");
//    diskController->import("blah");


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

