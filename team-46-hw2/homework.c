/* 
 * file:        homework.c
 * description: Skeleton code for CS 5600 Homework 2
 *
 * Peter Desnoyers, Northeastern CCIS, 2011
 * $Id: homework.c 530 2012-01-31 19:55:02Z pjd $
 */

#include <stdio.h>
#include <stdlib.h>
#include "hw2.h"

/********** YOUR CODE STARTS HERE ******************/

/*
 * Here's how you can initialize global mutex and cond variables
 */
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_sleep = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_ready = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_awake = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_done  = PTHREAD_COND_INITIALIZER;

/* the monitor states
 */
int barber_chairs  = 1;
int waiting_chairs = 4;
int barber_awake   = 1;
int customers_in   = 0;

/* the counters and timers
 */
int  all_customers = 0;
int  turning_away_customers = 0;
void *average_customer_num  = NULL;
void *average_customer_time = NULL;
void *fraction_sitting_baberchair = NULL;

/* the barber method
 */
void barber(void)
{
    pthread_mutex_lock(&m);
    if (customers_in == 0) {
        printf("DEBUG: %lf barber goes to sleep\n", timestamp());
    }
    while (1) {
        if (customers_in == 0) { // No customer left
            barber_awake = 0;            
            pthread_cond_wait(&cond_sleep, &m);                                    
            barber_awake = 1;
            pthread_cond_signal(&cond_awake);
        } else if (barber_chairs == 1) { // the barber is waiting for the next customer                        
            sleep_exp(0.01, &m);
        } else { // barber_chairs == 0
            sleep_exp(1.2, &m); // the barber is having the haircut           
                       
            // Free the customer who's waiting for the haircut done
            pthread_cond_signal(&cond_done);
            // Free the customer who's waiting for the next haircut
            pthread_cond_signal(&cond_ready);                 

            barber_chairs = 1; 
        }
    }
    pthread_mutex_unlock(&m);
}

/* the customer method
 */
void customer(int customer_num)
{
    pthread_mutex_lock(&m);    
    ++all_customers;
    if (waiting_chairs == 0) {
        // The barbershop is full.
        ++turning_away_customers;
    } else {
        printf("DEBUG: %lf customer %d enters shop\n", timestamp(), customer_num);
        --waiting_chairs;
        
        if (customers_in++ == 0) {
            printf("DEBUG: %lf barber wakes up\n", timestamp());
        }

        stat_count_incr(average_customer_num);
        stat_timer_start(average_customer_time);

        // Wait until the barber chair is available
        while (barber_chairs == 0) {
            pthread_cond_wait(&cond_ready, &m);
        }

        // The customer moves from the waiting chair to the barber chair
        barber_chairs = 0;
        ++waiting_chairs;
        stat_count_incr(fraction_sitting_baberchair);

        // Wait until the barber has been woken up
        pthread_cond_signal(&cond_sleep);
        while (!barber_awake) {
            pthread_cond_wait(&cond_awake, &m);
        }

        printf("DEBUG: %lf customer %d starts haircut\n", timestamp(), customer_num);
        pthread_cond_wait(&cond_done, &m); // Wait until the haircut is done
        stat_count_decr(fraction_sitting_baberchair); 
        printf("DEBUG: %lf customer %d leaves shop\n", timestamp(), customer_num);

        if (--customers_in == 0) {
            printf("DEBUG: %lf barber goes to sleep\n", timestamp());
        }

        stat_timer_stop(average_customer_time);
        stat_count_decr(average_customer_num);
    }
    pthread_mutex_unlock(&m);
}

/* Threads which call these methods. Note that the pthread create
 * function allows you to pass a single void* pointer value to each
 * thread you create; we actually pass an integer (the customer number)
 * as that argument instead, using a "cast" to pretend it's a pointer.
 */

/* the customer thread function - create 10 threads, each of which calls
 * this function with its customer number 0..9
 */
void *customer_thread(void *context) 
{
    int customer_num = (int)context; 
    while (1) {
        sleep_exp(10, NULL);
        customer(customer_num);
    }
    return 0;
}

/*  barber thread
 */
void *barber_thread(void *context)
{
    barber(); /* never returns */
    return 0;
}

void q2(void)
{        
    pthread_t tid;
    
    // Create barber thread, ignore result checking
    pthread_create(&tid, NULL, barber_thread, NULL);

    // Create customer thread
    int i;
    for (i = 0; i < 10; ++i) {
        pthread_create(&tid, NULL, customer_thread, (void*)i);
    }

    wait_until_done();
}

/* For question 3 you need to measure the following statistics:
 *
 * 1. fraction of customer visits result in turning away due to a full shop 
 *    (calculate this one yourself - count total customers, those turned away)
 * 2. average time spent in the shop (including haircut) by a customer 
 *     *** who does not find a full shop ***. (timer)
 * 3. average number of customers in the shop (counter)
 * 4. fraction of time someone is sitting in the barber's chair (counter)
 *
 * The stat_* functions (counter, timer) are described in the PDF. 
 */

void q3(void)
{
    average_customer_num  = stat_counter();
    average_customer_time = stat_timer();
    fraction_sitting_baberchair = stat_counter();
    q2();
    double val1 = (double) turning_away_customers / all_customers;
    double val2 = stat_count_mean(average_customer_num);
    double val3 = stat_timer_mean(average_customer_time);
    double val4 = stat_count_mean(fraction_sitting_baberchair);
    printf("fraction of customer visits result in turning away: %lf\n", val1);
    printf("average number of customers in the shop: %lf\n", val2);
    printf("average time spent in the shop (including haircut) by a customer: %lf\n", val3);
    printf("fraction of time someone is sitting in the barber's chair (counter): %lf\n", val4);
}
