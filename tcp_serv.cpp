#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <iostream>
#include <unistd.h>
#include <cassert>
#include <sys/fcntl.h>

const size_t k_max_message = 4096;

static void set_nonblocking(int fd) {
    // get current file status flags
    errno = 0;
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl(F_GETFL) failed.");
        exit(EXIT_FAILURE);
    }

    // combining acquired flag with NONBLOCK
    flags |= O_NONBLOCK;

    // set flag
    errno = 0;
    if (fcntl(fd, F_SETFL, flags) == -1) {
        perror("fcntl(F_SETFL failed.");
        exit(EXIT_FAILURE);
    }
}

// functions to read & write in loops
// given by `n`, the number of bytes to be read or written
static int32_t read_full(int fd, char *buf, size_t n){
    while (n > 0) {
        ssize_t rv = read(fd, buf, n);
        if (rv < 0) {
            if (errno == EINTR) {
                continue;   // retry after signal interrupt
            }
            else { return -1;} // read error has occurred
        }
        else if (rv == 0) {
            return -1; // unexpected EOF
        }

        assert((size_t)rv <= n);
        buf += n;
        n -= (size_t)rv;
        }
    return 0;
}

static int32_t write_all(int fd, const char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = write(fd, buf, n);
        if (rv < 0) {
            if (errno == EINTR) {
                continue;
            }
        }
        if (rv == 0) {
            return -1; // no bytes written, an error has occurred
        }
        assert((size_t)rv <= n);
        buf += rv;
        n -= (size_t)rv;
    }
    return 0;
}

int main(){

    // listening socket fd
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    // allow the socket to reuse its address
    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    
    // populate socket address and bind
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

    // listen for incoming connections
    rv = listen(fd, SOMAXCONN);
    if (rv) {
        std::cerr << "listen() failed: " << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }

    return 0;
}
