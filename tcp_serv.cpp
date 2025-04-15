#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <iostream>
#include <unistd.h>
#include <cassert>
#include <poll.h>
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

// append client data to incoming buffer
static void append_buffer(std::vector<uint8_t> &buf, const uint8_t *data, size_t len) {
    buf.insert(buf.end(), data, data + len);
}
// delete client data from the front of a buffer
static void consume_buffer(std::vector<uint8_t> &buf, size_t n) {
    buf.erase(buf.begin(), buf.begin() + n);
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

struct Connected {
    // client socket handle
    int fd = -1;
    // application intention in the event loop
    bool want_read = false;
    bool want_write = false;
    bool want_close = false;
    // buffering i/o
    std::vector<uint8_t> incoming;  // data to be parsed by the application
    std::vector<uint8_t> outgoing;  // response data from the app, to be sent
    };

static Connected *connection_accept(int fd) {
    // create stuct to store address info
    struct sockaddr_in client_addr = {};
    socklen_t addrlen = sizeof(client_addr);
    // accept
    int connfd = accept(fd, (sockaddr *)&client_addr, &addrlen);
    if (connfd < 0) {
        return NULL;
    }
    set_nonblocking(connfd); // set nb for connfd so accept is nonblocking

    // use connnect to populate a new Conn object
    Connected *connected = new Connected();
    connected->fd = connfd;
    connected->want_read = true; // safe bet, check if data is available and move on if it isn’t
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

    // set listening socket to non-blocking
    set_nonblocking(fd);

    // mapping client connections to fds, keyed by fd
    std::vector<Connected *>fd_conn_map;

    // event loop
    std::vector<pollfd> socketNotifiers; // list of socket notifiers
    while (true) {
        socketNotifiers.clear();
        // listening socket first into notifiers
        struct pollfd askListen = {fd, POLLIN, 0};
        socketNotifiers.push_back(askListen);

        // all the connection sockets into notifiers
        for (Connected *connected : fd_conn_map) {
            struct pollfd client {connected->fd, POLLERR, 0}; // default check for error

            //check for io intent
            if (connected->want_read) {client.events |= POLLIN;}
            if (connected->want_write) {client.events |= POLLOUT;}
            socketNotifiers.push_back(client);
        }

        // ready or not? waiting for readiness
        int rv = poll(socketNotifiers.data(), (nfds_t)socketNotifiers.size(), -1);
            if (rv < 0 && errno == EINTR) {
                continue; // signal interrupt, try again. don't treat as error
            }
            if (rv < 0) {
                std::cerr << "poll() failure" << std::endl;
            }

        /* if program is here, then at least one of the sockets is ready, its notifier active */

        // handle the ready listener
        // if there is a connection request, accept it
        if (socketNotifiers[0].revents) {
            if (Connected *connected = connection_accept(fd)) {
                // then add in the fd mapping
                // but first, check if there's a spot for it
                // if not, resize the vector before populating
                if (fd_conn_map.size() <= (size_t)connected->fd) {
                    fd_conn_map.resize(connected->fd + 1);
                }
                fd_conn_map[connected->fd] = connected;
            }
        }
    }

    // handle the connection sockets in the loop!!!
    // you need your read and write handlers to do this
    // script those first

    return 0;
}
