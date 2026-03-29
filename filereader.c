/*
 * filereader - the client gives a filename and the until client closes connection
 */
#include "csapp.h"
#include "filereader.h"
#include <unistd.h> // pour usleep (TEST Q10 seulement, a supprimer apres)
#include <dirent.h> // Q15 : pour opendir/readdir

#define DIR_SERVER "dirServer/"

#define BLOCK_SIZE 512


// Q15 : liste les fichiers du repertoire serveur et envoie le resultat au client
response_t filels(int connfd) {
    response_t res;
    char buf[4096];
    int len = 0;

    // ouvrir le repertoire serveur
    DIR *d = opendir(DIR_SERVER);
    if (d == NULL) {
        res.code = ERREUR;
        rio_writen(connfd, &res, sizeof(res));
        return res;
    }

    // lire les entrees et les concatener dans buf
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        // on saute . et ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        int n = snprintf(buf + len, sizeof(buf) - len, "%s\n", entry->d_name);
        if (n > 0) len += n;
    }
    closedir(d);

    // envoyer SUCCES, la taille, puis le contenu
    res.code = SUCCES;
    rio_writen(connfd, &res, sizeof(res));
    size_t total = (size_t)len;
    rio_writen(connfd, &total, sizeof(size_t));
    if (total > 0) {
        rio_writen(connfd, buf, total);
    }

    return res;
}

// Q10 : offset = blocs que le client a deja (0 = transfert normal)
response_t filereader(int connfd, char fichier[256], size_t offset)
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

    // Calculer et envoyer le nombre de blocs total :
        size_t nb_blocs = (file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
        Rio_writen(connfd, &nb_blocs, sizeof(size_t));

    // Q10 : on se déplace dans le fichier pour sauter les blocs deja recus
    if (offset > 0 && offset < nb_blocs) {
        lseek(fd, (off_t)(offset * BLOCK_SIZE), SEEK_SET);
    }

    // Q10 : blocs qu'il reste a envoyer (si offset=0 on envoie tout)
    size_t blocs_a_envoyer = (offset < nb_blocs) ? (nb_blocs - offset) : 0;
    size_t envoyes = 0;

    // Envoyer les blocs manquants :
    char buf[BLOCK_SIZE];
    ssize_t n;

    while (envoyes < blocs_a_envoyer && (n = Rio_readn(fd, buf, BLOCK_SIZE)) > 0) {
        if (n < BLOCK_SIZE) {
            memset(buf + n, 0, BLOCK_SIZE - n);
        }
        usleep(10000); // TEST Q10 : delai artificiel pour avoir le temps de crasher (a supprimer apres)
        // Q10 : rio_writen minuscule retourne -1 si le client crashe (Rio majuscule ferait exit)
        if (rio_writen(connfd, buf, BLOCK_SIZE) < 0) {
            // client crashe, on ferme le fichier et on sort proprement
            close(fd);
            res.code = ERREUR;
            return res;
        }
        envoyes++;
    }

    close(fd);
    return res;
}

