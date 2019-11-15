/*
 * File: spellcheck.c
 * Author: Steven Giang
 * Course: 3207-003: Operating Systems
 * Lab 3: Networked Spell Checker
 * Created on November 7, 2019, 12:03 PM
 * Create a spell-checking that multiple users can connect to and send words
 * to be spell-checked.
 * 
 */


//#include <stdio.h>
//#include <stdlib.h>
//#include "buffer.h"
//#include <sys/stat.h>
//#include <fcntl.h>
//#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctype.h>
#include "socket/open_listenfd.c"

//  Declaration of structs

typedef struct { //Pass this buf struct to the worker threads
    char **log_buf;             //log buffer
    int *job_buf;               //job buffer
    int job_len, log_len;       //Max sizes for the buffers
    int job_count, log_count;
    int job_front;              //front = front++
    int job_rear;
    int log_front;
    int log_rear;
    
    int word_count;
    char **dictionary;
    
    pthread_mutex_t *job_mutex, *log_mutex;
    pthread_cond_t *job_cv_cs, *job_cv_pd; //Condition Variables
    pthread_cond_t *log_cv_cs, *log_cv_pd;
} buf;





//------------------------------------------------------------------------//
//                         FUNCTION DECLARATION                           //
//------------------------------------------------------------------------//

//Related to the initialization and interaction with the buffer
void buf_init(buf *sp, int job_len, int log_len, char** dictionary, int count);
void buf_deinit(buf *sp);
void buf_insert_log(buf *sp, char* item);
void buf_insert_job(buf *sp, int item);
void buf_remove_log(buf *sp, char* out_buf);
int buf_remove_job(buf *sp);

//Spell-checker function, used to service the client
int check_word(char* word, void* args);

//Remove the '\n' from strings in the dictionary
void trim_strings(char** dictionary);

//Worker function that services a client. Worker threads use this function
void *worker(void* args);

//Logger function that outputs to a log file. Logger thread uses this function
void *logger(void* args);



//------------------------------------------------------------------------//
//                           MAIN FUNCTION                                //
//------------------------------------------------------------------------//

#define DEFAULT_DICTIONARY "words-2.txt"
#define DEFAULT_PORT 6666 //Placeholder
#define NUM_WORKERS 4
#define MAX_WORD_SIZE 64  
#define MAX_WORD_COUNT 200000 
#define MAX_SOCKETS 64
//Longest word in major dictionary is 45 letters
//273,000 words in the Oxford dictionary, 171,476 in current use

