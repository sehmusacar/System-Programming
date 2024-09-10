#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <signal.h>

#define QUEUE_SIZE 100
#define DELIVERY_CAPACITY 3
#define OVEN_CAPACITY 6

int server_fd;

typedef struct {
    int order_id;
    int x;
    int y;
} Order;

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t delivery_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t delivery_cond = PTHREAD_COND_INITIALIZER;

Order order_queue[QUEUE_SIZE];
Order delivery_queue[QUEUE_SIZE];
int front = 0, rear = 0, count = 0;
int delivery_front = 0, delivery_rear = 0, delivery_count = 0;
int pending_orders = 0;
int cancel_flag = 0;

FILE *log_file;

int delivery_speed;
int num_cooks;
int num_deliverers;

int cook_order_count[QUEUE_SIZE] = {0};
int deliverer_order_count[QUEUE_SIZE] = {0};

void manager(Order order, int cook_id);

void enqueue(Order queue[], int *rear, int *count, Order value) {
    queue[*rear] = value;
    *rear = (*rear + 1) % QUEUE_SIZE;
    (*count)++;
}

Order dequeue(Order queue[], int *front, int *count) {
    Order value = queue[*front];
    *front = (*front + 1) % QUEUE_SIZE;
    (*count)--;
    return value;
}

void handle_signal(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        printf("Server shutting down...\n");
        fprintf(log_file, "Server shutting down...\n");
        fclose(log_file);
        close(server_fd);
        exit(0);
    }
}

void manager(Order order, int cook_id) {
    fprintf(log_file, "Manager: Order %d received and sent to cook %d.\n", order.order_id, cook_id);
    fflush(log_file); // Ensure log is written immediately
}

void* cook(void* arg) {
    int cook_id = *(int*)arg;
    while (1) {
        pthread_mutex_lock(&queue_mutex);
        while (count == 0 && !cancel_flag) {
            pthread_cond_wait(&queue_cond, &queue_mutex);
        }
        if (cancel_flag && count == 0) {
            pthread_mutex_unlock(&queue_mutex);
            break;
        }
        Order order = dequeue(order_queue, &front, &count);
        pending_orders++;
        pthread_mutex_unlock(&queue_mutex);

        manager(order, cook_id);

        // Simulate cooking process
        fprintf(log_file, "Cook %d is preparing order %d...\n", cook_id, order.order_id);
        fflush(log_file); // Ensure log is written immediately
        fprintf(log_file, "Cook %d finished preparing order %d.\n", cook_id, order.order_id);
        fflush(log_file); // Ensure log is written immediately

        pthread_mutex_lock(&delivery_mutex);
        enqueue(delivery_queue, &delivery_rear, &delivery_count, order);
        pthread_cond_signal(&delivery_cond);
        pthread_mutex_unlock(&delivery_mutex);

        cook_order_count[cook_id]++;
    }
    return NULL;
}

void* deliver(void* arg) {
    int deliverer_id = *(int*)arg;
    while (1) {
        pthread_mutex_lock(&delivery_mutex);
        while ((delivery_count < DELIVERY_CAPACITY && pending_orders >= DELIVERY_CAPACITY && !cancel_flag) || cancel_flag) {
            pthread_cond_wait(&delivery_cond, &delivery_mutex);
        }
        if (cancel_flag && delivery_count == 0) {
            pthread_mutex_unlock(&delivery_mutex);
            break;
        }

        Order orders[DELIVERY_CAPACITY];
        int order_count = 0;

        while (delivery_count > 0 && order_count < DELIVERY_CAPACITY) {
            orders[order_count++] = dequeue(delivery_queue, &delivery_front, &delivery_count);
        }
        pthread_mutex_unlock(&delivery_mutex);

        if (order_count > 0) {
            for (int i = 0; i < order_count; i++) {
                Order order = orders[i];
                int delivery_time = (order.x + order.y) / delivery_speed;
                if (delivery_time == 0) delivery_time = 1;
                fprintf(log_file, "Deliverer %d is delivering order %d (%d orders) (Location: (%d, %d), Delivery time: %d seconds, Speed: %d m/min)...\n", deliverer_id, order.order_id, order_count, order.x, order.y, delivery_time, delivery_speed);
                fflush(log_file); // Ensure log is written immediately
                usleep(100000);
                if (cancel_flag) {
                    fprintf(log_file, "Deliverer %d is returning due to order cancellation.\n", deliverer_id);
                    fflush(log_file); // Ensure log is written immediately
                    break;
                }
                fprintf(log_file, "Deliverer %d delivered order %d.\n", deliverer_id, order.order_id);
                fflush(log_file); // Ensure log is written immediately
                pthread_mutex_lock(&queue_mutex);
                pending_orders--;
                if (pending_orders == 0) {
                    pthread_cond_signal(&queue_cond);
                }
                pthread_mutex_unlock(&queue_mutex);

                deliverer_order_count[deliverer_id]++;
            }
            fprintf(log_file, "Deliverer %d returned to the shop.\n", deliverer_id);
            fflush(log_file); // Ensure log is written immediately
        }
    }
    return NULL;
}

void cancel_order() {
    pthread_mutex_lock(&queue_mutex);
    front = rear = count = 0;
    cancel_flag = 1;
    pthread_cond_broadcast(&queue_cond);
    pthread_mutex_unlock(&queue_mutex);

    pthread_mutex_lock(&delivery_mutex);
    delivery_front = delivery_rear = delivery_count = 0;
    pthread_cond_broadcast(&delivery_cond);
    pthread_mutex_unlock(&delivery_mutex);

    fprintf(log_file, "Orders have been cancelled.\n");
    fflush(log_file); // Ensure log is written immediately
}

