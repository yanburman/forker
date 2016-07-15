#include <sys/signalfd.h>
#include <sys/epoll.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#define handle_error(msg)                                                                                              \
    do {                                                                                                               \
        perror(msg);                                                                                                   \
        exit(EXIT_FAILURE);                                                                                            \
    } while (0)

#define MAX_EVENTS 10

static void run_epoll(int epollfd)
{
    struct epoll_event events[MAX_EVENTS];
    int nfds, n;
    struct signalfd_siginfo fdsi;
    ssize_t sz;

    for (;;) {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            handle_error("epoll_wait");
        }

        for (n = 0; n < nfds; ++n) {
            sz = read(events[n].data.fd, &fdsi, sizeof(struct signalfd_siginfo));
            if (sz != sizeof(struct signalfd_siginfo))
                handle_error("read");

            if (fdsi.ssi_signo == SIGINT) {
                printf("Got SIGINT\n");
            } else if (fdsi.ssi_signo == SIGQUIT) {
                printf("Got SIGQUIT\n");
                exit(EXIT_SUCCESS);
            } else {
                printf("Read unexpected signal\n");
            }
        }
    }
}

int main(int argc, char *argv[])
{
    sigset_t mask;
    int sfd, epollfd;
    struct epoll_event ev;

    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGQUIT);

    /* Block signals so that they aren't handled
       according to their default dispositions */

    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
        handle_error("sigprocmask");

    sfd = signalfd(-1, &mask, 0);
    if (sfd == -1)
        handle_error("signalfd");

    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        handle_error("epoll_create1");
    }

    ev.events = EPOLLIN | EPOLLOUT | EPOLLERR;
    ev.data.fd = sfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sfd, &ev) == -1) {
        handle_error("epoll_ctl: signalfd");
    }

    run_epoll(epollfd);

    return EXIT_SUCCESS;
}
