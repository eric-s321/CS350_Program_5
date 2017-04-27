#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(){

    FILE *fileptr = fopen("DISK", "r");
    fseek(fileptr, 0, SEEK_END);          
    int filelen = ftell(fileptr);            
    rewind(fileptr);
    char buffer[filelen];

    for(int i = 0; i < filelen; i++) {
        fread(buffer+i, 1, 1, fileptr); 
    }

    printf("buffer:\n%s\n", buffer);
    
    fclose(fileptr);
    return 0;
}
