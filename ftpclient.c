#include "csapp.h"

static int port = 2121;
#define DIR_CLIENT "dirClient/"

int main(int argc, char **argv)
{
    int clientfd;
    FILE *fout;
    char *host, buf[MAXLINE];
    char filename[512];
    rio_t rio;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <host>\n", argv[0]);
        exit(0);
    }
    strcpy(filename, DIR_CLIENT);
    strcat(filename, "out");
    host = argv[1];
    fout = fopen(filename, "wb");
    if(fout == NULL){
        fprintf(stderr, "Impossible d'ouvrir %s", filename);
        exit(-1);
    }

    /*
     * Note that the 'host' can be a name or an IP address.
     * If necessary, Open_clientfd will perform the name resolution
     * to obtain the IP address.
     */
    clientfd = Open_clientfd(host, port);
    
    /*
     * At this stage, the connection is established between the client
     * and the server OS ... but it is possible that the server application
     * has not yet called "Accept" for this connection
     */
    printf("client connected to server OS\n"); 
    
    Rio_readinitb(&rio, clientfd);

    if (Fgets(buf, MAXLINE, stdin) != NULL) {
        Rio_writen(clientfd, buf, strlen(buf));

        size_t n;
        while ((n = Rio_readnb(&rio, buf, MAXLINE)) > 0) {
            fwrite(buf, 1, n, fout);
        }
    }

    Close(clientfd);
    exit(0);
}
