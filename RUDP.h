#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <sys/time.h>
#include <stdbool.h>

#define BUFFER_SIZE 65500

#define RUDP_SYN 0x01
#define RUDP_ACK 0x02
#define RUDP_FIN 0x04
#define RUDP_DATA 0x08


typedef struct RUDPHeader{
    unsigned short length; // length of data
    unsigned short checksum; // checksum of data
    u_int8_t flags;
}RUDPHeader;

typedef struct RUDPPacket{
    RUDPHeader header;
    char data[BUFFER_SIZE];
}RUDPPacket;


typedef struct rudp_socket
{
    int socket_fd; // UDP socket file descriptor
    bool isServer; // True if the RUDP socket acts like a server, false for client.
    bool isConnected; // True if there is an active connection, false otherwise.
    struct sockaddr_in dest_addr; // Destination address. Client fills it when it connects via rudp_connect(), server fills it when it accepts a connection via rudp_accept().
} RUDP_Socket;



/**
 * Allocates and Creates a new RUDP socket.
 * If isServer true it also binds the socket to the listen_port.
 * If isServer false it sets a receiving timeout for the client
 * @param isServer True if creating a server socket, false for a client socket.
 * @param listen_port The port to listen on for incoming connections (only for servers).
 * @return A pointer to the created RUDP socket, or NULL if an error occurs.
 */
RUDP_Socket* rudp_socket(bool isServer, unsigned short int listen_port);

/**
 * Initiates a connection to a remote RUDP socket.
 *
 * @param sockfd Pointer to the RUDP socket.
 * @param dest_ip IP address of the remote socket.
 * @param dest_port Port of the remote socket.
 * @return 1 if the connection is successful, 0 if an error occurs.
 */
int rudp_connect(RUDP_Socket *sockfd, const char *dest_ip, unsigned short int dest_port);

/**
 * Accepts an incoming connection request on a server RUDP socket.
 *
 * @param sockfd Pointer to the RUDP socket.
 * @return 1 if the connection is accepted, 0 if an error occurs.
 */
int rudp_accept(RUDP_Socket *sockfd);

/**
 * Receives data on a connected RUDP socket.
 *
 * @param sockfd Pointer to the RUDP socket.
 * @param buffer Buffer to store received data.
 * @param buffer_size Size of the buffer.
 * @return Number of bytes received if received DATA packet, -2 if got SYN packet
 * -3 if got EOF DATA packet, 0 if got FIN packet, -1 if an error occurs.
 */
int rudp_recv(RUDP_Socket *sockfd, void *buffer, unsigned int buffer_size);

/**
* Sends data on a connected RUDP socket.
*
* @param sockfd Pointer to the RUDP socket.
* @param buffer Data to send.
* @param buffer_size Size of the data to send.
* @return Number of bytes sent if sent DATA packet, 0 if sent FIN packet, -1 if an error occurs.
*/
int rudp_send(RUDP_Socket *sockfd, void *buffer, unsigned int buffer_size);

/**
* Disconnects from a connected RUDP socket.
*
* @param sockfd Pointer to the RUDP socket.
* @return 1 if disconnection is successful, 0 if an error occurs.
*/
int rudp_disconnect(RUDP_Socket *sockfd);

/**
 * Closes an RUDP socket.
 *
 * @param sockfd Pointer to the RUDP socket to close.
 * @return 0 on success, -1 if an error occurs.
 */
int rudp_close(RUDP_Socket *sockfd);

// Other Functions

unsigned short int calculate_checksum(void *data, unsigned int bytes);