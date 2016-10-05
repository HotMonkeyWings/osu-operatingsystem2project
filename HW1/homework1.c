/*
 *	CS 444 Assignment 1 - The Producer-Consumer Problem
 *	By: Jonathan Harijanto and Kyle Martin Collins
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

#include <mt19937ar.c>


// Set buffer size 
int buffIndex = 32;

// Condition variable initialization
pthread_cond_t bufferFull;
pthread_cont_t bufferNotFull;

// Struct with two numbers
struct value {
	int number;
	int waitPeriod;
};

void *producer(void *arg){

	// Wait random amount of time (3-7 seconds)
	int producerWait = rand() % (7 + 1 - 3) + 3;

}

void *consumer(void *arg){

}

int main(){

} 
