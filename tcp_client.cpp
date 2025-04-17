#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <iostream>
#include <unistd.h>
#include <cassert>

const size_t k_max_msg = 32 << 20;

// add data to the back of a buffer
static void append_buffer(std::vector<uint8_t> &buf, const uint8_t *data, size_t len) {
    buf.insert(buf.end(), data, data + len);
}
// delete data from the front of a buffer
static void consume_buffer(std::vector<uint8_t> &buf, size_t n) {
    buf.erase(buf.begin(), buf.begin() + n);
}

static int32_t read_full(int fd, uint8_t *buf, size_t n){
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

static int32_t write_all(int fd, const uint8_t *buf, size_t n) {
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

static int32_t request_send(int fd, const uint8_t *text, size_t len) {
    // check the length of the message against max limit
    if (len > k_max_msg) {
        perror("request exceeds permitted length.");
        return -1;
    }
    // creat write buffer, fill it with request body and write to socket
    std::vector<uint8_t>wbuf;
    append_buffer(wbuf, (const uint8_t *)&len, 4);
    append_buffer(wbuf, text, len);
    return write_all(fd, wbuf.data(), wbuf.size());
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
    /*
    int32_t err = query(fd, "testing 1");
    if (err) { goto L_DONE;}

    err = query(fd, "testing 2");
    if (err) { goto L_DONE;} */

    L_DONE:
        close(fd);
        return 0;

    return 0;
}