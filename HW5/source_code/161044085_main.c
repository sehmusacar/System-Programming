#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/time.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

// Buffer structure to hold file names and their types
typedef struct {
    char **file_names;
    int *file_types;
    int in, out, count;
    int done_flag;
    int buffer_size;
    pthread_mutex_t mutex;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
} Buffer;

Buffer buffer;

// Statistics structure to keep track of file counts and total bytes copied
typedef struct {
    int regular_files;
    int fifo_files;
    int directories;
    long total_bytes;
    pthread_mutex_t stats_mutex; // Mutex for statistics
} Statistics;

Statistics stats;

pthread_barrier_t barrier; // Barrier to synchronize worker threads

// Function to initialize the buffer
void initialize_buffer(int size) {
    buffer.file_names = (char **)malloc(size * sizeof(char *));
    buffer.file_types = (int *)malloc(size * sizeof(int));
    buffer.in = 0;
    buffer.out = 0;
    buffer.count = 0;
    buffer.done_flag = 0;
    buffer.buffer_size = size;
    pthread_mutex_init(&buffer.mutex, NULL);
    pthread_cond_init(&buffer.not_full, NULL);
    pthread_cond_init(&buffer.not_empty, NULL);
    pthread_mutex_init(&stats.stats_mutex, NULL); // Initialize stats mutex
}

// Function to clean up the buffer
void free_buffer() {
    free(buffer.file_names);
    free(buffer.file_types);
    pthread_mutex_destroy(&buffer.mutex);
    pthread_cond_destroy(&buffer.not_full);
    pthread_cond_destroy(&buffer.not_empty);
    pthread_mutex_destroy(&stats.stats_mutex); // Destroy stats mutex
}

// Function to create directories recursively
int mkdir_p(const char *path, mode_t mode) {
    char temp[PATH_MAX];
    char *p = NULL;
    size_t len;

    snprintf(temp, sizeof(temp), "%s", path);
    len = strlen(temp);
    if (temp[len - 1] == '/') {
        temp[len - 1] = 0;
    }
    for (p = temp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(temp, mode) != 0) {
                if (errno != EEXIST) {
                    return -1;
                }
            }
            *p = '/';
        }
    }
    if (mkdir(temp, mode) != 0) {
        if (errno != EEXIST) {
            return -1;
        }
    }
    return 0;
}

// Function to copy directory contents
void enqueue_directory_contents(char *directory_path) {
    DIR *dir = opendir(directory_path);
    if (!dir) {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue; // Skip '.' and '..' entries
        }

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", directory_path, entry->d_name);

        struct stat file_stat;
        if (stat(full_path, &file_stat) == 0) {
            pthread_mutex_lock(&buffer.mutex);
            while (buffer.count == buffer.buffer_size) {
                pthread_cond_wait(&buffer.not_full, &buffer.mutex);
            }

            buffer.file_names[buffer.in] = strdup(full_path);
            buffer.file_types[buffer.in] = file_stat.st_mode;
            buffer.in = (buffer.in + 1) % buffer.buffer_size;
            buffer.count++;

            if (S_ISREG(file_stat.st_mode)) {
                stats.regular_files++;
            } else if (S_ISFIFO(file_stat.st_mode)) {
                stats.fifo_files++;
            } else if (S_ISDIR(file_stat.st_mode)) {
                stats.directories++;
            }

            pthread_cond_signal(&buffer.not_empty);
            pthread_mutex_unlock(&buffer.mutex);

            if (S_ISDIR(file_stat.st_mode)) {
                enqueue_directory_contents(full_path); // Recursively enqueue directory contents
            }
        } else {
            perror("stat");
        }
    }

    closedir(dir);
}

// Manager thread function to read source directory and enqueue files
void *manager_function(void *arg) {
    char *source_directory = (char *)arg;
    enqueue_directory_contents(source_directory);

    pthread_mutex_lock(&buffer.mutex);
    buffer.done_flag = 1;
    pthread_cond_broadcast(&buffer.not_empty);
    pthread_mutex_unlock(&buffer.mutex);

    return NULL;
}

