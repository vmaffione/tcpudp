#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <getopt.h>


#define BUFLEN 512

static void
quit(char *s)
{
    perror(s);
    exit(1);
}

static void
usage(void)
{
    printf("udpsend [-h] [-d <DEST_IP_ADDR>] [-D <DEST_UDP_PORT>]"
            "[-S <SOURCE_UDP_PORT>] [-l <UDP_PAYLOAD_LEN>]\n");
    exit(0);
}

int main(int argc, char **argv)
{
    const char *ipstr = "10.60.1.1";
    struct sockaddr_in srv_addr, myaddr;
    unsigned long long n, limit;
    struct timespec t_last;
    char buf[BUFLEN + 1];
    int port = 9300;
    int sport = 10300;
    size_t len = BUFLEN;
    int s;
    int flags;
    int opt;

    (void)flags;

    while ((opt = getopt(argc, argv, "hd:D:S:l:")) != -1) {
        switch (opt) {
        case 'h':
        default:
            usage();
            break;

        case 'd':
            ipstr = optarg;
            break;

        case 'D':
            port = atoi(optarg) + 9300;
            break;

        case 'S':
            sport = atoi(optarg) + 10300;
            break;

        case 'l':
            len = (size_t)atoi(optarg);
            break;
        }
    }

    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) {
        quit("socket");
    }

#if 0
    flags = fcntl(s, F_GETFL, 0);
    if (fcntl(s, F_SETFL, flags | O_NONBLOCK)) {
        quit("fcntl");
    }
#endif

    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port);

    if (inet_aton(ipstr, &srv_addr.sin_addr) == 0) {
        quit("inet_aton() failed");
    }

    memset(&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = INADDR_ANY;
    myaddr.sin_port = htons(sport);
    if (bind(s, (struct sockaddr *)&myaddr, sizeof(myaddr))) {
        quit("bind");
    }

    if (connect(s, (struct sockaddr *)&srv_addr, sizeof(srv_addr))) {
        quit("bind");
    }

    clock_gettime(CLOCK_MONOTONIC, &t_last);
    limit = 100ULL;

    memset(buf, 'x', BUFLEN);

    for (n=0;; n++) {
        if (send(s, buf, len, 0) == -1 && errno != EAGAIN) {
            quit("send()");
        }

        if (n >= limit) {
            unsigned long long diff_ns;
            struct timespec t_cur;
            double rate;

            clock_gettime(CLOCK_MONOTONIC, &t_cur);
            diff_ns = (t_cur.tv_sec - t_last.tv_sec) * 1e9 +
                      (t_cur.tv_nsec - t_last.tv_nsec);

            if (diff_ns < 1e9) {
                limit <<= 1;
            } else if (limit > 1 && diff_ns > 2e9) {
                limit >>= 1;
            }

            rate = (((double)n) * 1e3) / diff_ns;
            printf("Sending %5.6f Mpps\n", rate);

            n = 0;
            clock_gettime(CLOCK_MONOTONIC, &t_last);
        }
    }

    close(s);

    return 0;
}
