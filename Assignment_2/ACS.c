#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include "queue.h"

/* -----Defining constants----- */ 
// Number of Queues
#define NQUEUE 2
// Number of total Clerks
#define NCLERKS 5
// To use in marking if the Queue is served by a Clerk or not
#define FREE -1

// Customer Structure
struct customer_info {
    int user_id;
    int class_type;
    int service_time;
    int arrival_time;
};

// Mutex for Queue and overall common use, Cond var for Queue and Clerk
pthread_mutex_t queue_mutex[NQUEUE];
pthread_cond_t queue_cond[NQUEUE];
pthread_cond_t clerk_cond[NCLERKS];
pthread_mutex_t common_use_mutex;

// Queues for Economy and Business
Queue queues[NQUEUE];
// To hold the status of which Clerk is serving the Queue
int queue_status[NQUEUE];
int queue_length[NQUEUE];
// To track once a Customer is picked by a Clerk from a Queue
int winner_selected[NQUEUE];
// To track remaining customers for Clerks to exit out once all served
int remaining_customers;

// Average Waiting times Total, Business and Economy 
double total_waiting_time;
double business_waiting_time;
double economy_waiting_time;

// Count for the Business and Economy Queues
int business_customer_count;
int economy_customer_count;

struct timeval init_start_time;

// Returns the elapsed time in seconds since a reference time
void get_current_time(double *time) {
    struct timeval current_time;
    // Fills current_time with the current time
    gettimeofday(&current_time, NULL);
    *time = (current_time.tv_sec - init_start_time.tv_sec) + (current_time.tv_usec - init_start_time.tv_usec) / 1000000.0;
}

// Function declarations
void* customer_entry(void *cus_info);
void* clerk_entry(void *clerkNum);
int read_customers_from_file(const char *filename, struct customer_info **customers_ptr, int *business_count, int *economy_count);

int main(int argc, char *argv[]) {
    // Catching invalid number of arguments
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        fprintf(stderr, "Please provide exactly one argument, the name of the file containing customer information.\n");
        exit(1);
    }

    struct customer_info *customers;

    // Tracking counts for Business and Economy customers
    int business_customer_count, economy_customer_count;

    // Reading customer from the file
    int num_customers = read_customers_from_file(argv[1], &customers, &business_customer_count, &economy_customer_count);
    if (num_customers <= 0) {
        fprintf(stderr, "Please make sure the file has some content or in right format.\n");
        exit(1);
    }

    remaining_customers = num_customers;

    // Initialize queues based on the number of customers
    Queue business_queue, economy_queue;
    if (initQueue(&business_queue, business_customer_count) != 0) {
        return EXIT_FAILURE;
    }

    if (initQueue(&economy_queue, economy_customer_count) != 0) {
        return EXIT_FAILURE;
    }

    // Assigning queues the order
    queues[0] = economy_queue;
    queues[1] = business_queue;

    // Flags for success for various operations
    int cond_var_init_success;
    int mutex_init_success;
    int thread_creation_success;
    int cond_var_destroy_success;
    int mutex_destroy_success;

    // Initializing the mutex and cond var for both Queues
    for (int i = 0; i < NQUEUE; i++) {
        mutex_init_success = pthread_mutex_init(&queue_mutex[i], NULL);
        if (mutex_init_success != 0) {
            fprintf(stderr, "Error: Failed to initialize mutex for queue %d. Error code: %d\n", i, mutex_init_success);
            exit(1);
        }

        cond_var_init_success = pthread_cond_init(&queue_cond[i], NULL);
        if (cond_var_init_success != 0) {
            fprintf(stderr, "Error: Failed to initialize condition variable for queue %d. Error code: %d\n", i, cond_var_init_success);
            exit(1);
        }
        queue_status[i] = FREE;
        winner_selected[i] = 0;
    }

    // Initialize common use mutex
    mutex_init_success = pthread_mutex_init(&common_use_mutex, NULL);
    if (mutex_init_success != 0) {
            fprintf(stderr, "Error: Failed to initialize mutex for common use. Error code: %d\n", mutex_init_success);
            exit(1);
    }

    // Initializing the cond var for Clerks
    for (int i = 0; i < NCLERKS; i++) {
        cond_var_init_success = pthread_cond_init(&clerk_cond[i], NULL);
        if (cond_var_init_success != 0) {
            fprintf(stderr, "Error: Failed to initialize condition variable for clerk %d. Error code: %d\n", i, cond_var_init_success);
            exit(1);
        }
    }

    gettimeofday(&init_start_time, NULL);

    // Creating the Clerk threads
    pthread_t clerks[NCLERKS];
    for (int i = 0; i < NCLERKS; i++) {
        thread_creation_success = pthread_create(&clerks[i], NULL, clerk_entry, (void *)(long)i);
        if (thread_creation_success != 0) {
            fprintf(stderr, "Error: Failed to thread for for clerk %d. Error code: %d\n", i, thread_creation_success);
            exit(1);
        }
        printf("Clerk %d started working.\n", i);
    }

    // Creating the Customer threads
    pthread_t customers_t[num_customers];
    printf("\nCUSTOMERS STARTED ARRIVING.\n\n");
    for (int i = 0; i < num_customers; i++) {
        thread_creation_success = pthread_create(&customers_t[i], NULL, customer_entry, (void *)&customers[i]);
        if (thread_creation_success != 0) {
            fprintf(stderr, "Error: Failed to thread for for customer %d. Error code: %d\n", i, thread_creation_success);
            exit(1);
        }
    }

    // Ensuring all the customer threads complete before program termination
    for (int i = 0; i < num_customers; i++) {
        pthread_join(customers_t[i], NULL);
    }

    // Destroying the mutex and cond var for Queues
    for (int i = 0; i < NQUEUE; i++) {
        mutex_destroy_success = pthread_mutex_destroy(&queue_mutex[i]);
        if (mutex_destroy_success != 0) {
            fprintf(stderr, "Warning: Failed to destroy mutex for Queue %d. Error code: %d\n", i, mutex_destroy_success);
        } 

        cond_var_destroy_success = pthread_cond_destroy(&queue_cond[i]);
        if (cond_var_destroy_success != 0) {
            fprintf(stderr, "Warning: Failed to destroy cond var for Queue %d. Error code: %d\n", i, cond_var_destroy_success);
        } 

        free(queues[i].items); // Free allocated memory for each queue
    }

    // Destroying the cond var for Clerks
    for (int i = 0; i < NCLERKS; i++) {
        cond_var_destroy_success = pthread_cond_destroy(&clerk_cond[i]);
        if (cond_var_destroy_success != 0) {
            fprintf(stderr, "Warning: Failed to destroy cond var for Clerk %d. Error code: %d\n", i, cond_var_destroy_success);
        } 
    }

    // Destroying the Common use mutex
    mutex_destroy_success = pthread_mutex_destroy(&common_use_mutex);
    if (mutex_destroy_success != 0) {
        fprintf(stderr, "Warning: Failed to destroy mutex for common use. Error code: %d\n", mutex_destroy_success);
    } 

    // Printing the final information statistics
    printf("\n ---------------------------------------------------------------------------- \n");
    printf("\n|-----------------------------FINAL STATISTICS-------------------------------| \n");
    printf("\n ---------------------------------------------------------------------------- \n");
    printf("\nWe served the total of %d customers, of which %d were business-class and %d were economy-class!\n\n", num_customers, business_customer_count, economy_customer_count);
    printf("The average waiting time for all customers in the system is: %.2f seconds. \n", total_waiting_time / num_customers);
    printf("The average waiting time for all business-class customers is: %.2f seconds. \n", business_waiting_time / business_customer_count);
    printf("The average waiting time for all economy-class customers is: %.2f seconds. \n", economy_waiting_time / economy_customer_count);

    // Free allocated memory for customers
    free(customers);

    return 0;
}

