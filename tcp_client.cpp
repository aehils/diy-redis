#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <iostream>
#include <unistd.h>

int main() {
    
    int fd {socket(AF_INET, SOCK_STREAM, 0)};
    int reuseBool {1};
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuseBool, sizeof(reuseBool));

    //setting up server address information to make the connection
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // the loopback address, 127.0.0.1
    int rv = connect(fd, (const sockaddr *)&addr, sizeof(addr));
    if (rv) {
        std::cerr << "connect() failed: ";
        perror(nullptr);
        exit(EXIT_FAILURE);
    }

    char msg[] = "hello";
    write(fd, msg, strlen(msg));

    char rbuf[64] = {};
    ssize_t n = read(fd, rbuf, sizeof(rbuf) - 1);
    if (n < 0) {
        std::cerr << "failed to read() from server: ";
        perror(nullptr);
        exit(EXIT_FAILURE);
    }

    printf("server response: %s", rbuf);
    close(fd);

    return 0;
}