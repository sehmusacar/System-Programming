#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define MAX_PICKUP_SPOTS 4
#define MAX_AUTOMOBILE_SPOTS 8

// Semaphore declarations
sem_t newPickup, inChargeforPickup, newAutomobile, inChargeforAutomobile;

// Global variables for available parking spots
int mFree_pickup = MAX_PICKUP_SPOTS;
int mFree_automobile = MAX_AUTOMOBILE_SPOTS;
int perm_free_pickup = MAX_PICKUP_SPOTS;
int perm_free_automobile = MAX_AUTOMOBILE_SPOTS;

void* carOwner(void* arg) {
    int vehicleType = *((int*)arg); // Get vehicle type (0 for pickup, 1 for automobile)

    if (vehicleType % 2 == 0) {
        // If vehicle type is pickup
        if (mFree_pickup > 0) {
            mFree_pickup--;
            printf("[Car Owner] Pickup arrived at the temporary parking area. Current available temporary pickup spots: %d\n", mFree_pickup);
            sem_post(&newPickup); // Signal the valet to take the vehicle
        } else {
            printf("[Car Owner] No available spot in the temporary pickup parking area. Pickup is leaving.\n");
        }
    } else {
        // If vehicle type is automobile
        if (mFree_automobile > 0) {
            mFree_automobile--;
            printf("[Car Owner] Automobile arrived at the temporary parking area. Current available temporary automobile spots: %d\n", mFree_automobile);
            sem_post(&newAutomobile); // Signal the valet to take the vehicle
        } else {
            printf("[Car Owner] No available spot in the temporary automobile parking area. Automobile is leaving.\n");
        }
    }

    free(arg); // Free dynamically allocated memory for vehicle type
    return NULL;
}

void* carAttendant(void* arg) {
    int vehicleType = *((int*)arg); // Get vehicle type (0 for pickup, 1 for automobile)

    while (1) {
        if (vehicleType % 2 == 0) {
            // If vehicle type is pickup
            sem_wait(&newPickup); // Wait for a pickup to be available in the temporary parking area
            sem_wait(&inChargeforPickup); // Ensure only one valet is in charge of the pickup

            mFree_pickup++;
            printf("[Car Attendant] Pickup removed from the temporary parking area. Current available temporary pickup spots: %d\n", mFree_pickup);

            sleep(1); // Simulate the time taken to park the vehicle

            perm_free_pickup--;
            printf("[Car Attendant] Pickup parked in the permanent parking area. Current available permanent pickup spots: %d\n", perm_free_pickup);
            sem_post(&inChargeforPickup); // Release the charge for the next pickup
        } else {
            // If vehicle type is automobile
            sem_wait(&newAutomobile); // Wait for an automobile to be available in the temporary parking area
            sem_wait(&inChargeforAutomobile); // Ensure only one valet is in charge of the automobile

            mFree_automobile++;
            printf("[Car Attendant] Automobile removed from the temporary parking area. Current available temporary automobile spots: %d\n", mFree_automobile);

            sleep(1); // Simulate the time taken to park the vehicle

            perm_free_automobile--;
            printf("[Car Attendant] Automobile parked in the permanent parking area. Current available permanent automobile spots: %d\n", perm_free_automobile);
            sem_post(&inChargeforAutomobile); // Release the charge for the next automobile
        }
    }

    return NULL;
}

int main() {
    pthread_t owners[12], attendants[2];
    int* vehicleType;

    // Print initial available spots information
    printf("Initial available temporary pickup spots: %d\n", mFree_pickup);
    printf("Initial available temporary automobile spots: %d\n\n", mFree_automobile);

    // Initialize semaphores
    sem_init(&newPickup, 0, 0);
    sem_init(&inChargeforPickup, 0, 1);
    sem_init(&newAutomobile, 0, 0);
    sem_init(&inChargeforAutomobile, 0, 1);

    // Create valet threads
    for (int i = 0; i < 2; i++) {
        vehicleType = (int*)malloc(sizeof(int));
        *vehicleType = i; // 0 for pickup, 1 for automobile
        pthread_create(&attendants[i], NULL, carAttendant, vehicleType);
    }

    // Create car owner threads
    for (int i = 0; i < 12; i++) {
        vehicleType = (int*)malloc(sizeof(int));
        *vehicleType = rand() % 2; // Randomly 0 or 1
        pthread_create(&owners[i], NULL, carOwner, vehicleType);
        sleep(2); // Simulate arrival time
    }

    // Join car owner threads
    for (int i = 0; i < 12; i++) {
        pthread_join(owners[i], NULL);
    }

    // Cancel valet threads
    for (int i = 0; i < 2; i++) {
        pthread_cancel(attendants[i]);
        pthread_join(attendants[i], NULL);
    }

    // Destroy semaphores
    sem_destroy(&newPickup);
    sem_destroy(&inChargeforPickup);
    sem_destroy(&newAutomobile);
    sem_destroy(&inChargeforAutomobile);

    return 0;
}