// To handle the Customer threads
void* customer_entry(void *cus_info) {
    struct customer_info *p_myInfo = (struct customer_info *)cus_info;

    // Simulating the arrival time by putting the customer to sleep as they arrive
    usleep(p_myInfo->arrival_time * 100000);

    // Arrival Stats
    double current_time;
    get_current_time(&current_time);
    printf("A customer arrives: customer ID %2d. \n", p_myInfo->user_id);

    // Getting info to which Queue the customer belongs
    int queue_id = p_myInfo->class_type;

    // Adding customer to the right Queue
    pthread_mutex_lock(&queue_mutex[queue_id]);
    enqueue(&queues[queue_id], p_myInfo);
    queue_length[queue_id]++;

    if (queue_id == 0) {
        printf("A customer %2d enters the Economy Queue and the line total is %2d. \n",p_myInfo->user_id, queue_length[queue_id]);
    }
    else {
        printf("A customer %2d enters the Business Queue with ID %1d, and the line total is %2d. \n",p_myInfo->user_id, queue_id, queue_length[queue_id]);
    }
    

    double entered_queue_at_time;
    get_current_time(&entered_queue_at_time);

    // Constant checking for the thread if the condition is met
    while(1) {
        // Putting the thread to sleep and releasing the lock
        pthread_cond_wait(&queue_cond[queue_id], &queue_mutex[queue_id]);
        // Check if the customer is the first one in the queue and if there is no other customer being served already from that Queue
        if(queues[queue_id].items[queues[queue_id].front] == p_myInfo && !winner_selected[queue_id]) {
            dequeue(&queues[queue_id]);
            queue_length[queue_id]--;
            winner_selected[queue_id] = 1;
            break;
        }
    }
    pthread_mutex_unlock(&queue_mutex[queue_id]);

    // Giving small time to other waiting threads as well to get back to pthread_cond_wait
    usleep(10);
    
    // Getting the Clerk id
    int clerk_woke_me_up = queue_status[queue_id];
    // Updating overall status to free so other Clerk can take customer from the Queue
    queue_status[queue_id] = FREE;

    // Keeping track of the waiting time for the customer before started being served
    double started_being_served_at_time;
    get_current_time(&started_being_served_at_time);
    double waiting_time = started_being_served_at_time - entered_queue_at_time;

    pthread_mutex_lock(&common_use_mutex);
    total_waiting_time += waiting_time;
    if (p_myInfo->class_type == 1) {
        business_waiting_time += waiting_time;
    }
    else {
        economy_waiting_time += waiting_time;
    }
    pthread_mutex_unlock(&common_use_mutex);

    // Simulating the customer being served by putting to sleep
    printf("A clerk starts serving a customer: start time %.2f, the customer ID %2d, the clerk ID %1d. \n", started_being_served_at_time, p_myInfo->user_id, clerk_woke_me_up);
    usleep(p_myInfo->service_time * 100000);

    double end_service_time;
    get_current_time(&end_service_time);
    printf("A clerk finishes serving a customer: end time %.2f, the customer ID %2d, the clerk ID %1d. \n", end_service_time, p_myInfo->user_id, clerk_woke_me_up);

    // Signalling the serving Clerk that customer is served so Clerk can take another
    pthread_cond_signal(&clerk_cond[clerk_woke_me_up]);

    // Updating the total customer count
    pthread_mutex_lock(&common_use_mutex);
    remaining_customers--;
    pthread_mutex_unlock(&common_use_mutex);

    pthread_exit(NULL);
    return NULL;
}

