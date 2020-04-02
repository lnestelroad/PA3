#include "test.h"

#define QUEUE_SIZE 10
#define SHM_KEY 5678

FILE* openFile(char* filePath, char* fileOption){
    FILE *fp = fopen(filePath, fileOption);
    // printf("FILEPATH: %s", filePath);
    if (fp == NULL) {
        perror(filePath);
        exit(1);
    }
    return fp;
}

void closeFile(FILE* fp){
    if (fclose(fp)) printf("Failed to close file\n");
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

void enqueue(char* domainName, queue* domain){
    // printf("Hello from enqueue.\n");
    // printf("Head: %d, Tail: %d, Size: %d\n", domain->head, domain->tail, domain->size);
    // pthread_mutex_lock(&shm_lock);
        
        if (domain->head == -1){ 
            domain->head = 0;
            domain->tail = 0; 
            strcpy(domain->buffer[domain->tail], domainName); 
            domain->capacity += 1;

            pthread_cond_signal(&buffer_empty);
        } 
    
        else if (domain->tail == domain->size-1 && domain->head != 0){ 
            domain->tail = 0; 
            strcpy(domain->buffer[domain->tail], domainName); 
            domain->capacity += 1;
            // pthread_cond_signal(&buffer_full);
        } 
    
        else{ 
            domain->tail++; 
            strcpy(domain->buffer[domain->tail], domainName); 
            domain->capacity += 1;
            // pthread_cond_signal(&buffer_full);
        } 

    // pthread_mutex_unlock(&shm_lock);
}

void dequeue(queue* domain, char** data){
    // printf("Hello form dequeue.\n");

    // pthread_mutex_lock(&shm_lock);

    strcpy(*data, domain->buffer[domain->head]); 
    strcpy(domain->buffer[domain->head], "");
    domain->capacity -= 1;

    if (domain->head == domain->tail) { 
        domain->head = -1; 
        domain->tail = -1; 
    } 
    else if (domain->head == domain->size-1) 
        domain->head = 0; 
    else
        domain->head++; 

    if (domain->capacity == domain->size -1)
        pthread_cond_signal(&buffer_full);

    // pthread_mutex_unlock(&shm_lock);s
}

void displayQueue(queue* domain) { 
    pthread_mutex_lock(&shm_lock);
    if (domain->head == -1) 
    { 
        printf("\nQueue is Empty\n"); 
        return; 
    } 
    printf("\nElements in Circular Queue are: "); 
    if (domain->tail >= domain->head) 
    { 
        for (int i = domain->head; i <= domain->tail; i++) 
            printf("%s   ",domain->buffer[i]); 
    } 
    else
    { 
        for (int i = domain->head; i < domain->size; i++) 
            printf("%s   ", domain->buffer[i]); 
  
        for (int i = 0; i <= domain->tail; i++) 
            printf("%s   ", domain->buffer[i]); 
    } 
    printf("\n");
    pthread_mutex_unlock(&shm_lock);
} 

void* Requestor(void* threadData){
    int shmid, i;
    int counter = 0;
    char* buffer = NULL;
    size_t buff_len = 0;
    char nextFile[256]; // these will be used as a temp space for the name files path and line contents
    FILE* serviced;
    queue* shm_data;
    data * thread_detail = (data*) threadData;
    char* serviceFile = thread_detail->serviceFile;
    // printf("Hello from requestor thread. %s\n", thread_detail->serviceFile);

    if ((shmid = shmget(5678, sizeof(queue), 0666)) < 0){
        perror("Thread shmget failed: ");
        exit(1);
    }

    if ((shm_data = (queue *)shmat(shmid, NULL, 0)) == (queue*)-1){
        perror("Thread shmat failed: ");
        exit(1);
    }

    pthread_t self = pthread_self();

    for (i = 0; i < thread_detail->filesNum; i++){
        sprintf(nextFile, "%s", thread_detail->fileNames[i]);
        if(!inFile(serviceFile, nextFile)){
            pthread_mutex_lock(&serviced_lock);
                serviced = openFile(serviceFile, "a");
                fprintf(serviced, "%s\n", nextFile);
                closeFile(serviced);
            pthread_mutex_unlock(&serviced_lock);

            FILE* nameFileP = openFile(nextFile, "r");
            while(getline(&buffer, &buff_len, nameFileP) != -1){
                buffer[strcspn(buffer, "\n")] = 0;

                pthread_mutex_lock(&shm_lock);
                    while(shm_data->capacity == shm_data->size){
                        pthread_cond_wait(&buffer_full, &shm_lock);
                    }
                    enqueue(buffer, shm_data);
                pthread_mutex_unlock(&shm_lock);
            }
            closeFile(nameFileP);
            counter++;
        }
    }

    pthread_mutex_lock(&perform_lock);
        serviced = openFile("./performance.txt", "a");
        fprintf(serviced, "Thread %lu serviced %d files\n", (unsigned long)self , counter);
        closeFile(serviced);
    pthread_mutex_unlock(&perform_lock);

    pthread_mutex_lock(&shm_lock);
        shm_data->semephore = shm_data->semephore - 1;
    pthread_mutex_unlock(&shm_lock);

    pthread_cond_broadcast(&buffer_full);
    // printf("Out of files. Thread exiting ..\n");

    shmdt(shm_data);
    return NULL; 
}

void* Resolver(void* threadData){
    int shmid;
    char IP[256];
    char* buffer = (char*)malloc(256 * sizeof(char)); // these will be used as a temp space for the name files path and line contents

    // printf("Hello from resolver thread\n");   

    data * thread_detail = (data*) threadData;
    queue* shm_data;
    char* resultsFile = thread_detail->resultsFile;

    if ((shmid = shmget(5678, sizeof(queue), 0666)) < 0){
        perror("Resolver shmget failed: ");
        exit(1);
    }

    if ((shm_data = (queue *)shmat(shmid, NULL, 0)) == (queue*)-1){
        perror("Resolver shmat failed: ");
        exit(1);
    } 

    pthread_t self = pthread_self();

    while(true){

        pthread_mutex_lock(&shm_lock);
        while (shm_data->capacity == 0){
            if (shm_data->semephore == -1){
                shmdt(shm_data);
                printf("%lu thread exiting\n", (unsigned long)self);

                pthread_mutex_unlock(&shm_lock);
                pthread_cond_broadcast(&buffer_empty);
                pthread_exit(NULL);
            }
            pthread_cond_wait(&buffer_empty, &shm_lock);
        }
        
        dequeue(shm_data, &buffer);
        pthread_mutex_unlock(&shm_lock);
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        dnslookup(buffer, IP, sizeof(IP));
        if (strcmp(IP, "UNHANDELED") == 0)
            strcpy(IP, "");

        pthread_mutex_lock(&results_lock);
            FILE* results = openFile(resultsFile, "a");
                fprintf(results, "%s,%s\n", buffer, IP);
            closeFile(results);
        pthread_mutex_unlock(&results_lock);
    }

    shmdt(shm_data);
    printf("%lu thread exiting\n", (unsigned long)self);
    return NULL;
}

int main(int argc, char *argv[]){
    printf("Hello, world!\n");

    if (argc < 6){
        printf("Error: not enough file inputs\n This program reqires a requestor thread count, a resolver thread count, a requsetor log file, a reslover log file, and an input file.");
        exit(1);
    }
    int totaltime, shmid, i;
    int numRequestors = atoi(argv[1]);
    int numResolvers = atoi(argv[2]);
    char* servicedFileName = argv[3];
    char* resultsFileName = argv[4];
    struct timeval start_tv, finish_tv;
    struct timezone tz;

    gettimeofday(&start_tv, &tz);

    queue* domain;
    data threadData;
    key_t key = 5678;

    threadData.filesNum = argc - 5;
    threadData.resultsFile = resultsFileName;
    threadData.serviceFile = servicedFileName;

    for (i = 0; i <= argc - 5; i++){
        threadData.fileNames[i] = argv[i+5];
    }

    FILE* results = openFile(resultsFileName, "w");
    FILE* serviced = openFile(servicedFileName, "w");
    FILE* perform = openFile("./performance.txt", "w");
    closeFile(results);
    closeFile(serviced);
    closeFile(perform);

    pthread_mutex_lock(&perform_lock);
        serviced = openFile("./performance.txt", "a");
        fprintf(serviced, "Number for requester thread = %d\n", numRequestors);
        fprintf(serviced, "Number for resolver thread = %d\n", numResolvers);
        closeFile(serviced);
    pthread_mutex_unlock(&perform_lock);

    pthread_t* requestors = malloc(sizeof(pthread_t)*numRequestors);
    pthread_t* resolvers = malloc(sizeof(pthread_t)*numResolvers);

    pthread_mutex_init(&shm_lock, NULL);
    pthread_mutex_init(&serviced_lock, NULL);
    pthread_mutex_init(&perform_lock, NULL);
    pthread_mutex_init(&results_lock, NULL);
    pthread_cond_init(&buffer_full, NULL);
    pthread_cond_init(&buffer_empty, NULL);

    if ((shmid = shmget(key, sizeof(queue), IPC_CREAT|0666)) < 0){
        perror("Main shmget failed: ");
        exit(1);
    }

    if ((domain = (queue *)shmat(shmid, NULL, 0)) == (queue*)-1){
        perror("Main shmat failed: ");
        exit(1);
    }

    domain->head = -1;
    domain->tail = -1;
    domain->size = QUEUE_SIZE;
    domain->capacity = 0;
    domain->semephore = numRequestors -1;


    for (i = 0; i < numRequestors; i++){
        if (pthread_create(&requestors[i], NULL, Requestor, (void*)&threadData)) {
            perror("error creating requestor thread");
            exit(1);
        } 
    }

    for (i = 0; i < numResolvers; i++){
        if (pthread_create(&resolvers[i], NULL, Resolver, (void*)&threadData)) {
            perror("error creating requestor thread");
            exit(1);
        } 
    }

    for (i = 0; i < numRequestors; i++){
        pthread_join(requestors[i], 0);
    }

    for (i = 0; i < numResolvers; i++){    
        pthread_join(resolvers[i], 0);
    }

    gettimeofday(&finish_tv, &tz); 
    totaltime = finish_tv.tv_sec - start_tv.tv_sec;
    // printf("Start time: %ld\nFinish time: %ld\n", start_tv.tv_sec, finish_tv.tv_sec);
    printf("The program took %d seconds to complete.\n", totaltime);

    pthread_mutex_lock(&perform_lock);
        serviced = openFile("./performance.txt", "a");
        fprintf(serviced, "Total time: %d seconds.\n\n", totaltime);
        closeFile(serviced);
    pthread_mutex_unlock(&perform_lock);

    free(requestors);
    free(resolvers);

    pthread_mutex_destroy(&shm_lock);
    pthread_cond_destroy(&buffer_full);
    pthread_cond_destroy(&buffer_empty);
    pthread_mutex_destroy(&serviced_lock);
    pthread_mutex_destroy(&results_lock);
    pthread_mutex_destroy(&perform_lock);

    shmctl(shmid,IPC_RMID,NULL); 

    exit(0);
}















