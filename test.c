#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<pthread.h>
#include<sys/ipc.h> 
#include<sys/shm.h> 
#include<unistd.h>
// #include<string.h>
#include"util.c"

#define QUEUE_SIZE 10
#define SHM_KEY 5678

FILE* openFile(char* filePath, char* fileOption){
    FILE *fp = fopen(filePath, fileOption);
    if (fp == NULL) {
        perror("Failed: ");
        return NULL;
    }
    
    return fp;
}

void closeFile(FILE* fp){
    if (fclose(fp))
        printf("Failed to close file: %hd\n", fp->_file);
}


int main(){
    char IP[256];
    dnslookup("facebook.com", IP, sizeof(IP));
    printf("%s\n", IP);
    FILE* results = openFile("/Users/liam/Documents/CUBoulderDocuments/2020_Spring/Operating_Systems/problem_sets/PA3/results.txt", "w");

    // if (strcmp("test", "TEMP") == 1)
    printf("%d", strcmp("test", "test"));

    closeFile(results);
    
    exit(0);
}