#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_STUDENTS 10000
#define MAX_LINE_LENGTH 256

// Function prototypes

// Checks if the file exists and creates it if necessary
void checkAndCreateFile(const char * fileName);

// Adds a new student grade to the specified file
void addStudentGrade(const char * fileName, const char * name, const char * grade);

// Searches for a student by name in the specified file
void searchStudent(const char * fileName, const char * name);

// Sorts all student grades in the specified file based on the sorting method
void sortAll(const char * fileName, int sortingMethod);

// Shows all student grades in the specified file
void showAll(const char * fileName);

// Lists the first 5 student grades in the specified file
void listGrades(const char * fileName);

// Lists a specified number of student grades on a specified page from the file
void listSome(const char * fileName, int numOfEntries, int pageNumber);

// Writes a log message to the log file
void writeLog(const char * message);

// Starts the interactive mode with the specified default file name
void interactiveMode(const char * fileName);

int main(int argc, char * argv[]) {
    char gradesFileName[MAX_LINE_LENGTH] = "grades.txt"; // Default file name

  // Check command-line arguments
    if (argc > 1 && strcmp(argv[1], "gtuStudentGrades") == 0 && argc == 3) {
    strcpy(gradesFileName, argv[2]); // Copy the file name specified by the user
    checkAndCreateFile(gradesFileName); // Check if the file exists and create it if necessary
    printf("File created or already exists: %s\n", gradesFileName);
    return 0; // Exit the program successfully
    }

  // Start interactive mode if no specific file name is given
    interactiveMode(gradesFileName);
    return 0;
}


void interactiveMode(const char *defaultFileName) {
    char input[MAX_LINE_LENGTH], fileName[MAX_LINE_LENGTH], name[MAX_LINE_LENGTH], grade[3];
    int numOfEntries, pageNumber;

    printf("Enter the command ('q' to quit / 'gtuStudentGrades' to help):\n");

    while (1) {
        printf("> ");
        if (!fgets(input, sizeof(input), stdin)) {
            break; // fgets failed
        }
        input[strcspn(input, "\n")] = 0; // Remove newline character

        if (strcmp(input, "q") == 0) {
            break; // Exit the loop
        }

        if (strcmp(input, "gtuStudentGrades") == 0) {
            // Perform necessary operations for the help command
            printf("Available commands:\n");
            printf("gtuStudentGrades \"fileName\" - Creates a new file or opens an existing file\n");
            printf("showAll \"fileName\" - Shows all student grades in the specified file\n");
            printf("addStudentGrade \"name\" \"grade\" \"fileName\" - Adds a new student and grade to the file\n");
            printf("searchStudent \"name\" \"fileName\" - Searches for the grade of a student by name\n");
            printf("listSome numOfEntries pageNumber \"fileName\" - Lists the specified number of student grades on a specific page\n");
            printf("sortAll \"fileName\" - Sorts all student grades alphabetically by name\n");
            printf("showAll - Shows all student grades in the default file\n");
            printf("listGrades - Lists all grades in the default file\n");
            continue;
        }

        // Parse and process commands
        if (sscanf(input, "gtuStudentGrades \"%[^\"]\"", fileName) == 1) {
            checkAndCreateFile(fileName);
            printf("File created or already exists: %s\n", fileName);
        } else if (sscanf(input, "showAll \"%[^\"]\"", fileName) == 1) {
            showAll(fileName);
        } else if (sscanf(input, "addStudentGrade \"%[^\"]\" \"%[^\"]\" \"%[^\"]\"", name, grade, fileName) == 3) {
            addStudentGrade(fileName, name, grade);
        } else if (sscanf(input, "addStudentGrade \"%[^\"]\" \"%[^\"]\"", name, grade) == 2) {
            printf("Error: You must specify a file name. Correct usage: addStudentGrade \"name\" \"grade\" \"fileName\"\n");
        } else if (sscanf(input, "searchStudent \"%[^\"]\" \"%[^\"]\"", name, fileName) == 2) {
            searchStudent(fileName, name);
        } else if (sscanf(input, "searchStudent \"%[^\"]\"", name) == 1) {
            searchStudent(defaultFileName, name);
        } else if (sscanf(input, "listSome %d %d \"%[^\"]\"", &numOfEntries, &pageNumber, fileName) == 3) {
            listSome(fileName, numOfEntries, pageNumber);
        } else if (sscanf(input, "listSome %d %d", &numOfEntries, &pageNumber) == 2) {
            listSome(defaultFileName, numOfEntries, pageNumber);
        } else if (strcmp(input, "sortAll") == 0 || sscanf(input, "sortAll \"%[^\"]\"", fileName) == 1) {
            int sortingMethod;
            printf("Select sorting method:\n");
            printf("1. Alphabetically by name\n");
            printf("2. From lowest to highest grade\n");
            printf("3. From highest to lowest  grade\n");
            printf("> ");
            scanf("%d", &sortingMethod);
            // Consume the rest of the line
            while (getchar() != '\n');
            
            if (sortingMethod >= 1 && sortingMethod <= 3) {
                sortAll(strcmp(input, "sortAll") == 0 ? defaultFileName : fileName, sortingMethod);
            } else {
                printf("Invalid sorting method. Please enter a number between 1 and 3.\n");
            }
        }
         else if (strcmp(input, "showAll") == 0) {
            showAll(defaultFileName);
        } else if (strcmp(input, "listGrades") == 0) {
            listGrades(defaultFileName);
        } else if (sscanf(input, "listGrades \"%[^\"]\"", fileName) == 1) {
            listGrades(fileName);
        } else {
            printf("Unknown command or incorrect format.For help to 'GtuStudentGrades'\n");
        }
    }
}