//Producer for the job buffer
//Reads the dictionary and saves it to memory
//Initializes the buffer
//Creates worker threads and logger thread
//Set up a network connection and waits for clients to connects
//Saves the clients' socket descriptors to a job_buf
int main(int argc, char** argv) {
    
    const char error_message[30] = "An error has occurred";
    char* buffer = malloc((int)(sizeof(char) * MAX_WORD_SIZE));
    int word_count = 0;
    int buffer_size;
//    char dictionary[MAX_WORD_COUNT][MAX_WORD_SIZE];
    char **dictionary = malloc(sizeof(char*) * MAX_WORD_COUNT);
    char* dictionary_name = DEFAULT_DICTIONARY;  // Default dictionary name
    int port = DEFAULT_PORT;
    printf("%s\n", dictionary_name);
    
    //Gets an alternative dictionary name + port number from the user
    //For argv[1] and argv[2] (if they exist) check if the string only contains numbers
    //If it contains anything but numbers, then assume it's the new dictionary name
    if(argc == 2){ 
        char arg1[64];
        int arg1_port = 1;
        strcpy(arg1, argv[1]);
        
        for(int i = 0; i < strlen(arg1); i++){
            if(isdigit(arg1[i]) != 0){ //Returns 0 if digit, non-zero otherwise
                arg1_port = 0;
            }
        }
        if(arg1_port == 1){
            port = atoi(arg1);
        }else{
            strcpy(dictionary_name, arg1);
        }
        
    } else if (argc == 3){
        
        //Assume that one of the arguments is the dictionary_name
        //Assume the other is the port
        char arg1[64], arg2[64];
        int arg1_port = 1;
        strcpy(arg1, argv[1]);
        strcpy(arg2, argv[2]);
        
        for(int i = 0; i < strlen(arg1); i++){
            if(isdigit(arg1[i]) != 0){ //Returns 0 if digit, non-zero otherwise
                arg1_port = 0;
            }
        }
        
        if(arg1_port == 1) { //If arg1 is found to be a port
            port = atoi(arg1);
            strcpy(dictionary_name, arg2);
        } else {
            port = atoi(arg2); //If arg1 is found to be a dictionary name
            strcpy(dictionary_name, arg1);
        }
        
    } else if (argc > 3) {
        printf("%s\n", "Too many arguments supplied.");
    }
    
    
    //Open a file pointer to the word list and copy its contents
    FILE* fp = fopen(dictionary_name, "r");
   
    //Error opening word list
    if(fp == NULL){
        printf("%s\n", error_message);
        printf("%s\n", "Failed to open dictionary file.");
        return (EXIT_FAILURE);
    }
    
    //Dictate the max possible size required for the buffer
    fseek(fp, 0, SEEK_END); //Moves the pointer to the end of the file
    buffer_size = ftell(fp); //Sets buffer_size to the size of the file
    fseek(fp, 0, SEEK_SET); //Bring the pointer back to the start
    
    //Copies the next word to the dictionary within the conditional
    while(fgets(dictionary[word_count++], buffer_size, fp) != NULL){
//        printf("%s\n", dictionary[word_count-1]);
    }
    
    //Close the file pointer
    fclose(fp);
    
    //Remove the '\n' character from the end of every entry in the dictionary
    trim_strings(dictionary);
    
    //Initialize the buffer
    buf* global_buf;
    buf_init(global_buf, 64, 64, dictionary, word_count);
    
    //Set up network connection
    int listeningSocket = open_listenfd(port);
    int connectionSocket;
    int signaled_to_stop = 0;
    
    //Create worker threads.
    //Reference Project3-1.pptx slide 12
    pthread_t threadPool[NUM_WORKERS];
    int threadIDs[NUM_WORKERS];
    printf("Launching threads.\n");
    for(int i = 0; i < NUM_WORKERS; i++){
        threadIDs[i] = i;
        //Start runnning the threads
        pthread_create(&threadPool[i], NULL, &worker, global_buf);
    }
    
    //Create logger thread
    pthread_t loggerThread;
    int loggerID = NUM_WORKERS;
    pthread_create(&loggerThread, NULL, &logger, global_buf);    
            
    //While loop that waits for clients to connect
    while(signaled_to_stop == 0){
        
        connectionSocket = accept(listeningSocket, NULL, NULL);
        //connectionSocket now holds information about a connected client
        
        if(connectionSocket >= 0){
            
            //Try to acquire the lock
            pthread_mutex_trylock(global_buf->job_mutex);
            
            //Add connectionSocket to the work queue
            while(global_buf->job_count == global_buf->job_len){
                //Sleep if the job queue is full, until a consumer wakes it up
                pthread_cond_wait(global_buf->job_cv_pd, global_buf->job_mutex);
                
            }
            buf_insert_job(global_buf, connectionSocket);
            
            //Signal any sleeping workers that there's a new socket in the queue
            pthread_cond_signal(global_buf->job_cv_cs);
            pthread_mutex_unlock(global_buf->job_mutex);    //Release the lock
            
        }
        
    }
    return (EXIT_SUCCESS);
}




//------------------------------------------------------------------------//
//                       FUNCTION IMPLEMENTATION                          //
//------------------------------------------------------------------------//

