/*
 * filereader - the client gives a filename and the until client closes connection
 */
#include "csapp.h"
#include "filereader.h"

response_t filereader(int connfd, char fichier[256])
{
    size_t n;
    int fd;
    response_t res;

    printf("fichier: %s\n", fichier);

    if ((fd = open(fichier, O_RDONLY)) < 0) {
        sprintf(fichier, "Error: cannot open file\n");
        Rio_writen(connfd, fichier, strlen(fichier));
        res.code = ERREUR;
        return res;
    }

    while ((n = read(fd, fichier, 256)) > 0) {
        Rio_writen(connfd, fichier, n);
    }

    close(fd);
    res.code = SUCCES;
    return res;
}

