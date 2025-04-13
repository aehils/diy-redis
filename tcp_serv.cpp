#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <iostream>
#include <unistd.h>
#include <cassert>

const size_t k_max_message = 4096;

static void interact(int connfd) {
    char rbuf[k_max_message] = {};
    ssize_t n = read(connfd, rbuf, sizeof(rbuf) - 1);
    if (n < 0) {
        std::cerr << "read() failed: " << strerror(errno) << std::endl;
        return;
    }
    if (n == 0) {
        std::cerr << "client disconnected" << std::endl;
        return;
    }
    std::cout << "client msg received: " << rbuf << std::endl;

    char wbuf[64] = "acknowledged\n";
    write(connfd, wbuf, strlen(wbuf));
}

// functions to read & write in loops
// given by `n`, the number of bytes to be read or written
static int32_t read_full(int fd, char *buf, size_t n){
    while (n > 0) {
        ssize_t rv = read(fd, buf, n);
        if (rv <= 0) {
            return -1; // unexpected EOF or error reading
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
        if (rv <= 0){
            return -1;
        }
        assert((size_t)rv <= n);
        buf += rv;
        n -= (size_t)rv;
    }
    return 0;
}

// this is to ensure messages are read/written completely and no bytes are dropped
// will utilise the read_full and write_all functions from before
static int32_t request_from(int connfd) {
   
    // buffer initialised with 4-byte header and maximum msg size we set earlier as a const
    // header stores the length of the incoming message so we know how to handle it
    // we will attempt to read the header 
    char rbuf[4 + k_max_message];
    errno = 0; // reset errno because it doesnt reset automatically on successful runs
    size_t err = read_full(connfd, rbuf, 4);
    if (err) {
        std::cerr << (errno == 0 ? "EOF" : "read() error") << std::endl;
        return err;
    }

    // copying the first 4 bytes of the header to `len`
    uint32_t len = 0;
    memcpy(&len, rbuf, 4); //assume this is little edian
    // if the length of the message is longer than the size we've allocated for it, we wanna know
    if (len > k_max_message) {
        std::cerr << ("request message is too long.") << std::endl;
        return -1;
    }
    // now to read the message. read from rbuf[4] because we already read the first 4 bytes
    // everything in the buffer from [4] is the actual msg. before that is the header
    err = read_full(connfd, &rbuf[4], len);
    if (err) {
        std::cerr << "error reading request message" << std::endl;
    }
    // okay so we have our message, we do something with it
    // for now we just print it ig
    printf("client msg: %.*s\n" ,len, &rbuf[4]);

    // send a response
    const char response[] = "message received";
    char wbuf[4 + sizeof(response)];
    len = (uint32_t)strlen(response);
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4],response, len);
    return write_all(connfd, wbuf, 4 + len);
}

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
        
        // try to process all incoming requests on this connection
        while (true) {
            int rv = request_from(connfd);
            if (rv < 0) { // error reading request or client closed connection
                break;
            }
        }
        close(connfd);
    }
    return 0;
}