//Initializes the buffer's values, locks, and condition variables
void buf_init(buf *sp, int job_len, int log_len, char** dictionary, int count){
    sp->job_len = job_len;
    sp->job_count = 0;
    sp->job_front = 0;
    sp->job_rear = 0;
    
    sp->log_len = log_len;
    sp->log_count = 0;
    sp->log_front = 0;
    sp->log_rear = 0;
    
    sp->dictionary = dictionary;
    sp->word_count = count;
    
    pthread_mutex_init(sp->job_mutex, NULL);
    pthread_mutex_init(sp->log_mutex, NULL);
    pthread_cond_init(sp->job_cv_cs, NULL);
    pthread_cond_init(sp->job_cv_pd, NULL);
    pthread_cond_init(sp->log_cv_cs, NULL);
    pthread_cond_init(sp->log_cv_pd, NULL);
    
//    sp->job_mutex = PTHREAD_MUTEX_INITIALIZER;
//    sp->log_mutex = PTHREAD_MUTEX_INITIALIZER;
//    sp->job_cv_cs = PTHREAD_COND_INITIALIZER;
//    sp->job_cv_pd = PTHREAD_COND_INITIALIZER;
//    sp->log_cv_cs = PTHREAD_COND_INITIALIZER;
//    sp->log_cv_pd = PTHREAD_COND_INITIALIZER;
    
}

//Sets the buffer's values to an unusable state
void buf_deinit(buf *sp){
    sp->job_len = -1;
    sp->job_count = -1;
    sp->job_front = -1;
    sp->job_rear = -1;
    
    sp->log_len = -1;
    sp->log_count = -1;
    sp->log_front = -1;
    sp->log_rear = -1;
    
    sp->dictionary = NULL;
    sp->word_count = -1;
    
    pthread_mutex_destroy(sp->job_mutex);
    pthread_cond_destroy(sp->job_cv_cs);
    pthread_cond_destroy(sp->job_cv_pd);
    
    pthread_mutex_destroy(sp->log_mutex);
    pthread_cond_destroy(sp->log_cv_cs);
    pthread_cond_destroy(sp->log_cv_pd);
    
}

//Insert a word to be logged at the backside of the log_buf
//Update sp->log_rear
void buf_insert_log(buf *sp, char* item){
    if(sp->log_count == 0){
        //Don't update log_rear
        strcpy(sp->log_buf[sp->log_rear], item); 
        sp->log_count++;
        
    } else {
        strcpy(sp->log_buf[sp->log_rear + 1], item);
        sp->log_rear = (sp->log_rear + 1) % sp->log_len;
        sp->log_count++;
    
    }
}

//Insert a socket descriptor to the backside of the job_buf
//Update sp->job_rear
void buf_insert_job(buf *sp, int item){
    if(sp->job_count == 0){
        //Don't update job_rear
        sp->job_buf[sp->job_rear] = item;
        sp->job_count++;
        
    } else {
        sp->job_buf[sp->job_rear + 1] = item;
        sp->job_rear = (sp->job_rear + 1) % sp->job_len;
        sp->job_count++;
    }
}

//Remove a log from the log_buf and push it to the out_buf
//Return a success or failure value
void buf_remove_log(buf *sp, char* out_buf){
    strcpy(out_buf, sp->log_buf[sp->log_rear]);
    
    if(sp->log_front != sp->log_rear){
        sp->log_front = (sp->log_front + 1) % sp->log_len;
    }
    
    sp->log_count--;
}

//Remove a socket descriptor from the job_buf
//Return the socket descriptor
int buf_remove_job(buf *sp){
    
    int ret_val = sp->job_buf[sp->job_front];
    
    if(sp->job_front != sp->job_rear){
        sp->job_front = (sp->job_front + 1) % sp->job_len;
    }
    sp->job_count--;
    
    return ret_val;
}


//Assume that the dictionary loading works
//Return 1 if the word was found in dictionary
//Return 0 otherwise
//Return -1 if an exit code is entered
int check_word(char* word, void* args){
    
    buf* buffer = (buf*) args;
    char* exit_code = "4747";
    
    if(strcmp(word, exit_code) == 0){
        return -1;
    }
    
    //Traverse the dictionary array
    //strcmp each word to the dictionary
    int found = 0;  // 0 = bad word, 1 = found word
    for(int i = 0; i < buffer->word_count; i++){
        if(strcmp(word, buffer->dictionary[i]) == 0){
            found = 1;
            return found;
        }
    }
    return found;
}

