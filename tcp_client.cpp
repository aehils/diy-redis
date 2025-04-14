#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <iostream>
#include <unistd.h>
#include <cassert>

const size_t k_max_msg = 4096;  

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

static int32_t query(int fd, const char *text) {
    // check the length of the message against max limit
    uint32_t len = (uint32_t)strlen(text);
    if (len > k_max_msg) {
        std::cerr << "message body exceeds permitted length.";
        return -1;
    }

    // sending the request with a matching protocol
    char wbuf[4 + k_max_msg];
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], text, len); // little eddy assumption once again
    u_int32_t err = write_all(fd, wbuf, 4 + len);
    if (err) {
        std::cerr << "request not sent." << std::endl;
        return err;
    }

    // reading server response
    char rbuf[4 + k_max_msg + 1];
    err = read_full(fd, rbuf, 4);
    if (err) {
        std::cerr << (errno == 0 ? "Unexpected EOF. Check connection." : "No response from server.");
        return err;
    }

    memcpy(&len, rbuf, 4);
    if (len > k_max_msg) {
        std::cerr << "Server response exceeds permitted length." << std::endl;
        return -1;
    }

    // read response body into rbuf
    err = read_full(fd, &rbuf[4], len);
    if (err) {
        std::cerr << "Error reading server response body." << std::endl;
        return err;
    }
    // query sent, response recieved. do something with server response.
    // just print it for now
    printf("server response: %.*s\n", len, &rbuf[4]);
    return 0;
}

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
    
    int32_t err = query(fd, "testing 1");
    if (err) { goto L_DONE;}

    err = query(fd, "testing 2");
    if (err) { goto L_DONE;}

    L_DONE:
        close(fd);
        return 0;

    return 0;
}