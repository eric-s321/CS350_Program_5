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
		void list();
		void cat(string fileName);
        iNodeWithAddress* fileNameToInode(string fileName);
        int getFirstFreeBlock();
		void read(string fileName, int startByte, int numBytes);
        void write(string fileName, char letter, int startByte, int numBytes); 
        void import(string unixFileName);
		int getBlockIndirect(int address, int blockOffset);
		void deleteFile(string fileName);
		void freeBlock(int blockNum);
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

void DiskController::cat(string fileName){
	iNodeWithAddress *inodeWithAddress = this->fileNameToInode(fileName);
	int startByte = 0;
	iNode* inode = inodeWithAddress -> inode;
	int numBytes = inode -> size;
	read(fileName, startByte, numBytes);
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
    
    //Using iNode because we want the size of the struct!
    fwrite(inode, sizeof(iNode), 1, this->diskFile);
    cout << "writing to block " << freeBlockAddress << endl;

    this->inodeIndexMap[fileName] = inodeIndex; //Add new file to map
}

// Function freeBlock sets blockMap[blockNum] = -1
void DiskController::freeBlock(int blockNum){
	int freeBlock = -1;
	if(fseek(this->diskFile, FREE_BLOCK_START + blockNum * sizeof(int8_t), SEEK_SET) != 0){
		perror("FreeBlock fseek error: ");
		exit(EXIT_FAILURE);
	}
	if(fwrite(&freeBlock, sizeof(int8_t), 1, this->diskFile) != 1){
		perror("FreeBlock fwrite error: ");
		exit(EXIT_FAILURE);
	}
	// cout << "Freed Block " << blockNum << endl;
}

