#include "multi-lookup.h"

/////////////////////////////////////////// Helper Functions /////////////////////////////////////////////////////
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
        printf("Failed to close file\n");
}

bool inFile(char* filePath, char* string){
    bool infile = false;
    char line_buffer[128];

    pthread_mutex_lock(&serviced_lock);
        FILE* file_pointer = openFile(filePath, "r");

        while(NULL != fgets(line_buffer, sizeof(line_buffer), file_pointer)){
        line_buffer[strcspn(line_buffer, "\n")] = 0; 
        if (!strcmp(line_buffer, string)) {
                infile = true;
                break;
            }
        }
        closeFile(file_pointer);
    pthread_mutex_unlock(&serviced_lock);
    return infile;
}

/////////////////////////////////////////// Queue Functions /////////////////////////////////////////////////////

void enqueue(char* domainName, queue* shm_data){
    pthread_mutex_lock(&shm_lock);

    while(shm_data->size == shm_data->capacity){
        pthread_cond_wait(&buffer_full, &shm_lock);
    }

    if (shm_data->size == 0){
        // pthread_mutex_lock(&shm_lock);
            strcpy(shm_data->domainBuffer[0], domainName);
            shm_data->head = 0;
            shm_data->tail = 0;
            shm_data->size += 1;
            pthread_cond_broadcast(&buffer_full);
        // pthread_mutex_unlock(&shm_lock);
    }

    else{
        // pthread_mutex_lock(&shm_lock); 
            shm_data->tail = (shm_data->tail + 1)%shm_data->capacity;
            strcpy(shm_data->domainBuffer[shm_data->tail], domainName);
            shm_data->size += 1;
            pthread_cond_broadcast(&buffer_full);
        // pthread_mutex_unlock(&shm_lock);
    }
    
    pthread_cond_broadcast(&buffer_full);
    pthread_mutex_unlock(&shm_lock);
}

char* dequeue(queue* shm_data){
    char *pop;
    // printf("dequeue\n");
    pthread_mutex_lock(&shm_lock); 
    // printf("%d", shm_data->size);

    if(shm_data->size == 0 ){
        pthread_cond_wait(&buffer_full, &shm_lock);
    }
    
    if(shm_data->tail == shm_data->head){
        pop = shm_data->domainBuffer[shm_data->head];
        shm_data->head = -1;
        shm_data->tail = -1;
        shm_data->size = 0;
        // pthread_mutex_unlock(&shm_lock); 
    }

    else{
        pop = shm_data->domainBuffer[shm_data->head];
        // pthread_mutex_lock(&shm_lock); 
        shm_data->head = (shm_data->head + 1) % shm_data->capacity; 
        shm_data->size -= 1;
    }

    pthread_cond_broadcast(&buffer_full);
    pthread_mutex_unlock(&shm_lock); 
    return pop;
}

///////////////////////////////////////////// Thread Functions /////////////////////////////////////////////////////
void* Requestor(void* shm_dataPointer){
    int shmid, i;
    int counter = 0;
    char buffer[128];
    char nextFile[256]; // these will be used as a temp space for the name files path and line contents

    data * shm_details = (data*) shm_dataPointer;
    queue* shm_data;

    if ((shmid = shmget(5678, sizeof(queue), 0666)) < 0){
        perror("Thread shmget failed: ");
        exit(1);
    }

    if ((shm_data = (queue *)shmat(shmid, NULL, 0)) == (queue*)-1){
        perror("Thread shmat failed: ");
        exit(1);
    }

    printf("Hello from requestor thread\n");

    for (i = 1; i < 6; i++){
        sprintf(nextFile, "../input/names%d.txt", i);
        if(!inFile("../serviced.txt", nextFile)){

            pthread_mutex_lock(&serviced_lock);
                FILE* serviced = openFile("../serviced.txt", "a");
                fprintf(serviced, "%s\n", nextFile);
                closeFile(serviced);
            pthread_mutex_unlock(&serviced_lock);

            FILE* nameFileP = openFile(nextFile, "r");
            while(NULL != fgets(buffer, sizeof(buffer), nameFileP)){
                buffer[strcspn(buffer, "\n")] = 0;
                enqueue(buffer, shm_data);
                // printf("REQUESTOR: Added %s\n", buffer);
            }
            closeFile(nameFileP);
            counter++;
        }
    }

    pthread_mutex_lock(&serviced_lock);
        FILE* serviced = openFile("../serviced.txt", "a");
        fprintf(serviced, "Thread YEET serviced %d files\n", counter);
        closeFile(serviced);
    pthread_mutex_unlock(&serviced_lock);

    pthread_mutex_lock(&shm_lock);
        shm_data->semephore -= 1;
    pthread_mutex_unlock(&shm_lock);

    pthread_cond_broadcast(&buffer_full);
    shmdt(shm_data);
    printf("Out of files. Thread exiting ...\n");

    return NULL; 
}

