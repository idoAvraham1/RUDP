#include "RUDP.h"


// Function to read content from a file and return it along with its size
void readFromFile(char** file_contant, int* size);

// Global variables
char *fileName = "tosend.txt";

int main(int argc,char** argv) {

     // Check command line arguments
    if (argc != 5) {
        fprintf(stderr, "Usage: %s -IP <receiver_ip> -P <receiver_port> \n", argv[0]);
        exit(1);
    }

    // Parse command line arguments
    unsigned short int port = atoi(argv[4]);
    char *receiver_ip = argv[2];

    // File-related variables
    char *fileContent = NULL;
    int fileSize = 0;


    RUDP_Socket* sock = rudp_socket(false, 0);
    if(sock == NULL){
        return -1;
    }

    printf("Sending connect message to receiver\n");

    if(rudp_connect(sock, receiver_ip, port) == 0){
        printf("Connction to Receiver Failed\n");
        rudp_close(sock);
        return -1;
    }

    printf("got ACK connection successful, sending file\n");

    // read the file
    readFromFile(&fileContent, &fileSize);

    //Send the file to the receiver
    int userChoice = 1;

    while(userChoice) {
        printf("Sending file...\n");

        // Send data in chunks
        RUDPPacket DATA_packet;
        memset(DATA_packet.data,0,BUFFER_SIZE);
        DATA_packet.header.flags = RUDP_DATA;
        int i;
        for(i=0;i+BUFFER_SIZE<fileSize;i+=BUFFER_SIZE) {

            memcpy(DATA_packet.data,fileContent+i,BUFFER_SIZE);
            DATA_packet.header.checksum = calculate_checksum(DATA_packet.data, BUFFER_SIZE);
            DATA_packet.header.length = BUFFER_SIZE;
            if(rudp_send(sock, &DATA_packet, sizeof(DATA_packet)) == -1){
                rudp_close(sock);
                free(fileContent);
                return -1;
            }
        }

        memset(DATA_packet.data,0,BUFFER_SIZE);
        memcpy(DATA_packet.data, fileContent+i,(fileSize-i));
        DATA_packet.header.checksum = calculate_checksum(DATA_packet.data, fileSize-i);
        DATA_packet.header.length = fileSize-i;
        if(rudp_send(sock, &DATA_packet, sizeof(DATA_packet)) == -1){
            rudp_close(sock);
            free(fileContent);
            return -1;
        }

        // Send the EOF
        memset(DATA_packet.data,EOF,BUFFER_SIZE);
        DATA_packet.header.checksum = calculate_checksum(DATA_packet.data, 0);
        DATA_packet.header.length = 0;
        if(rudp_send(sock, &DATA_packet, sizeof(DATA_packet)) == -1){
            rudp_close(sock);
            free(fileContent);
            return -1;
        }

        // waiting for user descision
        printf("Resend the file? 1 for resend, 0 for exit \n");
        scanf("%d",&userChoice);
        
        // send the data agagin
        if(userChoice == 1){
            memset(DATA_packet.data, 0, BUFFER_SIZE);
            memcpy(DATA_packet.data, "yes", 4);
            DATA_packet.header.length = 4;
            DATA_packet.header.checksum = calculate_checksum(DATA_packet.data, DATA_packet.header.length);
            int sendChoice = rudp_send(sock, &DATA_packet, sizeof(DATA_packet));
            if(sendChoice < 0){
                printf("send() failed\n");
                rudp_close(sock);
                free(fileContent);
                return -1;
            }
        }
        // send to the receiver exit 
        if(userChoice == 0){
            printf("Sending disconnect message to receiver\n");
            if(rudp_disconnect(sock) == 0){
                printf("Disconnect to receiver failed\n");
                rudp_close(sock);
                free(fileContent);
                return -1;
            }
            printf("Got Ack from receiver, sender Exit...\n");
        }
    }

    free(fileContent);

    //Close the connection and exit 
    rudp_close(sock);
    return 0;
}


void readFromFile(char** file_content, int* size) {
    FILE *fpointer = NULL;

    fpointer = fopen(fileName, "r");

    if (fpointer == NULL) {
        perror("fopen");
        exit(1);
    }

    // Find the file size and allocate enough memory for it.
    fseek(fpointer, 0L, SEEK_END);
    *size = (int) ftell(fpointer);
    *file_content = (char*) malloc(*size * sizeof(char));
    fseek(fpointer, 0L, SEEK_SET);

    fread(*file_content, sizeof(char), *size, fpointer);
    fclose(fpointer);

    printf("File \"%s\" total size is %d bytes.\n", fileName, *size);

}
