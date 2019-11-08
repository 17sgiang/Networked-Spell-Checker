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


#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
//#include <sys/types.h>
//#include <sys/stat.h>
//#include <fcntl.h>
#include <string.h>

//  Declaration of constants or global variables
const char error_message[30] = "An error has occurred";
   
const int MAX_WORD_SIZE = 64; //Longest word in major dictionary is 45 letters
const int MAX_WORD_COUNT = 200000;
//273,000 words in the Oxford dictionary, 171,476 in current use

//  Declaration of structs
typedef struct {
    char **log_buf;
    int *job_buf;
    int job_len, log_len;
    int job_count, log_count;
    int job_front;
    int job_rear;
    int log_front;
    int log_rear;
    pthread_mutex_t job_mutex, log_mutex;
    pthread_cond_t job_cv_cs, job_cv_pd;
    pthread_cond_t log_cv_cs, log_cv_pd;
} buf;

//typedef struct node{
//    char* word[64];
//    struct node* next;
//    
//} node;

//------------------------------------------------------------------------//
//                         FUNCTION DECLARATION                           //
//------------------------------------------------------------------------//



//------------------------------------------------------------------------//
//                           MAIN FUNCTION                                //
//------------------------------------------------------------------------//

int main(int argc, char** argv) {
    
    //Open a file pointer to the word list and copy its contents
    int word_count = 0;
    char dictionary[MAX_WORD_COUNT][MAX_WORD_SIZE];
//    char* buffer;
    int buffer_size;
    
    FILE* fp = fopen("words-2.txt", "r");
    
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
    printf("Here\n");
    
    while(fgets(dictionary[word_count++], buffer_size, fp) != NULL){
        printf("%s\n", dictionary[word_count-1]);
    }
    
    //Close the file pointer
    fclose(fp);
    
    
    
    //Create queue for incoming socket descriptors
    //Synchronization mechanisms
    //Set up network connection
    //Worker threads
    //Open a socket on a port
    //Wait for clients to connect
    
    //If client connects
    //Socket descriptor will be created
    //Flagged?
    //Socket descriptor put on queue that worker threads take from
    //Wait for a word from the client
    //Run spellchecker
    //Report result to the client
    //Wait for more words/terminate
    return (EXIT_SUCCESS);
}




//------------------------------------------------------------------------//
//                       FUNCTION IMPLEMENTATION                          //
//------------------------------------------------------------------------//