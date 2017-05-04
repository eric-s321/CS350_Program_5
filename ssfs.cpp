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
		int getBlockIndirect(int address, int blockOffset);
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


int DiskController::getBlockIndirect(int address, int blockOffset){
	int blockAddress;
	if(fseek(this->diskFile, address + blockOffset*sizeof(int), SEEK_SET) != 0){
		perror("Read fseek error: ");
		exit(EXIT_FAILURE);
	}
	int result = fread(&blockAddress, sizeof(int), 1, this->diskFile);
	if(result != 1){
		perror("Read fread error: ");
		exit(EXIT_FAILURE);
	}
	return blockAddress;
}

void DiskController::read(string fileName, int startByte, int numBytes){
	cout << "\nIN READ" << endl;
//	iNode *inode = this->fileNameToInode(fileName)->inode;

    //Should first check if inodeWithAddress is null before accessing inode attribute.
    //The function returns null if the name does not correspond to an existing inode
    iNodeWithAddress *inodeWithAddress = this->fileNameToInode(fileName);
    if(inodeWithAddress == NULL)
        return;
    iNode *inode = inodeWithAddress->inode;
    
	// if(endByte > inode->size){
        // fprintf(stderr, "Read Error: cannot access byte %d in a file of size %d\n", endByte, inode->size);
		// exit(EXIT_FAILURE);
    // }


    // ****Is this a better approach then giving an error if the last byte is too large?
	// If last byte to be read is larger than the file size then read upto last file byte
	int bytesLeft = startByte + numBytes > inode->size? inode->size - startByte: numBytes;
	int iNodeBlockNum = startByte/this->blockSize;
	int blockByte = startByte%this->blockSize;
	int blockAddress = -1;
	int indirectAddr = inode->indirect;
	int indirectSize = this->blockSize/sizeof(int);
	bool indirect = false;

	// cout << "iNode size: " << inode->size << endl;
	// cout << "iNode direct: [";
	// for(int i=0; i<12; i++) {
		// if (i > 0) cout << ",";
		// cout << inode->direct[i];
	// }
	// cout << "]" << endl;
	// cout << "iNode block " << iNodeBlockNum << " at blockByte "<< blockByte << endl;
	
	// ***Read by block
	while(bytesLeft > 0){
		if(indirect){
			// If double indirect get next indirect block
			if(iNodeBlockNum != 0 && iNodeBlockNum % indirectSize == 0){
				// Get double indirect block offset
				int offset = (iNodeBlockNum/indirectSize) - 1; // subtract 1 for single indirect
				indirectAddr = this->getBlockIndirect(inode->indirect2x, offset);
			}
			// Get block from indirect block
			blockAddress = this->getBlockIndirect(indirectAddr, iNodeBlockNum % indirectSize);
		}
		else{
			blockAddress = inode->direct[iNodeBlockNum];
		}
		
		// TODO remove
		if (bytesLeft == numBytes) {
			cout << "Reading first block at address " << blockAddress << " (block " << (blockAddress - this->startingByte)/ this->blockSize << ")" << endl;
		}
		
		// Go to Block Byte Address
		if(fseek(this->diskFile, blockAddress + blockByte, SEEK_SET) != 0){
			perror("Read fseek error: ");
			exit(EXIT_FAILURE);
		}
		
		// If stop reading in middle of block set numBytes to bytesLeft
		int size = bytesLeft < this->blockSize? bytesLeft:this->blockSize - blockByte;
		
		// Read Block
		char *str = (char *) calloc(size, sizeof(char));
		int result = fread(str, sizeof(char), size, this->diskFile);
		if(result != size){
			perror("Read fread error: ");
			exit(EXIT_FAILURE);
		}
		// Output Block
		cout << str;
		
		// Update 
		free(str);
		bytesLeft-=size;
		iNodeBlockNum++;
		blockByte = 0;
		
		// If direct and have read 12th direct block then switch to indirect
		if (!indirect && iNodeBlockNum == 12) {
			indirect = true;
			iNodeBlockNum = 0;
		}
		
		// cout <<endl; // TODO remove (new line every block)
	}
	
	// ***Read by char
	// if(fseek(this->diskFile, blockAddress, SEEK_SET) != 0){
		// perror("Read fseek error: ");
		// exit(EXIT_FAILURE);
	// }
	// while(bytesLeft > 0) {
		// char c;
		// int result = fread(&c, sizeof(char), 1, this->diskFile);
		// if(result != 1){
			// perror("Read fread error: ");
			// exit(EXIT_FAILURE);
		// }
		// cout << c;
		// blockByte++;
		// bytesLeft--;
		// if (blockByte == this->blockSize) {
			// blockAddress = ++blockIdx > 12? -1: inode->direct[blockIdx];//TODO indirect and dIndirect blocks
			// blockByte = 0;
			// if(fseek(this->diskFile, blockAddress, SEEK_SET) != 0){
				// perror("fseek failed: ");
				// exit(EXIT_FAILURE);
			// }
		// }
	// }
	
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

    int freeBlockAddress;

    //If there is not enough space in the file get more space
    while(inode->size < startByte + numBytes){
        freeBlockAddress = this->getFirstFreeBlock();
		// Find free block and add free block to inode
        for(int i = 0; i < 12; i++){
            if(inode->direct[i] == -1){
                inode->direct[i] = freeBlockAddress; 
                inode->size += this->blockSize;
                break;
            }
        }
        
       //TODO if all direct blocks were full use the indirect block
       
    }
    
    //Now that the file has enough size write to it
    int blockNumber = (int) startByte / this->blockSize; 
    int byteNumber = startByte % this->blockSize;
    cout << "Writing to block " << blockNumber << " at byte " << byteNumber << endl;
    int startAddress = inode->direct[blockNumber] + byteNumber;
	if(fseek(this->diskFile, startAddress, SEEK_SET) != 0){
		perror("fseek failed: ");
		exit(EXIT_FAILURE);
	}
	for (int i=0; i<numBytes; i++) {
		fwrite(&letter, sizeof(char), 1, this->diskFile);
	}
    
	// Update iNode 
	int iNodeAddress = inodeWithAddress->address;
	if(fseek(this->diskFile, iNodeAddress, SEEK_SET) != 0){
		perror("fseek failed: ");
		exit(EXIT_FAILURE);
	}
    fwrite(inode, sizeof(iNode), 1, this->diskFile);
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
    diskController->write("test", 'a', 0, 128);
    diskController->read("test", 0, 128);
    diskController->write("test", 'b', 125,20);
    diskController->read("test", 120, 145);
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
