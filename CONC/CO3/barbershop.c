/*
 *	Concurrency 4: The Barbershop Problem
 *	By Jonathan Harijanto and Kyle
 *
 *	Method: Pair Programming
 *
 *	Resources:
 *	http://caig.cs.nctu.edu.tw/course/OS11/OS_SleepingBarber_F10.pdf
 *	http://greenteapress.com/semaphores/LittleBookOfSemaphores.pdf
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <signal.h>
#include "mt19937ar.c"

#define MAX_CUSTOMERS 5

void get_hair_cut();
void cut_hair();
void *barber();
void *customer();
void signalHandler(int);

sem_t customerWait;
sem_t barberSleep;
sem_t customerSit;
sem_t haircutProcess;


void get_hair_cut(){
	printf("[CUSTOMER]: hair cut process.\n");
	sem_wait(&haircutProcess);
}

void cut_hair(){
	init_genrand(time(NULL));
	// Cut hair takes between 2-9 seconds
	int cutTime = genrand_int32() % 8 + 2;
	printf("[BARBER]: cut hair for %d seconds.\n", cutTime);
	sleep(cutTime);
	
}

void *barber(void *arg){
	while(1){
		printf("[BARBER]: sleep (no customer).\n");
		sem_wait(&barberSleep);
		cut_hair();
		printf("[BARBER]: haircut done.\n");
		sem_post(&haircutProcess); 
	}
}

void *customer(void *arg){
	sem_wait(&customerWait);
	printf("[CUSTOMER]: wait barber.\n");
	// Debugging:
	//waitingCustomer += 1;
	//printf("number wait before: %d\n", waitingCustomer);
	sem_wait(&customerSit);
	printf("[CUSTOMER]: sit in barber chair.\n");
	//waitingCustomer -= 1;
	//printf("number wait after: %d\n", waitingCustomer);
	sem_post(&customerWait);
	printf("[CUSTOMER]: wakes the barber.\n");
	sem_post(&barberSleep);
	get_hair_cut();
	sem_post(&customerSit);
	printf("[FINISHED CUSTOMER]: leaves barbershop.\n");
}

int main(){
	signal(SIGINT, signalHandler);

	pthread_t theBarber;
	pthread_t theCustomers[MAX_CUSTOMERS];

	// MANUALLY CHANGE #CUSTOMER AND CHAIR HERE!
	int waitingCustomer = 5;
	int waitingChair = 5;
	int i = 0;

	printf("%d NEW CUSTOMERS enter the barbershop.\n", waitingCustomer);
	if(waitingCustomer > MAX_CUSTOMERS){
		printf("%d of them have to leave due to FULL.\n", (waitingCustomer-waitingChair));
		printf("Change the number of customers & chairs in the code manually.\n");
		return;
	}

	sem_init(&customerWait, 0, waitingChair);
	sem_init(&barberSleep, 0, 0);
	sem_init(&haircutProcess, 0, 0);
	sem_init(&customerSit, 0, 1);

	pthread_create(&theBarber, NULL, barber, NULL);

	for(i=0; i<waitingCustomer; i++){
		pthread_create(&theCustomers[i], NULL, customer, (void *)&i);
	}
	
	for(i=0; i<waitingCustomer; i++){
		pthread_join(theCustomers[i], NULL);
	}	

	sem_post(&barberSleep);
	pthread_join(theBarber, NULL);

	return 0;
}

// Function that terminates the process when user press CTRL+C
void signalHandler(int signal){
	if(signal == SIGINT){
		printf(" --> SIGINT (Ctrl+C) caught! Exiting now.\n");
		exit(0);
	}
}
