/*
 *	CS 444 Assignment 1 - The Producer-Consumer Problem
 *	By: Jonathan Harijanto and Kyle Martin Collins
 *
 *	Method: Pair programming	
 *
 *	Resources: 
 *	www.sourceware.org, docs.oracle.com, https://linux.die.net
 *	pubs.opengroup.org, stackoverflow.com, ibm.com/support/knowledgecenter
 *	computing.llnl.gov/tutorials/pthreads and classmates!
 *
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>

#include "mt19937ar.c"

// Set buffer size 
#define BUFFERMAX 32

int bufferCounter = 0;

// Condition variable initialization
pthread_cond_t producerWait;
pthread_cond_t consumerWait;

// Insert something here
pthread_mutex_t mutex;

// Struct with two numbers
struct value {
	int number;
	int waitPeriod;
};

struct value buffer[BUFFERMAX];

void *producer(void *arg){
	// Declare struct
	struct value producer;

	// Wait random amount of time (3-7 seconds)
	int producerWait = genrand_int32() % 5 + 3;	
	
	while(1){
		// Initial wait time before 'producing' 
		sleep(producerWait);
		
		// Block calling thread to ensure synchronized access
		pthread_mutex_lock(&mutex);
	
		// Check for space availability in buffer
		while(bufferCounter == BUFFERMAX){
			printf("Buffer is full, producer has to wait.\n");
	
			// Condition variable is blocked - producer has to wait
			pthread_cond_wait(&producerWait,&mutex); 
		}
	
		// If buffer not full, Producer generates random number (1-25) 
		producer.number = genrand_int32() % 25 + 1;
	
		// Produce random wait time (2-9) for consumer
		producer.waitPeriod = genrand_int32() % 8 + 2;
	
		// Store it to buffer
		buffer[bufferCounter] = producer;

		printf("\n ========================================================== \n");
		printf("I'm a producer!\n");
		printf("Current index: %d/32, Value of produced item: %d," 
			"Going to sleep for: %d seconds\n", bufferCounter, 
			producer.number, producerWait);

		if(bufferCounter == 32) {
			printf("Resetting buffer...\n");
			bufferCounter = 0;
		} else {
			bufferCounter += 1;		
		}
	
		// Wakes up the waiting consumer
		pthread_cond_signal(&consumerWait);
		// Unlock the thread
		pthread_mutex_unlock(&mutex);
	}
}

void *consumer(void *arg){

	while(1){

		// Block calling thread to ensure synchronized access
		pthread_mutex_lock(&mutex);

		while(bufferCounter == 0){
			printf("Buffer is empty, consumer has to wait.\n");
	
			// Condition variable is blocked - consumer has to wait
			pthread_cond_wait(&consumerWait,&mutex); 
		}
	
		pthread_mutex_unlock(&mutex);

		sleep(buffer[bufferCounter].waitPeriod);

		printf("\nI'm a Consumer!\n");
		printf("Current index: %d/32, Value of produced item: %d," 
			"Going to sleep for: %d seconds\n", bufferCounter, 
			buffer[bufferCounter].number, buffer[bufferCounter].waitPeriod);
		printf("\n ========================================================== \n");

		// Block calling thread to ensure synchronized access
		pthread_mutex_lock(&mutex);

		bufferCounter -= 1;

		// Wakes up the waiting consumer
		pthread_cond_signal(&consumerWait);
		// Unlock the thread
		pthread_mutex_unlock(&mutex);

	}
}

// Terminates the process when user press CTRL+C
void signalHandler(int signal){
	if(signal == SIGINT){
		printf("SIGINT (Ctrl+C) caught!");
		// Free the memory because user cancels it
		//pthread_detach(theProd);
		//pthread_detach(theCons);
	}
}

int main(){
	
	pthread_t theProd;
	pthread_t theCons;

	// Initialize seed with time()
	init_genrand(time(NULL));

	// Set the interruption handler
	signal(SIGINT, signalHandler);	

	// Initialize mutex and conditional variable reference 	
	pthread_mutex_init(&mutex, NULL);	
  	pthread_cond_init(&consumerWait, NULL);		
  	pthread_cond_init(&producerWait, NULL);

  	// Create a new thread
  	pthread_create(&theCons, NULL, consumer, NULL);
  	pthread_create(&theProd, NULL, producer, NULL);

  	// Suspend execution of thread untill it terminates
  	pthread_join(theCons, NULL);
  	pthread_join(theProd, NULL);

	return 0;	
} 
