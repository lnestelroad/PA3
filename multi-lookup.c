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

/////////////////////////////////////////// Queue Functions /////////////////////////////////////////////////////

void enqueue(char* domainName, queue* domain){
    // printf("Hello from enqueue.\n");
    // printf("Head: %d, Tail: %d, Size: %d\n", domain->head, domain->tail, domain->size);
    if ((domain->head == 0 && domain->tail == domain->size-1) || (domain->tail == (domain->head-1)%(domain->size-1))){
        printf("Buffer is full.\n");
        return;
    }

    else if (domain->head == -1){ 
        domain->head = 0;
        domain->tail = 0; 
        strcpy(domain->buffer[domain->tail], domainName); 
    } 
  
    else if (domain->tail == domain->size-1 && domain->head != 0){ 
        domain->tail = 0; 
        strcpy(domain->buffer[domain->tail], domainName); 
    } 
  
    else{ 
        domain->tail++; 
        strcpy(domain->buffer[domain->tail], domainName); 
    } 
}

void dequeue(queue* domain, char** data){
    // printf("Hello form dequeue.\n");

    if (domain->head == -1) { 
        printf("domain->buffer is empty.\n");
        strcpy(*data, "");
        return;
    }

    strcpy(*data, domain->buffer[domain->head]); 
    strcpy(domain->buffer[domain->head], "");
    if (domain->head == domain->tail) { 
        domain->head = -1; 
        domain->tail = -1; 
    } 
    else if (domain->head == domain->size-1) 
        domain->head = 0; 
    else
        domain->head++; 
}

