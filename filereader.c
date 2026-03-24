/*
 * filereader - the client gives a filename and the until client closes connection
 */
#include "csapp.h"
#include "filereader.h"

#define DIR_SERVER "dirServer/"

#define BLOCK_SIZE 512


response_t filereader(int connfd, char fichier[256])
{
    
    int fd; // file descriptor du fichier à lire 
    response_t res;
    char path[512]; // chemin complet de fichier
    
    // ceci est pour construire le chemin complet : répertoire serveur + nom du fichier 
    strcpy(path, DIR_SERVER);
    strcat(path, fichier);
    printf("fichier: %s\n", path);

    if ((fd = open(path, O_RDONLY)) < 0) {
        res.code = ERREUR;
        Rio_writen(connfd, &res, sizeof(res)); // on prévient le client en cas d'erreur 
        return res;
    }

    // on envoie SUCCES d'abord puis les données 
    res.code = SUCCES;
    Rio_writen(connfd, &res, sizeof(res));

    // Calculer la taille de fichier : 
        size_t file_size = lseek(fd, 0, SEEK_END);
        lseek(fd, 0, SEEK_SET);
    
    // Calculer et envoyer le nombre de blocs : 
        size_t nb_blocs = (file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
        Rio_writen(connfd, &nb_blocs, sizeof(size_t));
    
    // Envoyer les blocs : 

    char buf[BLOCK_SIZE];

    ssize_t n;
    while ((n = Rio_readn(fd, buf, BLOCK_SIZE)) > 0) {
        if (n < BLOCK_SIZE) {
            memset(buf + n, 0, BLOCK_SIZE - n);
        }
        Rio_writen(connfd, buf, BLOCK_SIZE);
    }

    close(fd);
    return res;
}

