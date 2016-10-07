/*
 *	CS 444 Assignment 1 - The Producer-Consumer Problem
 *	By: Jonathan Harijanto and Kyle Martin Collins
 *
 *	Resources: 
 *	www.sourceware.org, docs.oracle.com, https://linux.die.net
 *	pubs.opengroup.org, stackoverflow.com, ibm.com/support/knowledgecenter
 *
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

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

// bla
struct value buffer[BUFFERMAX];

void *producer(void *arg){
	// Declare struct
	struct value producer;

	// Wait random amount of time (3-7 seconds)
	int producerWait = genrand_int32() % 8 + 3;	
	
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
	
		// If buffer not full, Producer generate random number value 
		// 1 - 25 
		producer.number = genrand_int32() % 25 + 1;
	
		// Producer random wait time (3-7) before producing again
		producer.waitPeriod = genrand_int32() % 8 + 3;
	
		// Store it to buffer
		buffer[bufferCounter] = producer;

		printf("I'm a producer!\n");
		printf("Current index: %d/32, Value of produced item: %d," 
			"Going to sleep for: %d seconds\n", bufferCounter, 
			producer.number, producer.waitPeriod);

		if(bufferCounter == 32) {
			printf("Resetting buffer...\n");
			bufferCounter = 0;
		} else {
			bufferCounter += 1;		
		}
	
		// Unlock the thread
		pthread_mutex_unlock(&mutex);
		// Wakes up the waiting consumer
		pthread_cond_signal(&consumerWait);
	}
}

void *consumer(void *arg){

	while(1){

	}
}

int main(){
	
	// Initialize seed with time()
	init_genrand(time(NULL));	
	
} 
