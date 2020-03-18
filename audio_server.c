#include <ao/ao.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <mpg123.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define BITS 8

mpg123_handle *mh;
int tcpSocket;
unsigned char *buffer;
size_t buffer_size;

int createAndBindSocket(struct sockaddr_in *serverAddress, int listenPort) {
    memset((void *)serverAddress, 0, sizeof(struct sockaddr_in));
    serverAddress->sin_family = AF_INET;
    serverAddress->sin_addr.s_addr = INADDR_ANY;
    serverAddress->sin_port = htons(listenPort);

    // TCP socket creation
    if ((tcpSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error while creating tcp socket:");
        exit(EXIT_FAILURE);
    }
    // Max size for send buffer = 3 times buffer size
    int sndbuf = buffer_size * 3;
    if (setsockopt(tcpSocket, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(int)) < 0) {
        perror("Error setting snd buffer:");
        exit(EXIT_FAILURE);
    }
    if (bind(tcpSocket, (struct sockaddr *)serverAddress, sizeof(*serverAddress)) != 0) {
        perror("Error while binding to local socket:");
        exit(EXIT_FAILURE);
    }
    if (listen(tcpSocket, 5) < 0) {
        perror("Error listen:");
        exit(EXIT_FAILURE);
    }
    return tcpSocket;
}

void cleanup() {
    close(tcpSocket);
    free(buffer);
    mpg123_close(mh);
    mpg123_delete(mh);
    mpg123_exit();
}

int main(int argc, char *argv[]) {
    size_t done;
    int driver;
    ao_sample_format format;
    int channels, encoding;
    long rate;

    // Arguments check
    // Default values
    // PORT = 30000
    // BUFFER = 64k
    if (argc == 1 || argc > 4) {
        printf("Usage: audio_server <mp3 file> <listen port = 30000> <buffer size = 64k>\n");
        exit(EXIT_FAILURE);
    }
    int port = argc == 3 ? atoi(argv[2]) : 0;
    buffer_size = argc == 4 ? atoi(argv[3]) : 0;  //mpg123_outblock(mh);
    port = port < 65000 && port > 1024 ? port : 30000;
    buffer_size = buffer_size > 10000 ? buffer_size : 64000;

    // Socket setup
    struct sockaddr_in serverAddress, clientAddress;
    memset(&clientAddress, 0, sizeof(clientAddress));
    buffer = (unsigned char *)malloc(buffer_size * sizeof(unsigned char));
    tcpSocket = createAndBindSocket(&serverAddress, port);

    // Mp3 decoder initialization
    mpg123_init();
    int len = sizeof(clientAddress);
    printf("Waiting for clients\n");
    int clientfd;
    while ((clientfd = accept(tcpSocket, (struct sockaddr *)&clientAddress, &len)) > 0) {
        // Parallel server, one child for every client
        if (fork() == 0) {
            int err;
            mh = mpg123_new(NULL, &err);

            // Open mp3 file
            int fdFile = open(argv[1], O_RDONLY);
            if (fdFile <= 0) {
                perror("Error while opening audio file:");
                cleanup();
                exit(EXIT_FAILURE);
            }
            mpg123_open_fd(mh, fdFile);
            
            // Read and send file format
            mpg123_getformat(mh, &rate, &channels, &encoding);
            format.bits = mpg123_encsize(encoding) * BITS;
            format.rate = rate;
            format.channels = channels;
            format.byte_format = AO_FMT_NATIVE;
            format.matrix = 0;
            if (write(clientfd, &format, sizeof(format)) < 0) {
                perror("Error while sending format:");
                cleanup();
                exit(EXIT_FAILURE);
            }
            printf("\t%-5s: %-20d\n", "PID", getpid());
            printf("\t%-5s: %-20s\n", "IP", inet_ntoa(clientAddress.sin_addr));
            printf("\n\tMP3 FORMAT\n");
            printf("\t%-10s: %8d bits\n", "Quantiz.", format.bits);
            printf("\t%-10s: %8d Hz\n", "Rate", format.rate);
            printf("\t%-10s: %8d\n", "Channels", format.channels);

            int ok;
            if (read(clientfd, &ok, sizeof(int)) < 0) {
                perror("Error while receiving OK:");
                cleanup();
                exit(EXIT_FAILURE);
            }

            printf("\tSending data in %d bytes packets\n", buffer_size);
            
            // struct timespec delay = {0, 300 * 1000000};
            while (mpg123_read(mh, buffer, buffer_size, &done) == MPG123_OK) {
                int nwrote = write(clientfd, buffer, done);
                if (nwrote < 0) {
                    perror("Error while writing data:");
                    cleanup();
                    exit(EXIT_FAILURE);
                }
                // nanosleep(&delay, NULL);
                // printf("PID:%d, Wrote %d bytes\n", getpid(), nwrote);
            }
            printf("\tPID %d, Stream ended\n", getpid());
            close(clientfd);
            close(fdFile);
            mpg123_close(mh);
        }
        sleep(1);
        printf("Waiting for clients\n");
    }

    exit(EXIT_FAILURE);
    return 0;
}