// To handle the Clerk threads
void* clerk_entry(void *clerkNum) {
    int clerk_id = (int)(long)clerkNum;

    // Constantly check if any customer remaining otherwise exit out
    while (remaining_customers > 0) {
        int selected_queue_id = -1;
        
        // Check business queue first
        pthread_mutex_lock(&queue_mutex[1]);
        if (queue_length[1] != 0 && queue_status[1] == -1) {
            selected_queue_id = 1;
            queue_status[selected_queue_id] = clerk_id;
        }
        pthread_mutex_unlock(&queue_mutex[1]);

        // Check economy queue if no business customers waiting
        if (selected_queue_id == -1) {
            pthread_mutex_lock(&queue_mutex[0]);
            if (queue_length[0] != 0 && queue_status[0] == -1) {
                selected_queue_id = 0;
                queue_status[selected_queue_id] = clerk_id;
            }
            pthread_mutex_unlock(&queue_mutex[0]);
        }

        // If either of the Queue has customers
        if (selected_queue_id != -1) {
            pthread_mutex_lock(&queue_mutex[selected_queue_id]);
            queue_status[selected_queue_id] = clerk_id;
            winner_selected[selected_queue_id] = 0;
            // Broadcasting to wake up all the customer in that Queue
            pthread_cond_broadcast(&queue_cond[selected_queue_id]);
            // Putting the clerk to sleep to simulate it is serving customer and realeasing the lock
            pthread_cond_wait(&clerk_cond[clerk_id], &queue_mutex[selected_queue_id]);
            pthread_mutex_unlock(&queue_mutex[selected_queue_id]);
        }
    }
    pthread_exit(NULL);
    return NULL;
}

// Read customers from the file and returns the total number of customers
int read_customers_from_file(const char *filename, struct customer_info **customers_ptr, int *business_count, int *economy_count) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    // Reading the number of customers from the first line
    int num_customers;
    if (fscanf(fp, "%d", &num_customers) != 1 || num_customers <= 0) {
        fprintf(stderr, "Error: Invalid number of customers.\n");
        fclose(fp);
        return -1;
    }

    struct customer_info *customers = malloc(num_customers * sizeof(struct customer_info));
    if (!customers) {
        perror("Error: malloc");
        fclose(fp);
        return -1;
    }

    // Tracking each type of customer count
    *business_count = 0;
    *economy_count = 0;

    // Reading customer data and type
    for (int i = 0; i < num_customers; i++) {
        if (fscanf(fp, "%d:%d,%d,%d", &customers[i].user_id, &customers[i].class_type, &customers[i].arrival_time, &customers[i].service_time) != 4) {
            fprintf(stderr, "Error: Failed to read customer data.\n");
            free(customers);
            fclose(fp);
            return -1;
        }

        // Check if arrival time and service time are positive integers
        if (customers[i].arrival_time <= 0 ||
            customers[i].service_time <= 0 ||
            customers[i].arrival_time != (int)customers[i].arrival_time ||
            customers[i].service_time != (int)customers[i].service_time) {
            fprintf(stderr, "Error: Invalid arrival time or service time for customer %d.\n", customers[i].user_id);
            free(customers);
            fclose(fp);
            return -1;
        }

        if (customers[i].class_type == 1) {
            (*business_count)++;
        }
        else {
            (*economy_count)++;
        }
    }
    fclose(fp);

    *customers_ptr = customers;
    return num_customers;
}