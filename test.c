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



int main(){
    char IP[256];
    dnslookup("facebook.com", IP, sizeof(IP));

    printf("%s", IP);
    
    exit(0);
}