void checkAndCreateFile(const char *fileName) {
    if (access(fileName, F_OK) == -1) {
        // File does not exist, create it
        pid_t pid = fork();

        if (pid == -1) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Child process
            int fd = open(fileName, O_CREAT, S_IRUSR | S_IWUSR);
            if (fd == -1) {
                perror("Failed to create file");
                exit(EXIT_FAILURE);
            }
            close(fd);
            writeLog("checkAndCreateFile");
            exit(EXIT_SUCCESS);
        } else {
            // Parent process, wait for child process to finish
            wait(NULL);
        }
    }
}

void addStudentGrade(const char *fileName, const char *name, const char *grade) {
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        int fd = open(fileName, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
        if (fd == -1) {
            perror("Failed to open file");
            exit(EXIT_FAILURE);
        }

        char logMessage[256];
        sprintf(logMessage, "%s %s\n", name, grade); // Format the name and grade
        if (write(fd, logMessage, strlen(logMessage)) == -1) {
            perror("Failed to write to file");
            close(fd);
            exit(EXIT_FAILURE);
        }

        sprintf(logMessage, "Added grade: %s %s", name, grade); // Prepare the log message
        writeLog(logMessage);

        close(fd);
        exit(EXIT_SUCCESS);
    } else {
        wait(NULL); // Wait for the child process to finish
        printf("Added Student: %s %s\n", name, grade);
    }
}