///////////////////////////////////////////// Thread Functions /////////////////////////////////////////////////////
void* Requestor(){
    int shmid, i;
    int counter = 0;
    char buffer[128];
    char nextFile[256]; // these will be used as a temp space for the name files path and line contents
    FILE* serviced;

    /* data * shm_details = (data*) shm_dataPointer; */
    queue* shm_data;

    // printf("Hello from requestor thread\n");

    if ((shmid = shmget(5678, sizeof(queue), 0666)) < 0){
        perror("Thread shmget failed: ");
        exit(1);
    }

    if ((shm_data = (queue *)shmat(shmid, NULL, 0)) == (queue*)-1){
        perror("Thread shmat failed: ");
        exit(1);
    }

    for (i = 1; i < 6; i++){
        sprintf(nextFile, "../input/names%d.txt", i);
        if(!inFile("../serviced.txt", nextFile)){

            pthread_mutex_lock(&serviced_lock);
                serviced = openFile("../serviced.txt", "a");
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

    pthread_t self = pthread_self();
    pthread_mutex_lock(&perform_lock);
        serviced = openFile("../performance.txt", "a");
        fprintf(serviced, "Thread %lu serviced %d files\n", (unsigned long)self , counter);
        closeFile(serviced);
    pthread_mutex_unlock(&perform_lock);


    pthread_mutex_lock(&shm_data->shm_lock);
        shm_data->semephore -= 1;
    pthread_mutex_unlock(&shm_data->shm_lock);

    pthread_cond_broadcast(&shm_data->buffer_full);
    shmdt(shm_data);
    printf("Out of files. Thread exiting ...\n");

    return NULL; 
}

void* Resolver(){
    int shmid;
    char IP[256];
    char* data = (char*)malloc(256 * sizeof(char));
    bool flag = true;

    // printf("Hello from resolver thread\n");   
    queue* shm_data;

    if ((shmid = shmget(5678, sizeof(queue), 0666)) < 0){
        perror("Resolver shmget failed: ");
        exit(1);
    }

    if ((shm_data = (queue *)shmat(shmid, NULL, 0)) == (queue*)-1){
        perror("Resolver shmat failed: ");
        exit(1);
    } 

    while(flag){
        pthread_mutex_lock(&shm_data->shm_lock);
            if (shm_data->semephore <= 0 && shm_data->size <= 1)
                flag = false;
        pthread_mutex_unlock(&shm_data->shm_lock);

        dequeue(shm_data, &data);
        dnslookup(data, IP, sizeof(IP));
        if (strcmp(IP, "UNHANDELED") == 0)
            strcpy(IP, "");
        // printf("RESOLVER: Removed %s\n", buffer);

        pthread_mutex_lock(&results_lock);
            FILE* results = openFile("../results.txt", "a");
                fprintf(results, "%s,%s\n", data, IP);
            closeFile(results);
        pthread_mutex_unlock(&results_lock);

        pthread_mutex_lock(&shm_data->shm_lock);
            if (shm_data->semephore <= 0 && shm_data->size <= 1)
                flag = false;
        pthread_mutex_unlock(&shm_data->shm_lock);
    }

    free(data);
    shmdt(shm_data);
    // printf("Resolver thread exiting\n");
    return NULL;
}
///////////////////////////////////////////// Main Function //////////////////////////////////////////////////
int main(int argc, char *argv[]){
    printf("Hello, World %d\n", argc); 

    int totaltime, shmid, i;
    int numRequestors = atoi(argv[1]);
    int numResolvers = atoi(argv[2]);
    struct timeval start_tv, finish_tv;
    struct timezone tz;

    gettimeofday(&start_tv, &tz);

    queue* shm_data = NULL;
    pthread_t* requestors = malloc(sizeof(pthread_t)*numRequestors);
    pthread_t* resolvers = malloc(sizeof(pthread_t)*numResolvers);
    key_t key = 5678; 

    FILE* results = openFile("../results.txt", "w");
    FILE* serviced = openFile("../serviced.txt", "w");
    /* FILE* perform = openFile("../performance.txt", "w"); */
    closeFile(results);
    closeFile(serviced);
    /* closeFile(perform); */

    pthread_mutex_lock(&perform_lock);
        serviced = openFile("../performance.txt", "a");
        fprintf(serviced, "Number for requester thread = %d\n", numRequestors);
        fprintf(serviced, "Number for resolver thread = %d\n", numResolvers);
        closeFile(serviced);
    pthread_mutex_unlock(&perform_lock);

    

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
    shm_data->size = QUEUE_SIZE;
    shm_data->semephore = numRequestors;

    pthread_mutex_init(&shm_data->shm_lock, NULL);
    pthread_mutex_init(&serviced_lock, NULL);
    pthread_mutex_init(&results_lock, NULL);
    pthread_cond_init(&shm_data->buffer_full, NULL);

    for (i = 0; i < numRequestors; i++){
        if (pthread_create(&requestors[i], NULL, Requestor, NULL)) {
            perror("error creating requestor thread");
            exit(1);
        } 
    }

    for (i = 0; i < numResolvers; i++){
        if (pthread_create(&resolvers[i], NULL, Resolver, NULL)) {
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

    free(requestors);
    free(resolvers);

    pthread_mutex_lock(&serviced_lock);
        serviced = openFile("../serviced.txt", "a");
        fprintf(serviced, "\nTime of day: \n");
        closeFile(serviced);
    pthread_mutex_unlock(&serviced_lock);

    pthread_mutex_destroy(&shm_data->shm_lock);
    pthread_mutex_destroy(&serviced_lock);
    pthread_mutex_destroy(&results_lock);
    pthread_cond_destroy(&shm_data->buffer_full);	

    shmctl(shmid,IPC_RMID,NULL); 

    gettimeofday(&finish_tv, &tz); 
    totaltime = finish_tv.tv_sec - start_tv.tv_sec;
    // printf("Start time: %ld\nFinish time: %ld\n", start_tv.tv_sec, finish_tv.tv_sec);
    printf("The program took %d seconds to complete.\n", totaltime);

    pthread_mutex_lock(&perform_lock);
        serviced = openFile("../performance.txt", "a");
        fprintf(serviced, "Total time: %d seconds.\n\n", totaltime);
        closeFile(serviced);
    pthread_mutex_unlock(&perform_lock);

    exit(0);
}

