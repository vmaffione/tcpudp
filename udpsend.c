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
#include <pthread.h>


#define BUFLEN 512

static void
quit(char *s)
{
    perror(s);
    exit(1);
}

struct td {
    struct g *g;
    pthread_t th;
    int tid;
};

struct g {
    const char *ipstr;
    int port;
    int sport;
    size_t len;
    int n;
    int stop;
    struct td tds[0];
};

static void *
sender_routine(void *opaque)
{
    struct td *td = opaque;
    struct sockaddr_in srv_addr, myaddr;
    unsigned long long n, limit;
    struct timespec t_last;
    char buf[BUFLEN + 1];
    size_t len = td->g->len;
    int s;

    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) {
        quit("socket");
    }

#if 0
    int flags;

    flags = fcntl(s, F_GETFL, 0);
    if (fcntl(s, F_SETFL, flags | O_NONBLOCK)) {
        quit("fcntl");
    }
#endif

    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(td->g->port + td->tid);

    if (inet_aton(td->g->ipstr, &srv_addr.sin_addr) == 0) {
        quit("inet_aton() failed");
    }

    memset(&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = INADDR_ANY;
    myaddr.sin_port = htons(td->g->sport + td->tid);
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

            if (diff_ns < 2e9) {
                limit <<= 1;
            } else if (limit > 1 && diff_ns > 3e9) {
                limit >>= 1;
            }

            rate = (((double)n) * 1e3) / diff_ns;
            printf("Sending %5.6f Mpps\n", rate);

            n = 0;
            clock_gettime(CLOCK_MONOTONIC, &t_last);
        }
    }

    close(s);

    return NULL;
}

static void
usage(void)
{
    printf("udpsend [-h] [-d <DEST_IP_ADDR>] [-D <DEST_UDP_PORT>] "
            "[-S <SOURCE_UDP_PORT>] [-l <UDP_PAYLOAD_LEN>] "
            "[-n <NUM_THREADS>]\n");
    exit(0);
}

int main(int argc, char **argv)
{
    struct g *g;
    const char *ipstr = "10.0.0.5";
    int port = 9300;
    int sport = 9800;
    size_t len = BUFLEN;
    int n = 1;
    int opt;
    int i;

    while ((opt = getopt(argc, argv, "hd:D:S:l:n:")) != -1) {
        switch (opt) {
        case 'h':
        default:
            usage();
            break;

        case 'd':
            ipstr = optarg;
            break;

        case 'D':
            port = atoi(optarg);
            break;

        case 'S':
            sport = atoi(optarg);
            break;

        case 'l':
            len = (size_t)atoi(optarg);
            if (len > BUFLEN) {
                len = BUFLEN;
            }
            break;

        case 'n':
            n = atoi(optarg);
            break;
        }
    }

    g = malloc(sizeof(*g) + n * sizeof(g->tds[0]));
    if (!g) {
        exit(EXIT_FAILURE);
    }
    g->ipstr = ipstr;
    g->port = port;
    g->sport = sport;
    g->n = n;
    g->stop = 0;

    for (i = 0; i < g->n; i ++) {
        g->tds[i].g = g;
        g->tds[i].tid = i + 1;
        pthread_create(&g->tds[i].th, NULL, sender_routine, g->tds + i);
    }

    for (i = 0; i < g->n; i ++) {
        pthread_join(g->tds[i].th, NULL);
    }

    return 0;
}