void searchStudent(const char *fileName, const char *name) {
    pid_t pid = fork();

    if (pid == -1) {
        // Fork failed
        perror("fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        int fd = open(fileName, O_RDONLY);
        if (fd == -1) {
            perror("Failed to open file");
            exit(EXIT_FAILURE);
        }

        char buffer[1024];
        ssize_t bytesRead;
        int found = 0;
        char *lineStart = buffer; // Pointer to track the beginning of each line
        char *lineEnd;

        while ((bytesRead = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytesRead] = '\0'; // Append null character to end of buffer to create a string
            lineStart = buffer; // Reset lineStart after each new read operation

            while ((lineEnd = strchr(lineStart, '\n')) != NULL) {
                *lineEnd = '\0'; // Replace newline character with null terminator to end the line

                if (strstr(lineStart, name)) { // If the searched name is found in the line
                    printf("Student found:\n");
                    printf("%s\n", lineStart);
                    found = 1;
                    break; // Exit the loop after finding the first matching line
                }

                lineStart = lineEnd + 1; // Move to the beginning of the next line
            }

            if (found) break; // Exit the outer loop if the searched name is found
        }

        if (!found) {
            printf("Student not found.\n");
        }

        writeLog("searchStudent"); // Assuming writeLog handles its own process management
        close(fd);
        exit(EXIT_SUCCESS); // Terminate child process successfully
    } else {
        // Parent process, wait for the child process to finish
        wait(NULL);
    }
}


typedef struct {
    char name[MAX_LINE_LENGTH];
    char grade[3]; // Grades contain two characters like "AA", "BB" along with a null terminator.
} Student;

int compareByName(const void *a, const void *b) {
    const Student *studentA = (const Student *)a;
    const Student *studentB = (const Student *)b;
    return strcasecmp(studentA->name, studentB->name);
}

int compareByGradeDesc(const void *a, const void *b) {
    const Student *studentA = (const Student *)a;
    const Student *studentB = (const Student *)b;
    return strcasecmp(studentB->grade, studentA->grade); // Not: Burada basit bir karşılaştırma yapıyoruz, gerçek not karşılaştırması farklı olabilir.
}

int compareByGradeAsc(const void *a, const void *b) {
    const Student *studentA = (const Student *)a;
    const Student *studentB = (const Student *)b;
    return strcasecmp(studentA->grade, studentB->grade);
}

void sortAll(const char *fileName, int sortingMethod) {
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        int fd = open(fileName, O_RDONLY);
        if (fd == -1) {
            perror("Failed to open file for reading");
            exit(EXIT_FAILURE);
        }

        FILE *file = fdopen(fd, "r");
        if (!file) {
            perror("Failed to open file as FILE *");
            exit(EXIT_FAILURE);
        }

        Student students[MAX_STUDENTS];
        char line[MAX_LINE_LENGTH];
        int numStudents = 0;

        while (fgets(line, sizeof(line), file) && numStudents < MAX_STUDENTS) {
            line[strcspn(line, "\n")] = 0; // Remove the newline character at the end of each line
            char *token = strrchr(line, ' '); // Find the last space
            if (token) {
                strcpy(students[numStudents].grade, token + 1); // Copy the grade
                *token = '\0'; // Separate name and grade
                strcpy(students[numStudents].name, line); // Copy the name
            }
            numStudents++;
        }
        fclose(file);

        // Determine the sorting function based on the sortingMethod parameter
        int (*compareFunc)(const void *, const void *);
        switch (sortingMethod) {
            case 1:
                compareFunc = compareByName;
                break;
            case 2:
                compareFunc = compareByGradeDesc;
                break;
            case 3:
                compareFunc = compareByGradeAsc;
                break;
            default:
                fprintf(stderr, "Invalid sorting method.\n");
                exit(EXIT_FAILURE);
        }

        // Sort students using the determined comparison function
        qsort(students, numStudents, sizeof(Student), compareFunc);

        fd = open(fileName, O_WRONLY | O_TRUNC);
        if (fd == -1) {
            perror("Failed to open file for writing");
            exit(EXIT_FAILURE);
        }

        FILE *outputFile = fdopen(fd, "w");
        if (!outputFile) {
            perror("Failed to open file as FILE *");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < numStudents; i++) {
            fprintf(outputFile, "%s %s\n", students[i].name, students[i].grade);
        }

        fclose(outputFile);
        // Assuming writeLog is implemented elsewhere
        writeLog("Sorted grades");
        exit(EXIT_SUCCESS);
    } else {
        wait(NULL); // Wait for the child process to finish
        printf("Operation completed successfully.\n"); 
    }
}