// Worker thread function to copy files from source to destination
void *worker_function(void *arg) {
    char *source_directory = ((char **)arg)[0];
    char *destination_directory = ((char **)arg)[1];

    while (1) {
        pthread_mutex_lock(&buffer.mutex);
        while (buffer.count == 0 && !buffer.done_flag) {
            pthread_cond_wait(&buffer.not_empty, &buffer.mutex);
        }

        if (buffer.count == 0 && buffer.done_flag) {
            pthread_mutex_unlock(&buffer.mutex);
            break;
        }

        char *file_name = buffer.file_names[buffer.out];
        int file_type = buffer.file_types[buffer.out];
        buffer.out = (buffer.out + 1) % buffer.buffer_size;
        buffer.count--;

        pthread_cond_signal(&buffer.not_full);
        pthread_mutex_unlock(&buffer.mutex);

        char source_path[PATH_MAX];
        char destination_path[PATH_MAX];
        snprintf(source_path, sizeof(source_path), "%s", file_name);
        snprintf(destination_path, sizeof(destination_path), "%s/%s", destination_directory, file_name + strlen(source_directory) + 1);

        if (S_ISDIR(file_type)) {
            mkdir_p(destination_path, 0755);
        } else if (S_ISREG(file_type)) {
            char dir_path[PATH_MAX];
            strcpy(dir_path, destination_path);
            char *last_slash = strrchr(dir_path, '/');
            if (last_slash != NULL) {
                *last_slash = '\0';
                mkdir_p(dir_path, 0755); // Ensure the destination directory exists
            }

            int src_fd = open(source_path, O_RDONLY);
            if (src_fd < 0) {
                perror("open src");
                free(file_name);
                continue;
            }

            int dest_fd = open(destination_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (dest_fd < 0) {
                perror("open dest");
                close(src_fd);
                free(file_name);
                continue;
            }

            char buffer[BUFSIZ];
            ssize_t bytes_read;
            while ((bytes_read = read(src_fd, buffer, BUFSIZ)) > 0) {
                write(dest_fd, buffer, bytes_read);

                pthread_mutex_lock(&stats.stats_mutex); // Lock stats mutex
                stats.total_bytes += bytes_read;
                pthread_mutex_unlock(&stats.stats_mutex); // Unlock stats mutex
            }

            close(src_fd);
            close(dest_fd);
        } else if (S_ISFIFO(file_type)) {
            if (mkfifo(destination_path, 0666) < 0) {
                perror("mkfifo");
            }
        }

        free(file_name);
    }

    pthread_barrier_wait(&barrier); // Barrier synchronization
    return NULL;
}

// Signal handler to terminate the program gracefully
void signal_handler(int sig) {
    printf("\nTerminating program...\n");
    exit(0);
}

// Main function to set up the manager and worker threads and measure execution time
int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: %s <buffer size> <number of workers> <source dir> <dest dir>\n", argv[0]);
        return 1;
    }

    int buffer_size = atoi(argv[1]);
    int num_workers = atoi(argv[2]);
    char *source_directory = argv[3];
    char *destination_directory = argv[4];

    // If destination folder doesn't exist, create it
    struct stat st = {0};
    if (stat(destination_directory, &st) == -1) {
        mkdir_p(destination_directory, 0755);
    }

    initialize_buffer(buffer_size);
    memset(&stats, 0, sizeof(stats));
    pthread_barrier_init(&barrier, NULL, num_workers); // Initialize barrier

    signal(SIGINT, signal_handler);

    pthread_t manager_thread;
    pthread_t worker_threads[num_workers];
    char *worker_args[2] = {source_directory, destination_directory};

    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);

    // Create manager thread
    pthread_create(&manager_thread, NULL, manager_function, (void *)source_directory);
    // Create worker threads
    for (int i = 0; i < num_workers; i++) {
        pthread_create(&worker_threads[i], NULL, worker_function, (void *)worker_args);
    }

    // Wait for manager thread to finish
    pthread_join(manager_thread, NULL);
    // Wait for all worker threads to finish
    for (int i = 0; i < num_workers; i++) {
        pthread_join(worker_threads[i], NULL);
    }

    gettimeofday(&end_time, NULL);
    double elapsed_time = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_usec - start_time.tv_usec) / 1000000.0;

    // Print statistics
    printf("\n---------------STATISTICS--------------------\n");
    printf("Consumers: %d - Buffer Size: %d\n", num_workers, buffer_size);
    printf("Number of Regular Files: %d\n", stats.regular_files);
    printf("Number of FIFO Files: %d\n", stats.fifo_files);
    printf("Number of Directories: %d\n", stats.directories);
    printf("TOTAL BYTES COPIED: %ld\n", stats.total_bytes);
    printf("TOTAL TIME: %02ld:%02ld.%03ld (min:sec.mili)\n", 
           (long)elapsed_time / 60, (long)elapsed_time % 60, 
           (long)((elapsed_time - (long)elapsed_time) * 1000));

    free_buffer();
    pthread_barrier_destroy(&barrier); // Destroy barrier

    return 0;
}
