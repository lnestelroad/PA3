#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<pthread.h>
#include<sys/ipc.h> 
#include<sys/shm.h> 
#include<unistd.h>
// #include<string.h>
#include"util.c"

#ifndef MULTI_H
#define MULTI_H

#define QUEUE_SIZE 10
#define SHM_KEY 5678

typedef struct queue {
    int size;
    int head;
    int tail;
    int capacity;
    char domainBuffer[QUEUE_SIZE][256];
}queue;

typedef struct data {
    key_t key;
}data;

// Helper functions
FILE* openFile(char* filepath, char* fileOption);
void closeFile(FILE* fd);
bool inFile(char* filePath, char* string);

// Queue funtions
// bool isEmpty(queue* shm_data);
// bool isFull(queue* shm_data);
void enqueue(char* hostname, queue* shm_data);
char* dequeue(queue* shm_data);

// Requestor thread fuction
void* Requestor(void* details);
void* Resolver(void* details);

// Shared Memory Functions
pthread_mutex_t shm_lock;
pthread_cond_t buffer_full;



#endif


//                          Attaching shared memory to array of strings
// https://stackoverflow.com/questions/13334352/how-to-attach-an-array-of-strings-to-shared-memory-c

//                          Better way to get attached memory
// http://users.cs.cf.ac.uk/Dave.Marshall/C/node27.html
