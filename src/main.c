#include "fmacros.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>

#include "main.h"
#include "logging.h"
#include "ipc.h"
#include "worker.h"
#include "networking.h"

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s port\n", argv[0]);
        exit(1);
    }

    char *port = argv[1];
    int listen_fd;//, conn_fd;
    int i, maxfd;//, sc;
    /* int ipc_buf, ipc_rc; */
    /* fd_set readset, masterset; */
    /* int last_used_worker = 0; */
    /* int available = N_WORKERS; */

    // Listen on port
    if ((listen_fd = he_listen(port)) < 0) {
        fprintf(stderr, "listen failed");
        exit(1);
    }

    // Pre-Fork workers
    workers = calloc(N_WORKERS, sizeof(struct worker));
    if (spawn_workers(N_WORKERS, workers, listen_fd) < 0) {
        err_kill_exit("spawning_workers failed");
    }

    // Prepare sets for select(2)
    /* FD_ZERO(&masterset); */
    maxfd = 0;

    // Add workers IPC pipes to masterset
    for (i = 0; i < N_WORKERS; i++) {
        /* FD_SET(workers[i].pipefd, &masterset); */
        maxfd = workers[i].pipefd > maxfd ? workers[i].pipefd : maxfd;
    }

    // Trap signals
    trap_sig(SIGINT, sigint_handler);

    printf("Helles booted up. %d workers listening on port %s\n",
            N_WORKERS, port);

    for ( ; ; ) {
        /* readset = masterset; */
        /* if (available <= 0) { */
        /*     // No worker is available. So do not accept until we have available */
        /*     // worker */
        /*     FD_CLR(listen_fd, &readset); */
        /* } */
        /*  */
        /* if ((sc = select(maxfd + 1, &readset, NULL, NULL, NULL)) < 0) { */
        /*     err_kill_exit("select failed"); */
        /* } */
        /*  */
        /* // Check if message from worker is ready to read */
        /* for (i = 0; i < N_WORKERS; i++) { */
        /*     if (FD_ISSET(workers[i].pipefd, &readset)) { */
        /*         if ((ipc_rc = read(workers[i].pipefd, &ipc_buf, 1)) == 0) { */
        /*             err_kill_exit("Could not read from worker socket"); */
        /*         } */
        /*         workers[i].available = 1; */
        /*         available++; */
        /*  */
        /*         // No workers ready to read anymore, no need to check them */
        /*         if (--sc == 0) { */
        /*             break; */
        /*         } */
        /*     } */
        /* } */
    }

    return 0;
}

void trap_sig(int sig, void (*sig_handler)(int))
{
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);

    sa.sa_handler = sig_handler;
    sa.sa_flags = 0;
#ifdef SA_RESTART
    sa.sa_flags |= SA_RESTART;
#endif

    if (sigaction(sig, &sa, NULL) < 0) {
        perror("sigaction");
        exit(1);
    }
}

void sigint_handler(int s)
{
    printf("Terminating...\n");

    kill_workers(N_WORKERS, workers);
    free(workers);

    exit(0);
}

void err_kill_exit(char *msg)
{
    fprintf(stderr, "%s\n", msg);
    kill_workers(N_WORKERS, workers);
    exit(1);
}

void kill_workers(int n, struct worker *workers)
{
    int i;

    for (i = 0; i < n; i++) {
        kill(workers[i].pid, SIGTERM);
    }
}

int spawn_workers(int n, struct worker *workers, int listen_fd)
{
    int i, pid, sockfd[2];

    for (i = 0; i < n; i++) {
        socketpair(AF_LOCAL, SOCK_STREAM, 0, sockfd);

        pid = fork();
        if (pid == 0) {
            // Child process
            close(sockfd[0]); // Close the 'parent-end' of the pipe

            worker_loop(listen_fd);
        } else if (pid > 0) {
            // Parent process
            close(sockfd[1]);
            workers[i].pid = pid;
            workers[i].available = 1;
            workers[i].pipefd = sockfd[0];
        } else {
            return -1;
        }
    }

    return 0;
}
