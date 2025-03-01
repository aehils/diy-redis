#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <iostream>
#include <unistd.h>

int main(){

    //creating the socket in IPv4 type TCP
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    //configuring a setting for the socket.
    //SO_REUSEADDR set to 1. the socket is now allowed to reuse its address
    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    // an instance of the sockaddr struct that stores IPv4 addr info
    //bound to our fd; rv - return value - handles error
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);  //port given - htons() converts host port no. to network short
    addr.sin_addr.s_addr = htonl(0); //wildcard 0.0.0.0 - htonl() converts addr to net long

    int rv  = bind(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (rv) {
        std::cerr << "bind() failed: ";
        perror(nullptr); // This will print the error message corresponding to errno
        exit(EXIT_FAILURE);
    }
    
    //the socket is actually created after listen()
    //only parameters have been passed up to this point
    rv = listen(fd, SOMAXCONN);
    if (rv) {
        std::cerr << "listen() failed: " << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }

    //configure the server to accept connections
    while (true) {
        struct sockaddr_in client_addr = {};
        socklen_t addrlen = sizeof(client_addr);
        int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
        if (connfd < 0) {
            continue;
        }
    do_something(connfd);
    close(connfd);
    }
}

static void do_something(int connfd) {
    char rbuf[64] = {};
    ssize_t n = read(connfd, rbuf, sizeof(rbuf -1));
    if (n < 0) {
        std::cerr << "read() failed: " << strerror(errno) << std::endl;
        return;
    }
    if (n == 0) {
        std::cerr << "client disconnected" << std::endl;
        return;
    }
    std::cout << "client msg received: " << rbuf << std::endl;

    char wbuf[64] = "message acknowledged";
    write(connfd, wbuf, sizeof(wbuf));

}
