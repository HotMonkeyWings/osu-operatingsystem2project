/*
 *	CS444 Concurrency 2 - Dining Philosophers Problem
 *	By: Kyle and Jonathan Harijanto
 *	Method: Pair Programming
 *
 *	Resources:
 *	http://adit.io/posts/2013-05-11-The-Dining-Philosophers-Problem-With-Ron-Swanson.html	
 *	Little Book Semaphores - http://greenteapress.com/semaphores/LittleBookOfSemaphores.pdf	
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "mt19937ar.c"

// Tanenbaum's solution from Little Book Semaphores 
// Kevin recommended us to read this book on week 2

#define SIZE 5
#define THINKING 0
#define HUNGRY 1
#define EATING 2
#define LEFT (position+4)%SIZE
#define RIGHT (position+1)%SIZE

// Define mutex and list of semaphores (5)
sem_t mutex;
sem_t S[SIZE];

void think(int);
void get_forks(int);
void eat(int);
void put_forks(int);
void *philosopher(void *num);
void signalHandler(int);
void checkNeighbor(int);

int status[SIZE];
int thePhilosophers[SIZE]={0,1,2,3,4}; 

int main() {
	signal(SIGINT, signalHandler);
	
	int i;
	pthread_t threadId[SIZE];
	sem_init(&mutex,0,1);
	
	printf("--------------------------------\n");
	printf("Philosopher 0 is Plato\n.");
	printf("Philosopher 1 is Aristotle\n.");
	printf("Philosopher 2 is Immanuel Kant\n.");
	printf("Philosopher 3 is Socrates\n.");
	printf("Philosopher 4 is John Locke\n.");
	printf("--------------------------------\n");

	for(i=0; i<SIZE; i++){
		sem_init(&S[i],0,0);
	}
	for(i=0; i<SIZE; i++){
		pthread_create(&threadId[i], NULL, philosopher, &thePhilosophers[i]);
		think(i);
	}
	for(i=0; i<SIZE; i++){
		pthread_join(threadId[i], NULL);
	}	
}

void *philosopher(void *num) {
	while(1){
		int *n = num;
		sleep(1);
		get_forks(*n);
		sleep(0);
		put_forks(*n);
	}
}

// Function that tell a philosopher to 'think'
void think(int position) {
	init_genrand(time(NULL));
	int thinking;

	thinking = genrand_int32() % 20 + 1;
	printf("Philosopher %d is thinking for %d seconds.\n", thePhilosophers[position], thinking);
	sleep(thinking);
}

// Function that tell a philosopher to 'eat'
void eat(int position) {
	init_genrand(time(NULL));
	int eating;

	eating = genrand_int32() % 7 + 2;
	printf("Philosopher %d is eating for %d seconds.\n", thePhilosophers[position], eating);	
	sleep(eating);
}

// Function that tell a philosopher to start eating if hungry
void get_forks(int position) {
	sem_wait(&mutex);
	status[position] = HUNGRY;
	printf("Philosopher %d is hungry.\n", thePhilosophers[position]);
	checkNeighbor(position);
	sem_post(&mutex);
	sem_wait(&S[position]);
}

// Function that tell a philosopher to stop eating and start thinking
void put_forks(int position) {
	sem_wait(&mutex);
	status[position] = THINKING;
	think(position);
	checkNeighbor(LEFT);
	checkNeighbor(RIGHT);
	sem_post(&mutex);
}

// Function that check whether a philosopher can start eating or not.
void checkNeighbor(int position) {
	if(status[position] == HUNGRY && status[LEFT] != EATING && status[RIGHT] != EATING){
		status[position] = EATING;
		eat(position);
		sem_post(&S[position]);
	}	
}

// Function that terminates the process when user press CTRL+C
void signalHandler(int signal){
	if(signal == SIGINT){
		printf(" --> SIGINT (Ctrl+C) caught! Exiting now.\n");
		exit(0);
	}
}
