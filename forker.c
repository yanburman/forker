#include <sys/signalfd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/wait.h>
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

void run_epoll(int sfd);

static void do_forks(int num, int sfd)
{
    pid_t pid;
    int i;

    for (i = 0; i < num; ++i) {
        pid = fork();
        if (pid == -1)
            handle_error("fork");

        if (pid) {
            fprintf(stderr, "Forked %d\n", pid);
        } else {
            run_epoll(sfd);
        }
    }
}

void run_epoll(int sfd)
{
    struct epoll_event ev, events[MAX_EVENTS];
    int nfds, n, status;
    pid_t pid;
    struct signalfd_siginfo fdsi;
    ssize_t sz;

    int epollfd = epoll_create1(EPOLL_CLOEXEC);
    if (epollfd == -1) {
        handle_error("epoll_create1");
    }

    ev.events = EPOLLIN;
    ev.data.fd = sfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sfd, &ev) == -1) {
        handle_error("epoll_ctl: signalfd");
    }

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
                fprintf(stderr, "%d: Got SIGINT from %d\n", getpid(), fdsi.ssi_pid);
            } else if (fdsi.ssi_signo == SIGQUIT) {
                fprintf(stderr, "%d: Got SIGQUIT from %d\n", getpid(), fdsi.ssi_pid);
                exit(EXIT_SUCCESS);
            } else if (fdsi.ssi_signo == SIGTERM) {
                fprintf(stderr, "%d: Got SIGTERM from %d\n", getpid(), fdsi.ssi_pid);
                exit(EXIT_SUCCESS);
            } else if (fdsi.ssi_signo == SIGCHLD) {
                fprintf(stderr, "%d: Got SIGCHLD from %d\n", getpid(), fdsi.ssi_pid);
                do {
                    pid = waitpid(-1, &status, WNOHANG);
                    if (pid > 0) {
                        fprintf(stderr, "%d: Process %d exited\n", getpid(), pid);
                        do_forks(1, sfd);
                    }
                } while (pid > 0);
            } else {
                fprintf(stderr, "%d: Read unexpected signal from %d\n", getpid(), fdsi.ssi_pid);
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
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGQUIT);
    sigaddset(&mask, SIGCHLD);

    /* Block signals so that they aren't handled
       according to their default dispositions */

    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
        handle_error("sigprocmask");

    sfd = signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK);
    if (sfd == -1)
        handle_error("signalfd");

    // forking after epoll created leades to world of pain
    do_forks(1, sfd);

    run_epoll(sfd);

    return EXIT_SUCCESS;
}
