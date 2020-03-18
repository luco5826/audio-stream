#include <ao/ao.h>
#ifdef _WIN32
    #include <winsock2.h>
    #define SOCK SOCKET
#else
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <sys/socket.h> 
    #define SOCK int
#endif
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#define BITS 8

int main(int argc, char *argv[]) {
    unsigned char *buffer;
    size_t done;
    int err;
    int ok = 1;

    int driver;
    ao_device *dev;

    ao_sample_format format;
    int channels, encoding;
    long rate;

    // Arguments check (server ip is mandatory)
    // Default values
    // PORT = 30000
    // BUFFER = 64k
    if (argc == 1 || argc > 4) {
        printf("Usage: audio_client <server ip> <server port = 30000> <buffer size = 64k>\n");
        exit(EXIT_FAILURE);
    }
    int port = argc == 3 ? atoi(argv[2]) : 0;
    int buffer_size = argc == 4 ? atoi(argv[3]) : 0;
    port = port < 65000 && port > 1024 ? port : 30000;
    buffer_size = buffer_size > 10000 ? buffer_size : 64000;

    // On Windows, we need to setup socket settings
    #ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }
    #endif

    struct sockaddr_in clientaddr, servaddr;
    memset((char *)&servaddr, 0, sizeof(struct sockaddr_in));
    memset((char *)&clientaddr, 0, sizeof(struct sockaddr_in));

    // Internet socket
    servaddr.sin_family = AF_INET;
    clientaddr.sin_family = AF_INET;

    printf("Searching for host %s\n", argv[1]);
    struct hostent *host = gethostbyname(argv[1]);
    if (host == NULL) {
        perror("Unable to find host:\n");
        exit(EXIT_FAILURE);
    }
    servaddr.sin_addr.s_addr = ((struct in_addr *)(host->h_addr_list[0]))->s_addr;
    clientaddr.sin_addr.s_addr = INADDR_ANY;

    servaddr.sin_port = htons(port);
    clientaddr.sin_port = 0;

    int len = sizeof(servaddr);

    // TCP Socket creation
    SOCK tcpSocket;
    if ((tcpSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error while creating socket:");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(tcpSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&ok, sizeof(int)) < 0) {
        perror("Error while setting ReuseAddress:");
        exit(EXIT_FAILURE);
    }
    if (bind(tcpSocket, (struct sockaddr *)&clientaddr, sizeof(clientaddr)) < 0) {
        perror("Error while binding socket:");
        exit(EXIT_FAILURE);
    }
    if (connect(tcpSocket, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("Connection failed\n");
        exit(EXIT_FAILURE);
    }
    printf("Connection estabilished\n");

    // Audio player initialization, main buffer creation
    ao_initialize();
    driver = ao_default_driver_id();
    buffer = (unsigned char *)malloc(sizeof(unsigned char) * buffer_size);

    // Read audio file format, we use recv/send instead of read/write because of
    // different socket implementation in Unix/Windows (recv/send calls library implementation,
    // read/write are OS's API)
    if (recv(tcpSocket, (char*)&format, sizeof(ao_sample_format), 0) < 0) {
        perror("Unable to read format:");
        exit(EXIT_FAILURE);
    }
    printf("%-12s: %8d Hz\n", "Rate", format.rate);
    printf("%-12s: %8d bits\n", "Quantiz.", format.bits);
    printf("%-12s: %8d\n", "Channels", format.channels);
    printf("%-12s: %8d kbps\n", "Bitrate", format.rate * format.bits / 1000);
    dev = ao_open_live(driver, &format, NULL);

    // Send ok to start reading data
    if (send(tcpSocket, (char*)&ok, sizeof(int), 0) < 0) {
        perror("Error while sending OK:");
        exit(EXIT_FAILURE);
    }
    printf("Reading data from %s\n", inet_ntoa(servaddr.sin_addr));

    int nread = 0;
    while ((nread = recv(tcpSocket, (char*)buffer, buffer_size, 0)) > 0) {
        printf("PID:%d, Read %d bytes\n", getpid(), nread);
        if (ao_play(dev, buffer, nread) < 0) {
            perror("Error while playing:");
            exit(EXIT_FAILURE);
        }
    }
    printf("Stream ended\n");
    
    // Cleanup
    free(buffer);
    ao_close(dev);
    ao_shutdown();
    return 0;
}
