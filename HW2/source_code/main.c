#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

volatile int child_exit_count = 0;

// Signal handler to catch SIGCHLD, increment child exit counter, and log child exit details
void child_handler(int sig) {
    pid_t pid;
    int status;
    // Process all terminated children without blocking
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("Child with PID %ld exited with status %d.\n", (long)pid, status);
        child_exit_count++;
        if (child_exit_count == 2) {
            printf("All child processes have completed.\n");
        }
    }
}

int main(int argc, char *argv[]) {
    // Check if program is run with the correct number of arguments
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <integer>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int num = atoi(argv[1]);  // Convert argument to integer
    const char *fifo1 = "./myfifo1";  
    const char *fifo2 = "./myfifo2";  
    
    // Remove any existing FIFOs with the same name
    unlink(fifo1);
    unlink(fifo2);

    // Create two FIFOs for inter-process communication
    if (mkfifo(fifo1, 0666) == -1 || mkfifo(fifo2, 0666) == -1) {
        perror("Failed to create FIFOs");
        exit(EXIT_FAILURE);
    }
    printf("FIFOs created successfully.\n");

    // Setup signal handling for child process termination
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = child_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD, &sa, NULL);

    // Fork first child process
    pid_t pid1, pid2;
    pid1 = fork();
    if (pid1 == 0) {  // Child 1: Process data from fifo1 and send results to fifo2
        int fd_read = open(fifo1, O_RDONLY);
        if (fd_read == -1) {
            perror("Failed to open fifo1 for reading");
            exit(EXIT_FAILURE);
        }
        int buffer[num], sum = 0, product = 1;
        if (read(fd_read, buffer, sizeof(buffer)) != sizeof(buffer)) {
            perror("Failed to read data from fifo1");
            close(fd_read);
            exit(EXIT_FAILURE);
        }
        close(fd_read);
        for (int i = 0; i < num; i++) {
            sum += buffer[i];
            product *= buffer[i];
        }
        int fd_write = open(fifo2, O_WRONLY);
        if (fd_write == -1) {
            perror("Failed to open fifo2 for writing");
            exit(EXIT_FAILURE);
        }
        if (write(fd_write, &sum, sizeof(sum)) != sizeof(sum) ||
            write(fd_write, &product, sizeof(product)) != sizeof(product) ||
            write(fd_write, "multiply", strlen("multiply") + 1) != strlen("multiply") + 1) {
            perror("Failed to write data to fifo2");
            close(fd_write);
            exit(EXIT_FAILURE);
        }
        close(fd_write);
        exit(0);
    }

    // Fork second child process
    pid2 = fork();
    if (pid2 == 0) {  // Child 2: Process data from fifo2
        sleep(10);  // Ensure this child process waits for data to be ready in fifo2
        int fd_read = open(fifo2, O_RDONLY);
        if (fd_read == -1) {
            perror("Failed to open fifo2 for reading");
            exit(EXIT_FAILURE);
        }
        int received_sum, received_product;
        if (read(fd_read, &received_sum, sizeof(received_sum)) != sizeof(received_sum) ||
            read(fd_read, &received_product, sizeof(received_product)) != sizeof(received_product)) {
            perror("Failed to read sum or product from fifo2");
            close(fd_read);
            exit(EXIT_FAILURE);
        }
        char command[10] = {0};  // Initialize command buffer to zero
        if (read(fd_read, command, sizeof(command) - 1) <= 0) { // Ensure null termination and check for read success
            perror("Failed to read command from fifo2");
            close(fd_read);
            exit(EXIT_FAILURE);
        }
        close(fd_read);

        int final_value = strcmp(command, "multiply") == 0 ? received_sum + received_product : received_sum;
        printf("Child 2 received command: %s\n", command);
        printf("Child 2 received sum: %d, multiplication result: %d, final value(sum+multiplication): %d.\n", received_sum, received_product, final_value);
        exit(0);
    }

    // Generate a random array and write it to fifo1
    int array[num];
    srand(time(NULL));
    printf("Random array generated:\n");
    for (int i = 0; i < num; i++) {
        array[i] = rand() % 10; // Generate random numbers from 0 to 9
        printf("%d ", array[i]);
    }
    printf("\n");

    int fd1 = open(fifo1, O_WRONLY);
    write(fd1, array, sizeof(array));
    close(fd1);

    // Wait for both children to exit, printing "Proceeding" every 2 seconds
    while (child_exit_count < 2) {
        printf("Proceeding\n");
        sleep(2);
    }

    // Clean up by removing FIFO files
    unlink(fifo1);
    unlink(fifo2);
    printf("FIFO files have been deleted.\n");

    printf("All child processes have completed.\n");
    printf("Parent finished waiting for children.\n");

    return 0;
}