void* Resolver(void* shm_dataPointer){
    int shmid, i;
    char IP[256];
    char buffer[128]; // these will be used as a temp space for the name files path and line contents
    char filePath[256];
    bool flag = true;

    data * shm_details = (data*) shm_dataPointer;
    queue* shm_data;

    if ((shmid = shmget(5678, sizeof(queue), 0666)) < 0){
        perror("Resolver shmget failed: ");
        exit(1);
    }

    if ((shm_data = (queue *)shmat(shmid, NULL, 0)) == (queue*)-1){
        perror("Resolver shmat failed: ");
        exit(1);
    } 
    

    printf("Hello from resolver thread\n");   

    
    while(flag){
        pthread_mutex_lock(&shm_lock);
            if (shm_data->semephore <= 0 && shm_data->size <= 1)
                flag = false;
        pthread_mutex_unlock(&shm_lock);

        strcpy(buffer, dequeue(shm_data));
        dnslookup(buffer, IP, sizeof(IP));
        if (strcmp(IP, "UNHANDELED") == 0)
            strcpy(IP, "");
        // printf("RESOLVER: Removed %s\n", buffer);
        pthread_mutex_lock(&results_lock);
            FILE* results = openFile("../results.txt", "a");
                fprintf(results, "%s,%s\n", buffer, IP);
            closeFile(results);
        pthread_mutex_unlock(&results_lock);

        pthread_mutex_lock(&shm_lock);
            if (shm_data->semephore <= 0 && shm_data->size <= 1)
                flag = false;
        pthread_mutex_unlock(&shm_lock);
    }

    shmdt(shm_data);
    printf("Resolver thread exiting\n");
    return NULL;
}
///////////////////////////////////////////// Main Function //////////////////////////////////////////////////
int main(int argc, char *argv[]){
    printf("Hello, World\n"); 

    int shmid, i, j;
    int numRequestors = atoi(argv[1]);
    int numResolvers = atoi(argv[2]);
    queue* shm_data;
    pthread_t* requestors = malloc(sizeof(pthread_t)*numRequestors);
    pthread_t* resolvers = malloc(sizeof(pthread_t)*numResolvers);
    key_t key = 5678; 

    FILE* results = openFile("../results.txt", "w");
    FILE* serviced = openFile("../serviced.txt", "w");
    closeFile(results);
    closeFile(serviced);

    pthread_mutex_init(&shm_lock, NULL);
    pthread_mutex_init(&serviced_lock, NULL);
    pthread_mutex_init(&results_lock, NULL);
    pthread_cond_init(&buffer_full, NULL);

    if ((shmid = shmget(key, sizeof(queue), IPC_CREAT|0666)) < 0){
        perror("Main shmget failed: ");
        exit(1);
    }

    if ((shm_data = (queue *)shmat(shmid, NULL, 0)) == (queue*)-1){
        perror("Main shmat failed: ");
        exit(1);
    }

    shm_data->head = -1;
    shm_data->tail = -1;
    shm_data->size = 0;
    shm_data->semephore = numRequestors;
    shm_data->capacity = QUEUE_SIZE;

    for (i = 0; i < numRequestors; i++){
        // requestors[i] = (pthread_t) malloc(sizeof(pthread_t));
        if (pthread_create(&requestors[i], NULL, Requestor, NULL)) {
            perror("error creating requestor thread");
            exit(1);
        } 
    }

    for (i = 0; i < numResolvers; i++){
        // resolvers[i] = (pthread_t) malloc(sizeof(pthread_t));
        if (pthread_create(&resolvers[i], NULL, Resolver, NULL)) {
            perror("error creating requestor thread");
            exit(1);
        } 
    }

    // data details[numRequestors + numResolvers];
    // details[0].key = 5678;

    // pthread_t* requestor, *resolver;
    // requestor = (pthread_t *) malloc(sizeof(pthread_t));
    // if (pthread_create(requestor, NULL, Requestor, NULL)) {
    //     perror("error creating the first thread");
    //     exit(1);
    // }

    // resolver = (pthread_t *) malloc(sizeof(pthread_t));
    // if (pthread_create(resolver, NULL, Resolver, (void*)&details[0])) {
    //     perror("error creating the second thread");
    //     exit(1);
    // }

    for (i = 0; i < numRequestors; i++){
        pthread_join(requestors[i], 0);
    }

    for (i = 0; i < numResolvers; i++){    
        pthread_join(resolvers[i], 0);
    }

    free(requestors);
    free(resolvers);

    pthread_mutex_destroy(&shm_lock);
    pthread_mutex_destroy(&serviced_lock);
    pthread_mutex_destroy(&results_lock);
    pthread_cond_destroy(&buffer_full);	


    shmctl(shmid,IPC_RMID,NULL); 
    exit(0);
}