//fgets() keeps the newline character on the strings
//If the words being checked end in newline anyways, this is not necessary
//If the words can end in newline or '\0', adopt this code to edit the word being processed
void trim_words(char** dictionary){
   
    //Traverse the dictionary array
    //Replace the newline character '\n' with the end of string character '\0'
    int i = 0;
    
    while(dictionary[i] != NULL){
        //Traverse the word
        int j = 0;
        while(dictionary[i][j] != '\n' 
                && dictionary[i][j] != EOF
                && dictionary[i][j] != '\0'){
            j++;
        }
        dictionary[i][j] = '\0';
        i++;
    }
}


//Consumer for the job buffer
//Producer for the log buffer
//Function that the worker threads execute
//Gets socket descriptor, receives and spellchecks words
//Echoes result onto the client
void *worker(void* args){
    
    buf* buffer = (buf*) args; 
    int socket_descriptor;
    int thread_terminated = 0;
    int connection_up = 1;
    char* word = malloc(sizeof(char) * MAX_WORD_SIZE);
    
    while (thread_terminated == 0) { 
        //Value of thread_terminated does not change.
        //Main thread should call for the thread to exit
        
        //Get a socket descriptor    
        pthread_mutex_trylock(buffer->job_mutex);
        while (buffer->job_count == 0) { //While no jobs
            //Wait for the main thread to signal there is work
            pthread_cond_wait(buffer->job_cv_cs, buffer->job_mutex);
        }

        //remove a socket descriptor from the queue
        socket_descriptor = buf_remove_job(buffer);

        //Signal that there's an empty spot in the queue
        pthread_cond_signal(buffer->job_cv_pd);
        pthread_mutex_unlock(buffer->job_mutex);

        while (connection_up != 0) { //While the client is still connected
            //Wait for a word from the client
            recv(socket_descriptor, word, MAX_WORD_SIZE, 0);
            
            //Run spellchecker
            int spell_correct = check_word(word, buffer);
            
            //Edit word based on the result of spellchecker
            if(spell_correct == 1){ //word + " OK"
                strcat(word, " OK");
            } else if(spell_correct == -1){ //Exit thread
                connection_up = 0;
            } else{ //word + " MISSPELLED"
                strcat(word, " MISSPELLED");
            }
            //Report result to the client
            send(socket_descriptor, word, MAX_WORD_SIZE, 0);
            
            
            //write the word and the socket response value ("OK" or "MISSPELLED")
            //to the log queue;
            //Try to get the key and put the result on the log_buf
            pthread_mutex_trylock(buffer->log_mutex);
            while(buffer->log_count == buffer->log_len){    //log_buf is full
                pthread_cond_wait(buffer->log_cv_pd, buffer->log_mutex);
            }
            
            buf_insert_log(buffer, word);
            pthread_cond_signal(buffer->log_cv_cs);
            pthread_mutex_unlock(buffer->log_mutex);
            
        }
        
    }    
    
}

//Consumer for the log buffer
//Function that the logger thread executes
//Takes results from the log_buf and writes them onto a log file
void *logger(void* args){
    
    buf* buffer = (buf*) args;
    FILE *fp = fopen("log.txt", "w");
    char* word = malloc(sizeof(char) * MAX_WORD_SIZE);
    int thread_terminated = 0;
    
    //While the main thread hasn't signaled for this thread to close    
    while(thread_terminated == 0){ 
        //Try to get the lock
        pthread_mutex_trylock(buffer->log_mutex);
        while(buffer->log_count == 0){
            pthread_cond_wait(buffer->log_cv_cs, buffer->log_mutex);    
        }
        
        buf_remove_log(buffer, word);
        pthread_mutex_unlock(buffer->log_mutex);
        
        //write an entry to the log file
        fprintf(fp, word);
        
    }
    fclose(fp);
}

