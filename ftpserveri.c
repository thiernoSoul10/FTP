#include "csapp.h"
#include "request.h"

#define MAX_NAME_LEN 256
#define NB_PROC 10
static int port = 2121;

static pid_t workers[NB_PROC];


void child_handler(int sig){
    while (Waitpid(-1, NULL, WNOHANG) > 0);
}

void parent_stop_handler(int sig){
    printf("\nServer shutting down...\n");

    for (int i = 0; i < NB_PROC; i++) {
        kill(workers[i], SIGTERM);
    }
    // attendre que tous les fils meurent
    while(wait(NULL) > 0);

    exit(0);
}

/* 
 * Note that this code only works with IPv4 addresses
 * (IPv6 is not supported)
 */
int main(int argc, char **argv)
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
    char client_ip_string[INET_ADDRSTRLEN];
    char client_hostname[MAX_NAME_LEN];
    
    Signal(SIGCHLD, child_handler);
    Signal(SIGINT, parent_stop_handler);
    
    clientlen = (socklen_t)sizeof(clientaddr);

    listenfd = Open_listenfd(port);
    
        
    for(int i = 0; i < NB_PROC; i++){

        pid_t p = Fork();

        if(p < 0) exit(-3);

        if(p == 0){
            while (1) {        
                connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    
                /* determine the name of the client */
                Getnameinfo((SA *) &clientaddr, clientlen,
                            client_hostname, MAX_NAME_LEN, 0, 0, 0);
                
                /* determine the textual representation of the client's IP address */
                Inet_ntop(AF_INET, &clientaddr.sin_addr, client_ip_string,
                        INET_ADDRSTRLEN);
                
                printf("server connected to %s (%s) port <%d>\n", client_hostname,
                    client_ip_string, port);

                response_t res = requestHandler(connfd);

                afficherResponse(res);
                Close(connfd);
            }

            exit(0);
        }

        workers[i] = p;
        
    }

    while (1) {
        pause();
    }

    exit(0);
}