void showAll(const char *fileName) {
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        int fd = open(fileName, O_RDONLY);
        if (fd == -1) {
            perror("Failed to open file for reading");
            exit(EXIT_FAILURE);
        }

        char buffer[4096];
        ssize_t bytesRead;

        while ((bytesRead = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytesRead] = '\0';
            printf("%s\n", buffer);
        }

        if (bytesRead == -1) {
            perror("Failed to read from file");
            close(fd);
            exit(EXIT_FAILURE);
        }

        close(fd);
        writeLog("ShowAll grades");
        exit(EXIT_SUCCESS);
    } else {
        wait(NULL);
    }
}

void listGrades(const char *fileName) {
    pid_t pid = fork();

    if (pid == -1) {
        // Fork failed
        perror("fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        int fd = open(fileName, O_RDONLY);
        if (fd == -1) {
            perror("Failed to open file");
            exit(EXIT_FAILURE);
        }

        ssize_t bytesRead;
        char buffer[MAX_LINE_LENGTH];
        int linesPrinted = 0;

        // Read records and print the first 5
        while ((bytesRead = read(fd, buffer, sizeof(buffer) - 1)) > 0 && linesPrinted < 5) {
            buffer[bytesRead] = '\0'; // Make the string safe by adding a null-terminator
            char *line = buffer;
            while (*line && linesPrinted < 5) {
                char *nextLine = strchr(line, '\n');
                if (nextLine)
                    *nextLine = '\0'; // Replace with null-terminator if next line is found

                printf("%s\n", line);
                linesPrinted++;

                if (nextLine)
                    line = nextLine + 1;
                else
                    break;
            }
        }

        if (bytesRead == -1) {
            perror("Failed to read file");
        }

        close(fd);
        writeLog("Listed first 5 grades");
        exit(EXIT_SUCCESS);
    } else {
        // Parent process, wait for the child process to finish
        wait(NULL);
    }
}


void listSome(const char *fileName, int numOfEntries, int pageNumber) {
    pid_t pid = fork();

    if (pid == -1) {
        // Fork failed
        perror("fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        int fd = open(fileName, O_RDONLY);
        if (fd == -1) {
            perror("Failed to open file");
            exit(EXIT_FAILURE);
        }

        int startLine = numOfEntries * (pageNumber - 1);
        int endLine = startLine + numOfEntries;
        char buffer[MAX_LINE_LENGTH * numOfEntries];
        int currentLine = 0, printedLines = 0;
        ssize_t bytesRead;
        char *next = NULL, *line = NULL;

        // Read file content
        while ((bytesRead = read(fd, buffer, sizeof(buffer) - 1)) > 0 && printedLines < numOfEntries) {
            buffer[bytesRead] = '\0'; // Make the string safe by adding a null-terminator
            line = buffer;

            while ((next = strchr(line, '\n')) != NULL && printedLines < numOfEntries) {
                *next = '\0'; // Replace line ending with null character
                if (currentLine >= startLine && currentLine < endLine) {
                    printf("%s\n", line);
                    printedLines++;
                }
                line = next + 1; // Move to the next line
                currentLine++;
            }

            // If the next line is outside the read buffer or the desired number of entries is printed, exit the loop
            if (printedLines >= numOfEntries || next == NULL)
                break;
        }

        if (bytesRead == -1) {
            perror("Failed to read file");
        }

        close(fd);

        char logMessage[256];
        sprintf(logMessage, "Listed grades on page %d with %d entries per page.", pageNumber, numOfEntries);
        writeLog(logMessage);
        exit(EXIT_SUCCESS);
    } else {
        // Parent process, wait for the child process to finish
        wait(NULL);
    }
}

void writeLog(const char *message) {
    int fd = open("log.txt", O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        perror("Failed to open log file");
        exit(EXIT_FAILURE);
    }

    // Write the message and a newline character to the file
    if (write(fd, message, strlen(message)) == -1 || write(fd, "\n", 1) == -1) {
        perror("Failed to write to log file");
    }

    close(fd); // Close the log file
}
