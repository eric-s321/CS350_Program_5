CC = gcc 
CFLAGS = -Wall -std=gnu99


all: ssfs_mkdsk

ssfs_mkdsk: ssfs_mkdsk.o
	$(CC) $(CFLAGS) ssfs_mkdsk.o -o ssfs_mkdsk

ssfs_mkdsk.o: ssfs_mkdsk.cpp
	$(CC) $(CFLAGS) -c ssfs_mkdsk.cpp -o ssfs_mkdsk.o 

clean:
	rm ssfs_mkdsk *.o
