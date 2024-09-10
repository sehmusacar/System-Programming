#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>

int sock = 0;
FILE *log_file;

// Signal handler function to cancel orders and close the socket
void handle_signal(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        printf(">^C signal .. cancelling orders.. editing\n");
        send(sock, "cancel", strlen("cancel"), 0);
        close(sock);
        if (log_file) {
            fclose(log_file);
        }
        exit(0);
    }
}

int main(int argc, char const *argv[]) {
    if (argc != 6) {
        fprintf(stderr, "Usage: %s [IP] [Port] [num_customers] [p] [q]\n", argv[0]);
        return 1;
    }
    
    const char* ip_address = argv[1];
    int port = atoi(argv[2]);
    int num_customers = atoi(argv[3]);
    int p = atoi(argv[4]);
    int q = atoi(argv[5]);
    
    if (p <= 0 || q <= 0) {
        fprintf(stderr, "City dimensions (p and q) must be greater than zero.\n");
        return 1;
    }
    
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};
    
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal); // Catch SIGTERM signal as well
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\nSocket creation error\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, ip_address, &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported\n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed\n");
        return -1;
    }

    log_file = fopen("client_log.txt", "a");
    if (!log_file) {
        perror("Failed to create log file");
        return 1;
    }
    
    // Print the PID to the console
    printf("Client PID: %d\n", getpid());
    fprintf(log_file, "Client PID: %d\n", getpid());

    // Seed the random number generator
    srand(time(NULL));
    
    sprintf(buffer, "%d %d %d", num_customers, p, q);

    // Collect all customer data first
    char customer_data[num_customers][50];
    for (int i = 0; i < num_customers; i++) {
        int x = rand() % p;
        int y = rand() % q;
        fprintf(log_file, "Customer %d, Location: (%d, %d)\n", i + 1, x, y);
        sprintf(customer_data[i], "order %d %d %d", i + 1, x, y);
    }

    // Send the collected customer data to the server
    send(sock, buffer, strlen(buffer), 0);
    for (int i = 0; i < num_customers; i++) {
        send(sock, customer_data[i], strlen(customer_data[i]), 0);
    }
    
    // Wait for the "done" message from the server
    int valread = read(sock, buffer, 1024);
    if (valread > 0) {
        buffer[valread] = '\0';
        if (strcmp(buffer, "done") == 0) {
            printf("All customers served\n");
            fprintf(log_file, "All customers served\n");

            printf("Log file written\n");
            fprintf(log_file, "Log file written\n");
        }
    }

    close(sock);
    if (log_file) {
        fclose(log_file);
    }
    
    return 0;
}
