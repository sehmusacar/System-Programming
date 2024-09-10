#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Queue.h"
#define MAX_SIZE 100

struct Queue{
    char array[MAX_SIZE][100]; 
    int size; // Kuyruktaki eleman sayisi
};

Queue initializeQueue() {
    Queue queue;
    queue.size = 0;
    return queue;
}

void showQueue(Queue* queue) {
    if (checkQueueEmpty(queue)) {
        printf("The queue is empty. \n");
        return;
    }

    printf("Next elements:\n");
    for (int i = 0; i < queue->size; i++) {
        printf("%s\n", queue->array[i]); // Kuyruktaki tum elemanlari yazdir
    }
}

int checkQueueFull(Queue* queue) {
    return queue->size == MAX_SIZE;  // Kuyruk dolu mu diye kontrol et
}

int checkQueueEmpty(Queue* queue) {
    return queue->size == 0; // Kuyruk bos mu diye kontrol et
}

void insertInQueue(Queue* queue, const char* item) {
    if (checkQueueFull(queue)) {
        printf("Error Queue full. Failed to add an element.\n");
        return;
    }

    strcpy(queue->array[queue->size], item); // Yeni elemani kuyruga ekle
    queue->size++;
}

char* extractFromQueue(Queue* queue) {
    if (checkQueueEmpty(queue)) {
        printf("Error The queue is empty. Element cannot be removed.\n");
        return NULL;
    }

    char* item = (char*)malloc(10 * sizeof(char)); // Cikarilacak eleman icin yer ayir
    strcpy(item,queue->array[0]); // Ilk elemani al
    for (int i = 0; i < queue->size - 1; i++) {
        strcpy(queue->array[i], queue->array[i + 1]); // Elemanlari kaydir
    }
    queue->size--; // Kuyruk boyutunu azalt
    return item; // Elemani dondur
}

