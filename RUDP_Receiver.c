#include "RUDP.h"
#include <stdio.h>

#define MAX_RUNS 50



// Structure to store statistics for each run
struct RunStatistics {
    double time;    // Time taken for the run in milliseconds
    double speed;   // Data transfer speed in MB/s
};

void printStatistics(struct RunStatistics* statistics, int numRuns);
void calcTime(int fileSize, struct timeval start, struct RunStatistics* runStatistics, int numRuns);

int main(int argc,char** argv) {

   // Check command line arguments
    if (argc != 3) {
        fprintf(stderr, "Usage: %s -p <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // time statistics variables
    struct RunStatistics runStatistics[MAX_RUNS];
    int numRuns = 0;
    struct timeval start;

    // Parse command line arguments
    unsigned short int port = atoi(argv[2]);

    RUDP_Socket* sock = rudp_socket(true, port);
    if(sock == NULL){
        return -1;
    }

    printf("Starting Receiver...\n");

    //Get a connection from the sender
    printf("Waiting for RUDP Connection...\n");
    int connection_status = rudp_accept(sock);
    if(connection_status == 0){
        rudp_close(sock);
        return -1;
    }
    printf("Sender connected, beginning to receive file...\n");

    // Receive the file.
    int keep_receiving = 1;

    while(keep_receiving) {

        // count the total of bytes received
        int totalReceived = 0;
        char data[BUFFER_SIZE];

        // start measuring time
        gettimeofday(&start,NULL);

        while (1) {
            memset(data, 0, sizeof(data));
            int receiveResult = rudp_recv(sock, data, sizeof(data));

            // add the received data to the total sent
            if(receiveResult > 0){totalReceived+=receiveResult;}
            
            // if failed return -1,
            if (receiveResult == -1) {
                rudp_close(sock);
                return -1;
            }

            //if got SYN retransmission
            if(receiveResult == -2){
                gettimeofday(&start,NULL);
            }

            //if got EOF break
            if (receiveResult == -3) {
                printf("File transfer completed\n");
                printf("Ack sent\n");

                // data sent. calc the time it took
                calcTime(totalReceived, start, runStatistics, numRuns);
                numRuns++;

                break;
            }
        }

        // Wait for Sender response:
        printf("Waiting for Sender response...\n");

        while(1) {

            int receiveChoice = rudp_recv(sock, data, sizeof(data));

            // if no response, exit
            if(receiveChoice == -1){
                rudp_close(sock);
                return -1;
            }

            // handle case where sender wants to exit
            if(receiveChoice == 0) {
                printf("Sender sent exit message...\n");
                printf("ACK sent\n");
                keep_receiving = 0;
                break;
            }
            // handle case where sender is sending again
            if(strcmp(data, "yes") == 0){
                printf("Sender sending  again...\n");
                totalReceived = 0;
                gettimeofday(&start, NULL);  // Reset start time for the new run
                break;
            }

        }

    }


    
     // Print statistics after receiving the exit message
    printf("----------------------------------\n");
    printf("- * Statistics * -\n");

    for (int i = 0; i < numRuns; i++) {
        printf("- Run #%d Data: Time=%.2fms; Speed=%.2fMB/s\n", i + 1, runStatistics[i].time, runStatistics[i].speed);
    }

    // Calculate and print averages
    printStatistics(runStatistics, numRuns);

    printf("----------------------------------\n");

    // Exit and close connections
    rudp_close(sock);
    return 0;
}

// Function to print statistics for each run
void printStatistics(struct RunStatistics* statistics, int numRuns) {
    double totalTime = 0.0;
    double totalSpeed = 0.0;

    for (int i = 0; i < numRuns; i++) {
        totalTime += statistics[i].time;
        totalSpeed += statistics[i].speed;
    }

    double avgTime = totalTime / numRuns;
    double avgSpeed = totalSpeed / numRuns;

    printf("- Average time: %.2fms\n", avgTime);
    printf("- Average bandwidth: %.2fMB/s\n", avgSpeed);
}

// Function to calculate time and speed for a run
void calcTime(int fileSize, struct timeval start, struct RunStatistics* runStatistics, int numRuns) {
    struct timeval end;
    gettimeofday(&end, NULL);
    double elapsedTime = (end.tv_sec - start.tv_sec) * 1000.0;  // Convert to milliseconds
    elapsedTime += (end.tv_usec - start.tv_usec) / 1000.0;

    double speed = (fileSize / elapsedTime) * 1000.0 / (1024 * 1024);  // Speed in MB/s

    runStatistics[numRuns].time = elapsedTime;
    runStatistics[numRuns].speed = speed;
}