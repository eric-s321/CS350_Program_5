# Eric Scagnelli
# Matt Shapiro
# Ashley Yale

PROJECT = Prog5
TARGET = all
CC = g++
CFLAGS = -Wall -pthread

SRC = \
	ssfs_mkdsk.cpp \
	ssfs.cpp \
	Makefile

PROGS = \
	ssfs_mkdsk \
	ssfs

$(TARGET): $(PROGS)

ssfs_mkdsk: ssfs_mkdsk.o
	$(CC) $(CFLAGS) ssfs_mkdsk.o -o ssfs_mkdsk

ssfs_mkdsk.o: ssfs_mkdsk.cpp 
	$(CC) $(CFLAGS) -c ssfs_mkdsk.cpp -o ssfs_mkdsk.o 

ssfs: ssfs.o
	$(CC) $(CFLAGS) ssfs.o -o ssfs

ssfs.o:	ssfs.cpp
	$(CC) $(CFLAGS) -c ssfs.cpp -o ssfs.o


clean:
	rm $(PROGS) *.o


$(PROJECT).tar.gz \
dist:	$(SRC_FILES)
	tar --transform='flags=r;s|^|$(PROJECT)/|' -czvf $(PROJECT).tar.gz $(SRC_FILES)
	# mkdir $(PROJECT)
	# cp $(SRC_FILES) $(PROJECT)
	# tar -czvf $(PROJECT).tar.gz $(PROJECT)