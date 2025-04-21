#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <iostream>
#include <unistd.h>
#include <cassert>

const size_t max_msg = 32 << 20;

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
        buf += rv;
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

static int32_t request_send(int fd, const std::vector<std::string> &cmd) {
    uint32_t len = 4;   // 4 bytes reserved for nstr
    for (const std::string &s : cmd) {
        len += 4 + s.size();
    }   // for each string in cmd, add 4 bytes to encode the str size
    if (len > max_msg) {
        perror("request exceeds permitted length");
        return -1;
    }   // len carries the entire payload size (excluding msg length prefix)

    uint8_t wbuf[4 + max_msg];
    memcpy(&wbuf[0], &len, 4);  // length prefix
    uint32_t nstr = cmd.size();
    memcpy(&wbuf[4], &nstr, 4); // number of strings

    size_t pos = 8;     // next available idx in wbuf
    for (const std::string &s : cmd) {
        uint32_t arglen = (uint32_t)s.size();
        memcpy(&wbuf[pos], &arglen, sizeof(arglen));
        memcpy(&wbuf[pos + sizeof(arglen)], s.data(), arglen);
        pos += sizeof(arglen) + arglen;
    }
    return write_all(fd, wbuf, sizeof(wbuf));
}

static int32_t response_read(int fd) {
    errno = 0;
    uint8_t rbuf[4 + max_msg + 1];
    int32_t err = read_full(fd, rbuf, 4);   // read length prefix
    if (err) {
        if (errno == 0) {
            printf("EOF");
        } else {
            perror("response not received from server"); }
        return err;
    }

    uint32_t len = 0;
    memcpy(&len, rbuf, 4);
    if (len > max_msg) {
        perror("server response exceeds permitted length");
        return -1;
    }   // evaluating length prefix

    // payload
    err = read_full(fd, &rbuf[4], len);
    if (err) {
        perror("cannot read server response payload");
        return err;
    }
    // print result
    uint32_t statuscode = 0;
    if (len < 4) {
        perror("malformed response read from server");
        return -1;
    }
    memcpy(&statuscode, &rbuf[4], 4); // acquire status code
    printf("server says: [%u] %.*s\n", statuscode, len - 4, &rbuf[8]);
    return 0;
}

int main(int argc, char **argv) {
    
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

    std::vector<std::string> cmd;
    for (int i = 1; i < argc; ++i) {
        cmd.push_back(argv[i]);
    }

    int32_t err = request_send(fd, cmd);
    if (err) {
        goto L_DONE;
    }
    err = response_read(fd);
    if (err) {
        goto L_DONE;
    }


    L_DONE:
        close(fd);
        return 0;
}