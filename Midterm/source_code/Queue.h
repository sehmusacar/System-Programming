#ifndef QUEUE_H
#define QUEUE_H

#define MAX_SIZE 100

typedef struct {
    char array[MAX_SIZE][100]; 
    int size;
} Queue;

Queue initializeQueue();
void insertInQueue(Queue* queue, const char* item);
void showQueue(Queue* queue);
int checkQueueEmpty(Queue* queue);
int checkQueueFull(Queue* queue);
char* extractFromQueue(Queue* queue) ;


#endif /* QUEUE_H */