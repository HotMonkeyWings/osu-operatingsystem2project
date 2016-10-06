/*
 *	CS 444 Assignment 1 - The Producer-Consumer Problem
 *	By: Jonathan Harijanto and Kyle Martin Collins
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#include "mt19937ar.c"

// Set buffer size 
#define BUFFINDEX = 32;

// Condition variable initialization
pthread_cond_t bufferFull =  PTHREAD_COND_INITIALIZER;
pthread_cond_t bufferNotFull = PTHREAD_COND_INITIALIZER;

// Struct with two numbers
struct value{
	int number;
	int waitPeriod;
};

void *producer(void *arg){
	
	// Wait random amount of time (3-7 seconds)
	int producerWait = genrand_int32() % 8 + 3;	

	
	while(true){

	}
}

void *consumer(void *arg){

	while(true){

	}
}

int main(){
	
	// Initialize seed with time()
	init_genrand(time(NULL));	

	struct value buffer[BUFFINDEX];
	
} 
