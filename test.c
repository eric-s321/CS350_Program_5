#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(){
    
    char buff[16];
    int i = 14;
    int j;

    memcpy(buff, &i, sizeof(int));
    memcpy(&j, buff, sizeof(int));

    if (i == j)
       printf("IT WORKED\n"); 

    return 0;
}
