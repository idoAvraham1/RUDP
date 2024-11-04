CFLAGS = -Wall -g
CC = gcc

all: RUDP_receiver RUDP_sender

RUDP_receiver: RUDP_Receiver.o RUDP.o
	$(CC) $(CFLAGS) RUDP_Receiver.o RUDP.o -o RUDP_receiver

RUDP_sender: RUDP_Sender.o RUDP.o
	$(CC) $(CFLAGS) RUDP_Sender.o RUDP.o -o RUDP_sender

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o RUDP_receiver RUDP_sender

