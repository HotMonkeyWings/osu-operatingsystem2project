/*CS 444 Assignment 1 - The Producer-Consumer Problem
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

// Declare buffer of type value
struct value buffer[BUFFERMAX];
int count = 1;

void *producer(void *arg){
	// Declare struct
	struct value producer;

	// Wait random amount of time (3-7 seconds)
	while(1){
		pthread_mutex_lock(&mutex);
		printf("\nProducer locked mutex\n");
		if(bufferCounter == BUFFERMAX){
			printf("Buffer full. Waiting for consumer to withdraw item\n");
			
			pthread_cond_wait(&producerWait,&mutex);
		}
			
		printf("About to generate stuff and produce it\n");
		int producerWait = genrand_int32() % 5 + 3;	
		producer.number = genrand_int32() % 25 + 1;
		producer.waitPeriod = genrand_int32() % 8 + 2;
		buffer[bufferCounter] = producer;
		bufferCounter++;
		printf("\n ========================================================== \n");
		printf("I'm a producer!\n");
		printf("Inserting new item at index: %d/32, Value of produced item: %d," 
			"Consumer sleep time for item: %d Going to sleep for: %d seconds\n"
			,bufferCounter-1,producer.number,producer.waitPeriod,producerWait);
	
		if(bufferCounter == 1)
		{
			printf("Producer about to unlock mutex\n");
			pthread_mutex_unlock(&mutex);
			printf("Producer unlocked mutex\n");
			printf("\nSending sig to consumer to wake the fuk up! BufferCounter = %d\n",bufferCounter);
			pthread_cond_signal(&consumerWait);
			sleep(producerWait);
		}
		else{
			pthread_mutex_unlock(&mutex);
			sleep(2);//producerWait);
		}

	}
}

void *consumer(void *arg){
	while(1){
		struct value tmp;
		int bufferTmp;
		printf("Consumer about to lock mutex\n");
		pthread_mutex_lock(&mutex);

		bufferTmp = bufferCounter-1;

		printf("Consumer locked mutex");
		tmp = buffer[bufferCounter-1];


		if(bufferCounter == 0){
			printf("Buffer is empty, consumer must wait\n");
			pthread_cond_wait(&consumerWait,&mutex);
		}
	
		bufferCounter--;

		printf("\n	***** Consumer will sleep for %d for item at index %d *****\n",
		tmp.waitPeriod,bufferTmp); //buffer[bufferCounter-1].waitPeriod,bufferCounter-1);
		pthread_mutex_unlock(&mutex);
		sleep(8);//tmp.waitPeriod); //buffer[bufferCounter-1].waitPeriod);
	
		//bufferCounter--;
		pthread_mutex_lock(&mutex);	
		printf("Consumer consumed item at index: %d/32, Value of consumed item: %d,", 
			bufferTmp, tmp.number);	//,bufferCounter-1, buffer[bufferCounter-1].number);
		printf("\n ========================================================== \n");
//		pthread_mutex_unlock(&mutex);
		if(bufferCounter == BUFFERMAX){
			pthread_mutex_unlock(&mutex);
			pthread_cond_signal(&producerWait);
	
		}

		if(bufferCounter == 0){
			printf("Buffer is empty, consumer must wait\n");
			pthread_cond_wait(&consumerWait,&mutex);
			printf("Consumer woke up from empty buffer\n");
			
		}
	

		printf("Relinquishing mutex control\n");
		pthread_mutex_unlock(&mutex);
		printf("Consumer mutex control gone\n");

/*		if(bufferCounter == 0){
			printf("Buffer is empty, consumer must wait\n");
			pthread_cond_wait(&consumerWait,&mutex);
			printf("Consumer woke up from empty buffer\n");
			
		}*/
	
	}
}

// Terminates the process when user press CTRL+C
void signalHandler(int signal){
	if(signal == SIGINT){
		printf("\nSIGINT (Ctrl+C) caught! Exiting now...\n");
		exit(0);
		// Free the memory because user cancels it
		//pthread_detach(producer);
		//pthread_detach(consumer);
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
  	pthread_cond_init(&producerWait, NULL);
  	pthread_cond_init(&consumerWait, NULL);		

  	// Create a new thread
  	pthread_create(&theProd, NULL, producer, NULL);
  	pthread_create(&theCons, NULL, consumer, NULL);

  	// Suspend execution of thread untill it terminates
  	pthread_join(theProd, NULL);
  	pthread_join(theCons, NULL);

	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&theProd);
	pthread_cond_destroy(&theCons);
	
}
