#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <iostream>
#include <unistd.h>
#include <cassert>
#include <poll.h>
#include <sys/fcntl.h>


const size_t k_max_msg = 4096;  

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
        perror("fcntl(F_SETFL) failed.");
        exit(EXIT_FAILURE);
    }
}

// add data to the back of a buffer
static void append_buffer(std::vector<uint8_t> &buf, const uint8_t *data, size_t len) {
    buf.insert(buf.end(), data, data + len);
}
// delete data from the front of a buffer
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

    return connected;
}

static void nb_read(Connected *connected) {
    // do a single non-blocking read
    uint8_t rbuf[64 *1024];
    ssize_t rv = read(connected->fd, rbuf, sizeof(rbuf));
    if (rv < 0) {
        if (errno == EINTR){
            perror("RETRY: read signal was interrupted");
            return; // retry later, don't treat as an error
        } 
        else if (errno == EAGAIN || errno == EWOULDBLOCK){
            perror("RETRY: socket not ready");
            return;
        } 
        else {
            perror("CLOSE: read error");
            connected->want_close = true;
            return;
        }
    }
    if (rv == 0){
        if (connected->incoming.empty()){
            perror("CLOSE: client not connected");
            connected->want_close = true;
            return;
        } else {
            perror("CLOSE: unexpected EOF");
            connected->want_close = true; // EOF - ask to close connection
            return;
        }
    }
    // put everything read from rbuf into ::incoming for this connection
    append_buffer(connected->incoming, rbuf, (size_t)rv);
    // check if the data in the buffer makes a complete request
    while (try_single_request(connected)) {}

    // if the program has response for this connection, change flag to write
    if (connected->outgoing.size() > 0) {   
        connected->want_read = false;
        connected->want_write = true;
        // if no response, leave on read incase new msg
    }
}

static void nb_write(Connected *connected) {
    // do a single non-bloacking write 
    // first make sure there's actually something in ::outgoing to write to this client
    assert(connected->outgoing.size() > 0);
    ssize_t rv = write(connected->fd, connected->outgoing.data(), connected->outgoing.size());
    if (rv < 0) {
        if (errno == EINTR) {
            perror("RETRY: signal interrupted");
            return;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            perror("RETRY: socket not ready");
            return;
        }
        else {
            perror("CLOSE: write errror");
            connected->want_close = true;
            return;
        }
    }
    else if (rv == 0) {
        // 0 rv means the connection was probably closed by the remote peer
        perror("CLOSE: no bytes written to client. connection likely closed");
        connected->want_close = true;
        return;
    }

    // consume from ::outgoing
    consume_buffer(connected->outgoing, (size_t)rv);
    // if ::outgoing is now empty, allow the program to read new requests in this connection
    if (connected->outgoing.size() == 0) {
        connected->want_write = false;
        connected->want_read = true;
        // if ::outgoing is not empty, the flag doesnt change - allow to keep writing
    }
}

static bool try_single_request(Connected *connected) {
    /* try to parse the accumulated buffer
        process the parsed message
        remove the message from incoming buffer  */

    // check that the data suffices a header at least
    if (connected->incoming.size() < 4) {
        return false;   // wants to read again in next iteration
    }

    uint32_t len = 0;
    memcpy(&len, connected->incoming.data(), 4);
    if (len > k_max_msg) {
        perror("Incoming data exceeds request limit.");
        connected->want_close = true;   // want to close 
        return false; 
    }

    if ((4 + len) > connected->incoming.size()) {
        return false;   // the message is clearly not complete, read again next iteration
    }

    const uint8_t *request = &connected->incoming[4];

    // request parsed, print it for your own sake
    printf("client msg-> len: %u, data: %.*s\n", len, (len < 100 ? len : 100), request);

    // echo client message (for now) from the server
    // so just take the data from ::incoming and put in ::outgoing
    append_buffer(connected->outgoing, (const uint8_t *)&len, 4);
    append_buffer(connected->outgoing, request, len);
    // consume message from connected::incoming to clear the buffer
    consume_buffer(connected->incoming, 4 + len);
    return true; // successfully parsed a complete message - (bool) let the caller know
}

int main() {

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
                // check that there isnt a connection already stored at [conn::fd]
                assert(!fd_conn_map[connected->fd]);
                fd_conn_map[connected->fd] = connected;
            }
        }

        // handle the client socket connections
        for (size_t i = 1; i < socketNotifiers.size(); i++) {
            uint32_t ready = socketNotifiers[i].revents;

            Connected *connected = fd_conn_map[socketNotifiers[i].fd];
            if (ready && POLLIN) {
                assert(connected->want_read == true);
                nb_read(connected);
            }
            if (ready && POLLOUT) {
                assert(connected->want_write == true);
                nb_write(connected);
            }
            if ((ready && POLLERR) || connected->want_close) {
                // error, close the connection
                perror("CLOSED CLIENT CONNECTION");
                (void)close(fd);
                fd_conn_map[connected->fd] = NULL;
                delete connected;
            }
        } // for all client connections
    } // end of event loop
    return 0;
}
