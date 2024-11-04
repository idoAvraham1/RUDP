#include "RUDP.h"

/**
 * Allocates and Creates a new RUDP socket.
 *
 * @param isServer True if creating a server socket, false for a client socket.
 * @param listen_port The port to listen on for incoming connections (only for servers).
 * @return A pointer to the created RUDP socket, or NULL if an error occurs.
 */
RUDP_Socket* rudp_socket(bool isServer, unsigned short int listen_port){

    RUDP_Socket* sock = (RUDP_Socket*)malloc(sizeof(RUDP_Socket));
    if(sock == NULL){
        perror("Error in RUDP socket allocation\n");
        return NULL;
    }

    sock->socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sock->socket_fd == -1){
        perror("Error creating UDP socket\n");
        free(sock);
        return NULL;
    }

    // Initialize other fields
    memset(&(sock->dest_addr), 0, sizeof(struct sockaddr_in)); // Initialize destination address structure
    sock->isServer = isServer; // Set state based on the isServer parameter
    sock->isConnected = false; // Set initial connection state

    //Initialize a server
    if(isServer){
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(listen_port);

        if(bind(sock->socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1){
            perror("Error binding UDP socket");
            close(sock->socket_fd);
            free(sock);
            return NULL;
        }
    }
    else{
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 1000;

        setsockopt(sock->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    }

    return sock;
}

/**
 * Initiates a connection to a remote RUDP socket.
 *
 * @param sockfd Pointer to the RUDP socket.
 * @param dest_ip IP address of the remote socket.
 * @param dest_port Port of the remote socket.
 * @return 1 if the connection is successful, 0 if an error occurs.
 */
int rudp_connect(RUDP_Socket *sockfd, const char *dest_ip, unsigned short int dest_port){

    if (sockfd == NULL || dest_ip == NULL || sockfd->isConnected || sockfd->isServer) {
        return 0;  // Failure
    }

    struct sockaddr_in server_addr, recv_addr;
    socklen_t recv_addrlen;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(dest_port);

    if(inet_aton(dest_ip, &server_addr.sin_addr) == 0){
        perror("Invalid destination IP\n");
        return 0;  // Failure
    }

    RUDPPacket SYN_packet;
    memset(SYN_packet.data, 0, BUFFER_SIZE);
    SYN_packet.header.length = 0;
    SYN_packet.header.checksum = 0;
    SYN_packet.header.flags = RUDP_SYN;

    while(1){
        if(sendto(sockfd->socket_fd, &SYN_packet, sizeof(SYN_packet), 0,
                  (struct sockaddr*) &server_addr, sizeof(server_addr))  == -1){

            perror("Error sending SYN Packet\n");
            return 0;  // Failure
        }

        RUDPPacket answer;
        recv_addrlen = sizeof(recv_addr);
        int num_bytes = recvfrom(sockfd->socket_fd, &answer, sizeof(answer), 0,
                                 (struct sockaddr*) &(recv_addr), &recv_addrlen);
        if(num_bytes == -1){

            if(errno == EWOULDBLOCK || errno == EAGAIN){
                printf("Timeout occurred, sending connect request again\n");
                continue;
            }
            else{
                perror("Receive failed\n");
                return 0;
            }
        }
        else{

            if(memcmp(&recv_addr.sin_addr, &server_addr.sin_addr, sizeof(struct in_addr)) == 0 &&
                    recv_addr.sin_port == server_addr.sin_port){

                if(answer.header.flags == RUDP_ACK){
                    sockfd->dest_addr.sin_family = AF_INET;
                    sockfd->dest_addr.sin_port = htons(dest_port);
                    inet_aton(dest_ip, &(sockfd->dest_addr.sin_addr));
                    sockfd->isConnected = true;
                    return 1;
                }
                else{
                    perror("Wrong packet received\n");
                    return 0;
                }
            }
            else{
                perror("Received a packet from an unexpected source\n");
                return 0;
            }


        }


    }

}

/**
 * Accepts an incoming connection request on a server RUDP socket.
 *
 * @param sockfd Pointer to the RUDP socket.
 * @return 1 if the connection is accepted, 0 if an error occurs.
 */
int rudp_accept(RUDP_Socket *sockfd){

    if (sockfd == NULL || sockfd->isConnected || !(sockfd->isServer)) {
        return 0;  // Failure
    }

    RUDPPacket packet;
    socklen_t recv_addrlen = sizeof(struct sockaddr_in);

    int num_bytes = recvfrom(sockfd->socket_fd, &packet, sizeof(packet), 0,
                             (struct sockaddr*) &sockfd->dest_addr, &recv_addrlen);
    if(num_bytes == -1){
        perror("Error in receiving connection requests\n");
        return 0;
    }
    else{
        if(packet.header.flags == RUDP_SYN){
            RUDPHeader ACK_packet;
            ACK_packet.length = 0;
            ACK_packet.checksum = 0;
            ACK_packet.flags = RUDP_ACK;

            printf("Connection request received, sending ACK\n");

            if(sendto(sockfd->socket_fd, &ACK_packet, sizeof(ACK_packet), 0,
                      (struct sockaddr*) &sockfd->dest_addr, sizeof(sockfd->dest_addr)) == -1){
                perror("Error sending ACK packet\n");
                return 0;
            }
            else{
                sockfd->isConnected = true;
                return 1;
            }
        }
        else{
            printf("Packet received is not a SYN packet, connection failed\n");
            return 0;
        }
    }

}

/**
 * Receives data on a connected RUDP socket.
 *
 * @param sockfd Pointer to the RUDP socket.
 * @param buffer Buffer to store received data.
 * @param buffer_size Size of the buffer.
 * @return Number of bytes received if received DATA packet, -2 if got SYN packet
 * -3 if got EOF DATA packet, 0 if got FIN packet, -1 if an error occurs.
 */
int rudp_recv(RUDP_Socket *sockfd, void *buffer, unsigned int buffer_size){
    if(sockfd == NULL ||!sockfd->isConnected || buffer == NULL || buffer_size <= 0){
        return -1;
    }

    RUDPPacket RECV_packet;
    struct sockaddr_in recv_addr;
    socklen_t recv_addrlen = sizeof(recv_addr);
    int num_bytes = recvfrom(sockfd->socket_fd,&RECV_packet, sizeof(RECV_packet), 0,
                             (struct sockaddr*) &recv_addr, &recv_addrlen);

    if(num_bytes == -1){
        perror("Error on recvfrom failed\n");
        return -1;
    }
    else{
        if(memcmp(&recv_addr.sin_addr, &sockfd->dest_addr.sin_addr, sizeof(struct in_addr)) == 0 &&
           recv_addr.sin_port == sockfd->dest_addr.sin_port){

            RUDPHeader ACK_packet;
            ACK_packet.length = 0;
            ACK_packet.checksum = 0;
            ACK_packet.flags = RUDP_ACK;

            switch(RECV_packet.header.flags){

                case RUDP_SYN:

                    if(sendto(sockfd->socket_fd, &ACK_packet, sizeof(ACK_packet), 0,
                              (struct sockaddr*) &sockfd->dest_addr, sizeof(sockfd->dest_addr)) == -1){
                        perror("Error sending ACK packet\n");
                        return -1;
                    }
                    else{
                        return -2;
                    }

                case RUDP_DATA:

                    if(RECV_packet.header.checksum == calculate_checksum(RECV_packet.data, RECV_packet.header.length)){

                        if(sendto(sockfd->socket_fd, &ACK_packet, sizeof(ACK_packet), 0,
                                  (struct sockaddr*) &sockfd->dest_addr, sizeof(sockfd->dest_addr)) == -1){
                            perror("Error sending ACK packet");
                            return -1;
                        }
                        else{
                            memcpy(buffer, RECV_packet.data, buffer_size);
                            if(RECV_packet.data[0] == EOF){
                                return -3;
                            }
                            return RECV_packet.header.length;
                        }
                    }
                    else{
                        perror("Checksum failed\n");
                        return -1;
                    }

                case RUDP_FIN:

                    if(sendto(sockfd->socket_fd, &ACK_packet, sizeof(ACK_packet), 0,
                              (struct sockaddr*) &sockfd->dest_addr, sizeof(sockfd->dest_addr)) == -1){
                        perror("Error sending ACK packet\n");
                        return -1;
                    }
                    else{
                        sockfd->isConnected = false;
                        memset(&sockfd->dest_addr, 0, sizeof(sockfd->dest_addr));
                        return 0;
                    }
            }
        }
        else{
            perror("Received a packet from an unexpected source\n");
            return -1;
        }
    }
    return -1;

}


/**
 * Sends data on a connected RUDP socket.
 *
 * @param sockfd Pointer to the RUDP socket.
 * @param buffer Data to send.
 * @param buffer_size Size of the data to send.
 * @return Number of bytes sent if sent DATA packet, 0 if sent FIN packet, -1 if an error occurs.
 */
int rudp_send(RUDP_Socket *sockfd, void *buffer, unsigned int buffer_size){
    if(sockfd == NULL || !sockfd->isConnected  || buffer == NULL || buffer_size <= 0){
        return -1;
    }

    RUDPPacket* send_packet = (RUDPPacket*)buffer;
    while(1){
        int num_bytes = sendto(sockfd->socket_fd, buffer, buffer_size, 0,
                           (struct sockaddr*)&sockfd->dest_addr, sizeof(sockfd->dest_addr));
        if(num_bytes == -1){
            perror("sendto failed\n");
            return -1;
        }

        struct sockaddr_in recv_addr;
        socklen_t recv_addrlen;

        RUDPHeader answer;
        recv_addrlen = sizeof(recv_addr);

        int num_recv_bytes = recvfrom(sockfd->socket_fd, &answer, sizeof(answer), 0,
                                 (struct sockaddr*) &(recv_addr), &recv_addrlen);
        if(num_recv_bytes == -1){

            if(errno == EWOULDBLOCK || errno == EAGAIN){
                printf("Timeout occurred, sending data again\n");
                continue;
            }
            else{
                perror("Receive failed\n");
                return -1;
            }
        }
        else{
            if(memcmp(&recv_addr.sin_addr, &sockfd->dest_addr.sin_addr, sizeof(struct in_addr)) == 0 &&
               recv_addr.sin_port == sockfd->dest_addr.sin_port){

                if(answer.flags == RUDP_ACK){

                    if(send_packet->header.flags == RUDP_DATA){
                        return num_bytes;
                    }
                    if(send_packet->header.flags == RUDP_FIN){
                        return 0;
                    }
                }
                else{
                    perror("Wrong packet received\n");
                    return -1;
                }
            }
            else{
                perror("Received a packet from an unexpected source\n");
                return -1;
            }
        }



    }


}


/**
 * Disconnects from a connected RUDP socket.
 *
 * @param sockfd Pointer to the RUDP socket.
 * @return 1 if disconnection is successful, 0 if an error occurs.
 */
int rudp_disconnect(RUDP_Socket *sockfd){

    if(sockfd == NULL || !sockfd->isConnected){
        return 0;
    }
    RUDPPacket FIN_packet;
    FIN_packet.header.flags = RUDP_FIN;
    memset(FIN_packet.data, 0, BUFFER_SIZE);
    FIN_packet.header.length = 0;
    FIN_packet.header.checksum = 0;

    if(rudp_send(sockfd, &FIN_packet, sizeof(FIN_packet)) == 0){
        sockfd->isConnected = false;
        memset(&sockfd->dest_addr, 0, sizeof(sockfd->dest_addr));
        return 1;
    }
    return 0;
}

/**
 * Closes an RUDP socket.
 *
 * @param sockfd Pointer to the RUDP socket to close.
 * @return 0 on success, -1 if an error occurs.
 */
int rudp_close(RUDP_Socket *sockfd){
    if(sockfd == NULL){
        return -1;
    }
    close(sockfd->socket_fd);
    free(sockfd);
    return 0;
}


/*
*   A checksum function that returns 16 bit checksum for data.
*   This function is taken from RFC1071, can be found here:
*   https://tools.ietf.org/html/rfc1071
*
*/
unsigned short int calculate_checksum(void *data, unsigned int bytes) {
    unsigned short int *data_pointer = (unsigned short int *)data;
    unsigned int total_sum = 0;
// Main summing loop
    while (bytes > 1) {
        total_sum += *data_pointer++;
        bytes -= 2;
    }
// Add left-over byte, if any
    if (bytes > 0)
        total_sum += *((unsigned char *)data_pointer);
// Fold 32-bit sum to 16 bits
    while (total_sum >> 16)
        total_sum = (total_sum & 0xFFFF) + (total_sum >> 16);
    return (~((unsigned short int)total_sum));
}