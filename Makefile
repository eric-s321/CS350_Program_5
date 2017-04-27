CC = g++
CFLAGS = -Wall -pthread -std=gnu99


all: ssfs_mkdsk ssfs

ssfs_mkdsk: ssfs_mkdsk.o
	$(CC) $(CFLAGS) ssfs_mkdsk.o -o ssfs_mkdsk

ssfs_mkdsk.o: ssfs_mkdsk.cpp 
	$(CC) $(CFLAGS) -c ssfs_mkdsk.cpp -o ssfs_mkdsk.o 

ssfs: ssfs.o
	$(CC) $(CFLAGS) ssfs.o -o ssfs

ssfs.o:	ssfs.cpp
	$(CC) $(CFLAGS) -c ssfs.cpp -o ssfs.o


clean:
	rm ssfs_mkdsk *.o