int main(int argc, char const *argv[]) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};

    if (argc != 5) {
        fprintf(stderr, "Usage: %s [Port] [CookThreadPoolSize] [DeliveryThreadPoolSize] [DeliverySpeed]\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    num_cooks = atoi(argv[2]);
    num_deliverers = atoi(argv[3]);
    delivery_speed = atoi(argv[4]);

    pthread_t cooks[num_cooks];
    pthread_t deliverers[num_deliverers];
    int cook_ids[num_cooks];
    int deliverer_ids[num_deliverers];

    for (int i = 0; i < num_cooks; i++) {
        cook_ids[i] = i + 1;
        pthread_create(&cooks[i], NULL, cook, &cook_ids[i]);
    }

    for (int i = 0; i < num_deliverers; i++) {
        deliverer_ids[i] = i + 1;
        pthread_create(&deliverers[i], NULL, deliver, &deliverer_ids[i]);
    }

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    // Retrieve the host name
    char hostbuffer[256];
    char *IPbuffer;
    struct hostent *host_entry;
    int hostname = gethostname(hostbuffer, sizeof(hostbuffer));
    if (hostname == -1) {
        perror("gethostname");
        exit(EXIT_FAILURE);
    }

    // Retrieve the host information
    host_entry = gethostbyname(hostbuffer);
    if (host_entry == NULL) {
        perror("gethostbyname");
        exit(EXIT_FAILURE);
    }

    // Convert the Internet network address to ASCII string
    IPbuffer = inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0]));

    // Create the log file
    log_file = fopen("pideshop_log.txt", "a");
    if (!log_file) {
        perror("Log file creation failed");
        exit(EXIT_FAILURE);
    }
    fprintf(log_file, "PideShop started\n");
    fflush(log_file); // Ensure log is written immediately

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    while (1) {
        printf("PideShop active, waiting for connection on IP: %s, port: %d\n", IPbuffer, port);

        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        int should_continue = 1;
        while (should_continue) {
            int valread = read(new_socket, buffer, 1024);
            if (valread <= 0) {
                printf("Connection closed.\n");
                break;
            }
            buffer[valread] = '\0';
            if (strcmp(buffer, "cancel") == 0) {
                cancel_order();
                printf("Waiting for new customers...\n");
                should_continue = 0;
            } else if (strcmp(buffer, "keep-alive") == 0) {
                // Do nothing, just keep the connection alive
            } else {
                pthread_mutex_lock(&queue_mutex);
                cancel_flag = 0;
                pthread_mutex_unlock(&queue_mutex);

                int num_customers, p, q;
                sscanf(buffer, "%d %d %d", &num_customers, &p, &q);

                if (num_customers > 0) {
                    printf("%d new customers.. Serving, City dimensions: %d x %d\n", num_customers, p, q);
                    for (int i = 0; i < num_customers; i++) {
                        int x = rand() % p;
                        int y = rand() % q;
                        Order order;
                        order.order_id = i + 1;
                        order.x = x;
                        order.y = y;

                        pthread_mutex_lock(&queue_mutex);
                        enqueue(order_queue, &rear, &count, order);
                        pthread_cond_signal(&queue_cond);
                        pthread_mutex_unlock(&queue_mutex);

                        pthread_mutex_lock(&delivery_mutex);
                        pthread_cond_signal(&delivery_cond);
                        pthread_mutex_unlock(&delivery_mutex);
                    }
                    should_continue = 0;
                }
            }
        }

        pthread_mutex_lock(&queue_mutex);
        while (pending_orders > 0 && !cancel_flag) {
            pthread_cond_wait(&queue_cond, &queue_mutex);
        }
        pthread_mutex_unlock(&queue_mutex);

        if (cancel_flag) {
            printf("All tasks completed. Waiting for new customers...\n");
        }

        printf("Done serving client PID %d\n", getpid());
        fprintf(log_file, "Done serving client PID %d\n", getpid());
        fflush(log_file); // Ensure log is written immediately

        int max_cook_orders = 0;
        int best_cook_id = 0;
        for (int i = 0; i < num_cooks; i++) {
            if (cook_order_count[i + 1] > max_cook_orders) {
                max_cook_orders = cook_order_count[i + 1];
                best_cook_id = i + 1;
            }
        }

        int max_deliverer_orders = 0;
        int best_deliverer_id = 0;
        for (int i = 0; i < num_deliverers; i++) {
            if (deliverer_order_count[i + 1] > max_deliverer_orders) {
                max_deliverer_orders = deliverer_order_count[i + 1];
                best_deliverer_id = i + 1;
            }
        }

        printf("Thanks Cook %d and Moto %d\n", best_cook_id, best_deliverer_id);
        fprintf(log_file, "Thanks Cook %d and Moto %d\n", best_cook_id, best_deliverer_id);
        fflush(log_file); // Ensure log is written immediately

        send(new_socket, "done", strlen("done"), 0);

        close(new_socket);
    }

    for (int i = 0; i < num_cooks; i++) {
        pthread_join(cooks[i], NULL);
    }

    for (int i = 0; i < num_deliverers; i++) {
        pthread_join(deliverers[i], NULL);
    }

    fclose(log_file);
    return 0;
}