// Works -- need to check double indirect
void DiskController::deleteFile(string fileName){
	cout << "\nIN DELETE" <<endl;
	iNodeWithAddress *inodeWithAddress = this->fileNameToInode(fileName);
    if(inodeWithAddress != NULL) {
		iNode *inode = inodeWithAddress->inode;
		int totalBlocks = inode->size/this->blockSize;
		int indirectSize = this->blockSize/sizeof(int);
		int indirectAddr = inode->indirect;
		bool indirect = false;
		int iNodeBlockNum = 0;
		// cout << "totalBlocks: " << totalBlocks << endl;
		while(inode->size != 0 && totalBlocks >= 0) {
			int blockAddress = -1;
			if(indirect){
				// If double indirect get next indirect block
				if(iNodeBlockNum != 0 && iNodeBlockNum % indirectSize == 0){
					// Free previous indirect block
					// cout << "Indirect: ";
					this->freeBlock((indirectAddr - startingByte)/this->blockSize);
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
			
			// Go to Block Byte Address
			// cout << iNodeBlockNum << ": ";
			this->freeBlock((blockAddress - startingByte)/this->blockSize);
			
			totalBlocks--;
			iNodeBlockNum++;
			if (!indirect && iNodeBlockNum == 12) {
				indirect = true;
				iNodeBlockNum = 0;
			}
		}
		// Free last indirect block
		if(indirect){
			// cout << "Indirect: ";
			this->freeBlock((indirectAddr - startingByte)/this->blockSize);
		}
		// Free double indirect block
		if (inode->indirect2x != -1) {
			// cout << "Double Indirect: ";
			this->freeBlock((inode->indirect2x - startingByte)/this->blockSize);
		}
		// Free Block containing iNode
		// cout << "iNode: ";
		this->freeBlock((inodeWithAddress->address - startingByte)/this->blockSize);
		// Free iNode
		int freeBlock = -1;
		if(fseek(this->diskFile, INODE_START + this->inodeIndexMap[fileName] * sizeof(int), SEEK_SET) != 0){
			perror("FreeBlock fseek error: ");
			exit(EXIT_FAILURE);
		}
		if(fwrite(&freeBlock, sizeof(int), 1, this->diskFile) != 1){
			perror("FreeBlock fwrite error: ");
			exit(EXIT_FAILURE);
		}
		// Remove inode from map
		this->inodeIndexMap.erase(fileName);
	}
}

void DiskController::list(){
	int inodeAddress = INODE_START;

	for (int i = 0; i < 256; i++){
		if (fseek(this-> diskFile, inodeAddress, SEEK_SET) != 0){
			perror("fseek failed: ");
        		exit(EXIT_FAILURE);
		}
		int blockAddress;
		//read the inode address
		int result = fread(&blockAddress, sizeof(int), 1, this->diskFile);
		if(result != 1){
			perror("fread error: ");
			exit(EXIT_FAILURE);
		}
		//if inode exists
		if (blockAddress != -1){
			//seek to inode
			if(fseek(this->diskFile,blockAddress,SEEK_SET) != 0){
       				perror("fseek failed: ");
        			exit(EXIT_FAILURE);
    			}
			//read in inode
			iNode *inode = new iNode();
			result = fread(inode, sizeof(iNode), 1, this->diskFile);
			if(result != 1){
				perror("fread error: ");
				exit(EXIT_FAILURE);
			}
			char n[32];
			for (int i = 0; i < 32; i++){
				n[i] = inode -> name[i];
			}
			cout << n << endl;
		}
		inodeAddress += sizeof(int);
	}	
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
//
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
	if(fread(&blockAddress, sizeof(int), 1, this->diskFile) != 1){
		perror("Read fread error: ");
		exit(EXIT_FAILURE);
	}
	return blockAddress;
}

void DiskController::read(string fileName, int startByte, int numBytes){
	cout << "\nIN READ" << endl;
    //Should first check if inodeWithAddress is null before accessing inode attribute.
    //The function returns null if the name does not correspond to an existing inode
    iNodeWithAddress *inodeWithAddress = this->fileNameToInode(fileName);
    if (inodeWithAddress != NULL) {
		iNode *inode = inodeWithAddress->inode;
		int endByte = startByte + numBytes;
		// if(endByte> inode->size){
			// fprintf(stderr, "Read Error: cannot access byte %d in a file of size %d\n", endByte, inode->size);
			 // exit(EXIT_FAILURE);
		// }
		
		// ****Is this a better approach then giving an error if the last byte is too large?
		// If last byte to be read is larger than the file size then read upto last file byte
		int bytesLeft = endByte > inode->size? inode->size - startByte: numBytes;
		int iNodeBlockNum = startByte/this->blockSize;
		int blockByte = startByte%this->blockSize;
		int blockAddress = -1;
		// Set up for indirect blocks (and double indirect)
		int indirectAddr = inode->indirect;
		int indirectSize = this->blockSize/sizeof(int);
		bool indirect = iNodeBlockNum >= 12;
		if(indirect) iNodeBlockNum -= 12;
	/*
		 cout << "iNode size: " << inode->size << endl;
		 cout << "iNode direct: [";
		 for(int i=0; i<12; i++) {
		   if (i > 0) cout << ",";
		   cout << inode->direct[i];
		 }
		 cout << "]" << endl;
		 cout << "iNode indirect: " << inode->indirect << endl;
		 cout << "iNode dIndirect: " << inode->indirect2x << endl;
		 cout << "iNode block " << iNodeBlockNum << " at blockByte "<< blockByte << endl;
	*/
		
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
			
			// If stop reading in middle of block set numBytes to bytesLeft
			int size = blockByte + bytesLeft < this->blockSize? bytesLeft:this->blockSize - blockByte;
			char *str = (char *) calloc(size, sizeof(char));
			
			// Read Block
			if(fseek(this->diskFile, blockAddress + blockByte, SEEK_SET) != 0){
				perror("Read fseek error: ");
				exit(EXIT_FAILURE);
			}
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
			
	//		 cout <<endl; // TODO remove (new line every block)
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
    int bytesToWrite = numBytes;
	int indirectSize = this->blockSize/sizeof(int);
	int indirectAddr = inode->indirect;

    int blockNumber = (int) startByte / this->blockSize; 
    int byteNumber = startByte % this->blockSize;
    //If there is not enough space in the file get more space
    while(bytesToWrite > 0){
        cout << "bytes to write " << bytesToWrite << endl;
        cout << "Writing to block " << blockNumber << " at byte " << byteNumber << endl;
        cout << "SIZE IS " << inode->size << endl;

        //Check if the next place to write is already allocated
        int blockAddress;
			int indirectAddr;
        if(blockNumber < 12){//direct
            blockAddress = inode->direct[blockNumber];
        }
        //indirect
        else if(blockNumber > 12 && blockNumber <= this->blockSize / (int)sizeof(int) + 12){
            if(inode->indirect != -1)
                blockAddress = this->getBlockIndirect(inode->indirect, blockNumber - 12);
            else
                blockAddress = -1;
        }
        //double indirect
        else{
            /*int offset = ((blockNumber - 12) / this->blockSize / (int)sizeof(int)) - 1
            if(inode->indirect2x != -1) {
                blockAddress = this->getBlockIndirect(inode->indirect2x, offset);
                if(blockAddress == -1){
                    //allocate 
                    freeBlockAddress = this->getFirstFreeBlock();
							if(fseek(this->diskFile, inode->indirect2x + offset * sizeof(int), SEEK_SET) != 0){
								 perror("fseek failed: ");
								 exit(EXIT_FAILURE);
							}
                    fwrite(&freeBlockAddress, sizeof(int), 1, this->diskFile);
							if(fseek(this->diskFile, freeBlockAddress, SEEK_SET) != 0){
								 perror("fseek failed: ");
								 exit(EXIT_FAILURE);
							}
							for(int i = 0; i < this->blockSize / (int)sizeof(int); i++){
						     int notUsed = -1;
						     fwrite(&notUsed, sizeof(int), 1, this->diskFile);
                   	}
                    blockAddress = freeBlockAddress;
                }
                blockAddress = this->getBlockIndirect(blockAddress, (blockNumber - 12) % 
                       (this->blockSize / (int)sizeof(int)));
            }*/
        }

        int writeSize = this->blockSize - byteNumber;
        if(bytesToWrite < this->blockSize)
            writeSize = bytesToWrite;

        //This block is already allocated - write to it
        if(blockAddress != -1){
            if(fseek(this->diskFile, blockAddress + byteNumber, SEEK_SET) != 0){
                perror("fseek failed: ");
                exit(EXIT_FAILURE);
            }
            cout << "Already allocated. Writing" << endl;
        }

        //Block not allocated get the space and allocate it
        else{
            freeBlockAddress = this->getFirstFreeBlock();
            bool indirect = true;
            // Find free block and add free block to inode
            
            for(int i = 0; i < 12; i++){
                if(inode->direct[i] == -1){
                    inode->direct[i] = freeBlockAddress; 
                    indirect = false;

                    //Write to free block
                    if(fseek(this->diskFile, freeBlockAddress, SEEK_SET) != 0){
                        perror("fseek failed: ");
                        exit(EXIT_FAILURE);
                    }
                    /*
                    cout << "Writing " << writeSize << " characters" << endl;
                    for(int i = 0; i < writeSize; i++){
                        fwrite(&letter, sizeof(char), 1, this->diskFile);
                    }
                    */
                    inode->size += writeSize;
                    cout << "Adding "<<writeSize << " to writesize " << endl;
                   // bytesToWrite -= writeSize;
                    break;
                }
            }
            
           if(indirect){
					//if (blockNumber)


               cout << "IN INDIRECT " << endl;
               int address;
               int directBlockAddress = freeBlockAddress;
               if(inode->indirect == -1){// indirect block does not already exist
                   inode->indirect = freeBlockAddress;
                   address = freeBlockAddress;
                   //Go to indirect block
                   if(fseek(this->diskFile, address, SEEK_SET) != 0){
                       perror("fseek failed: ");
                       exit(EXIT_FAILURE);
                   }

                   //Set all data block points in the indirect block to -1
                   for(int i = 0; i < this->blockSize / (int)sizeof(int); i++){
                       int notUsed = -1;
                       fwrite(&notUsed, sizeof(int), 1, this->diskFile);
                   }
                   directBlockAddress = this->getFirstFreeBlock();
               }
               else
                   address = inode->indirect;

			    cout << "Reading indirect block at address " << address << " (block " << (address - this->startingByte)/ this->blockSize << ")" << endl;
			    cout << "Reading DIRECT first block at address " << directBlockAddress << " (block " << (directBlockAddress - this->startingByte)/ this->blockSize << ")" << endl;
               //Go back to indirect block
               if(fseek(this->diskFile, address, SEEK_SET) != 0){
                   perror("fseek failed: ");
                   exit(EXIT_FAILURE);
               }

               
               //Move file pointer to first available data block in indirect block
					int count = -1;
               int dataUsed;
               do{
                   fread(&dataUsed, sizeof(int), 1, diskFile);
						count++;
               }while(dataUsed != -1);
					if(fseek(this->diskFile, address+count*sizeof(int), SEEK_SET) != 0){
                   perror("fseek failed: ");
                   exit(EXIT_FAILURE);
               }

               //Write the direct block address to use to indirect block
               fwrite(&directBlockAddress, sizeof(int), 1, this->diskFile);
               inode->size += writeSize;
               bytesToWrite -= writeSize;
               
               //Go to datablock to write to 
               if(fseek(this->diskFile, directBlockAddress, SEEK_SET) != 0){
                   perror("fseek failed: ");
                   exit(EXIT_FAILURE);
               }
           }
        }
        for(int i = 0; i < writeSize; i++){
            fwrite(&letter, sizeof(char), 1, this->diskFile);
        }
        bytesToWrite -= writeSize;

        blockNumber++;
        byteNumber = 0;
    }
    
/*
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
*/
    
    if(startByte + numBytes > inode->size)
        inode->size = startByte + numBytes;
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
    diskController->write("test", 'a', 0, 1546);
    //diskController->write("test", 'b', 1535, 20);
    //diskController->write("test", 'b', 1536, 10);
    diskController->read("test", 1531, 15);
//    diskController->write("test", 'b', 125,20);
//    diskController->read("test", 120, 145);

//    diskController->read(1);
//    diskController->import("test");
//    diskController->import("blah");
	diskController -> list();
	diskController->deleteFile("test");
	diskController->deleteFile("Eric");
	diskController -> list();
	
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
