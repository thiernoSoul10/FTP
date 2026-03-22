/*
 * filereader - the client gives a filename and the until client closes connection
 */
#include "csapp.h"
#include "filereader.h"

#define DIR_SERVER "dirServer/"

response_t filereader(int connfd, char fichier[256])
{
    size_t n;
    int fd;
    response_t res;
    char path[512];
    
    // ceci est pour construire le chemin complet : répertoire serveur + nom du fichier 
    strcpy(path, DIR_SERVER);
    strcat(path, fichier);
    printf("fichier: %s\n", path);

    if ((fd = open(path, O_RDONLY)) < 0) {
        res.code = ERREUR;
        Rio_writen(connfd, &res, sizeof(res)); // on envoie une structure response_t au lieu d'un texte 
        return res;
    }

    // on envoie SUCCES d'abord puis les données 
    
    res.code = SUCCES;
    Rio_writen(connfd, &res, sizeof(res));

    while ((n = read(fd, fichier, 256)) > 0) {
        Rio_writen(connfd, fichier, n);
    }

    close(fd);
    return res;
}

