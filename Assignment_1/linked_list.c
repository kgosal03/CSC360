#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include "linked_list.h"

// Function to add a node
Node * add_newNode(Node* head, pid_t new_pid, char * new_path){
	Node *new_node = (Node *)malloc(sizeof(Node));
    if (!new_node) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    
    new_node->pid = new_pid;
    new_node->path = strdup(new_path);
    new_node->next = NULL;

    // Create a new node if list empty
    if (!head) {
        head = new_node;
    } 
    else {
        Node *current = head;
        // Traverse list to find last node
        while (current->next) {
            current = current->next;
        }
        // Adding the node in the list
        current->next = new_node;
    }
    
    return head;
}

// Function to delete a node
Node *deleteNode(Node *head, pid_t pid) {
    Node* current = head;
    Node* prev = NULL;

    // Traverse the list to find node with given pid
    while (current != NULL && current->pid != pid) {
        prev = current;
        current = current->next;
    }

    if (current == NULL) {
        // PID node not found
        fprintf(stderr, "Process with the PID %d is not found\n", pid);
        return head;
    }

    // Node is head node
    if (prev == NULL) {
        head = current->next;
    }
    else {
        // Node is not the head node
        prev->next = current->next;
    }

    // Deallocate the memory
    free(current->path);
    free(current);

    return head;
}

// Function to print the list or processes
void printList(Node *node){
	Node *current = node;

    if (current == NULL) {
        printf("No background jobs\n");
        return;
    }
    while (current != NULL) {
        printf("%d: %s\n", current->pid, current->path);
        current = current->next;
    }
}

// Function to check if node with given pid exists
int PifExist(Node *node, pid_t pid){
	Node *current = node;
    while (current != NULL) {
        // Process found
        if (current->pid == pid) {
            return 1;
        }
        current = current->next;
    }
    // Process not found
    return 0;
}

