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
#include <pthread.h>
#include <sys/types.h>
//#include <sys/stat.h>
//#include <fcntl.h>
//#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctype.h>
//#include "buffer.h"
#include "socket/open_listenfd.c"

//  Declaration of structs

typedef struct BUF { //Pass this buf struct to the worker threads
    char **log_buf;             //log buffer
    int *job_buf;               //job buffer
    int job_len, log_len;       //Max sizes for the buffers
    int job_count, log_count;
    int job_front;              //front = front++
    int job_rear;
    int log_front;
    int log_rear;
    pthread_mutex_t job_mutex, log_mutex;
    pthread_cond_t job_cv_cs, job_cv_pd;
    pthread_cond_t log_cv_cs, log_cv_pd;
} buf;


void buf_init(buf *sp, int job_len, int log_len);
void buf_deinit(buf *sp);
void buf_insert_log(buf *sp, char* item);
void buf_insert_job(buf *sp, int item);
int buf_remove_log(buf *sp, char** out_buf);
int buf_remove_job(buf *sp);



//------------------------------------------------------------------------//
//                         FUNCTION DECLARATION                           //
//------------------------------------------------------------------------//

int check_word(char* word, char** dictionary, int word_count);

void trim_strings(char** dictionary);

void worker(void* args);

void logger(void* args);



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
int main(int argc, char** argv) {
    
    const char error_message[30] = "An error has occurred";
    char* buffer = malloc((int)(sizeof(char) * MAX_WORD_SIZE));
    int word_count = 0;
    int buffer_size;
    char dictionary[MAX_WORD_COUNT][MAX_WORD_SIZE];
    
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
        
        if(arg1_port == 1) {
            port = atoi(arg1);
            strcpy(dictionary_name, arg2);
        } else {
            port = atoi(arg2);
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
    
    
    //buf struct contains the job and log queues
    struct buf* buffer;
    buf_init(buffer, NULL, NULL);
    
//    int work_queue[MAX_SOCKETS];
//    int queue_size = 0; //Modify based on how many items are in the queue
    
    //Synchronization mechanisms
    //Only one thread accesses the work queue at a given time
    //If the queue is full, the main thread should wait for a worker to signal
    //That there is space
    
    //Set up network connection
    int listeningSocket = open_listenfd(port);
    int connectionSocket;
    int signaled_to_stop = 0;
    
    //Create worker threads
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, worker, NULL);
    
    //While loop that waits for clients to connect
    while(signaled_to_stop == 0){
        
        connectionSocket = accept(listeningSocket, NULL, NULL);
        //connectionSocket now holds information about a connected client
        
        if(connectionSocket >= 0){
            //Add connectionSocket to the work queue
            while(queue_size == MAX_SOCKETS){ //Use a condition variable here
                //Sleep
                //Try to acquire the lock
            }
            buf_insert_job(buffer, connectionSocket);
        }

        //Signal any sleeping workers that there's a new socket in the queue
        pthread_cond_signal(pthread_cond_t *cond);
        
    }
    

        
    
    //Flagged?
    //Wait for more words/terminate
    //Log the results
    
    
    return (EXIT_SUCCESS);
}




//------------------------------------------------------------------------//
//                       FUNCTION IMPLEMENTATION                          //
//------------------------------------------------------------------------//

void buf_init(buf *sp, int job_len, int log_len){
    
    sp->job_len = job_len;
    sp->log_len = log_len;
    sp->job_count = 0;
    sp->log_count = 0;
    sp->job_front = 0;
    sp->job_rear = 0;
    sp->log_front = 0;
    sp->log_rear = 0;
    
}
void buf_deinit(buf *sp){

}

//Insert a word to be logged at the backside of the log_buf
//Update sp->log_rear
void buf_insert_log(buf *sp, char* item){
    sp->log_buf[sp->job_rear];
    
    //Update log_rear
    //Update log_count
    
}

//Insert a socket descriptor to the backside of the job_buf
//Update sp->job_rear
void buf_insert_job(buf *sp, int item){
    sp->job_buf[sp->job_rear + 1] = item;
   
    //Update job_rear
    sp->job_rear++;
    
    //Update job_count
    
    if(sp->job_front == sp->job_rear){
        //Queue is full
    }
    
}

//Remove a log from the log_buf and push it to the out_buf
//Return a success or failure value
int buf_remove_log(buf *sp, char** out_buf){
    
}

//Remove a socket descriptor from the job_buf
//Return the socket descriptor
int buf_remove_job(buf *sp){
    int ret_val = sp->job_buf[sp->job_front];
    
    //Update job_front
    sp->job_front++;
    
    if(sp->job_front == sp->job_rear){
        //Queue is empty
        
        
    }
    //Figure out how to use the modulo operator on the buf
    
    return ret_val;
}


//Assume that the dictionary loading works
//Return 1 if the word was found in dictionary
//Return 0 otherwise
int check_word(char* word, char** dictionary, int word_count){
    //Traverse the dictionary array
    //strcmp each word to the dictionary
    int found = 0;  // 0 = bad word, 1 = found word
    for(int i = 0; i < word_count; i++){
        
        if(strcmp(word, dictionary[i]) == 0){
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



void worker(void* stuff){
    //Open a socket on a port
    //If the queue is empty, the workers should wait for the main thread to
    //signal there is work
    
    
    while(true){ //While the client is still connected
        while(the work queue is not empty){ //Check using cond variables
            //remove a socket from the queue
                //
            //notify that there's an empty spot in the queue
                //Signal
            //service client (run spell checker)
            //close socket
        }
    }
    //Wait for a word from the client
    //Run spellchecker
    //Report result to the client
    
}


void logger(void* stuff){
    while(true){ //While the other threads haven't been closed
        while(log queue not empty){
            //remove entry from the log
            //write an entry to the log file
            //Word processed, is it a word
            
        }
    }
}

void service(void* stuff){
    while(word to read){
//        read work from socket
        if(word in dictionary){
            //echo the word back on the socket + "OK"
            
        }else{
            //echo the word back on the socket + "MISSPELLED"
        }
        //write the word and the socket response value ("OK" or "MISSPELLED")
        //to the log queue;
        
    }
}

