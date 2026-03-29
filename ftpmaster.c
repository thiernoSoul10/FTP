#include "csapp.h"
#include "request.h"

#define NB_SLAVES   2     // nombre statique d'esclaves
#define MASTER_PORT 2121  // port d'écoute pour les clients
#define SLAVE_BASE_PORT 3131 // port de base des esclaves

typedef struct {
    char ip[16];
    int  port;
} slave_info_t;

int main(int argc, char **argv)
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
    slave_info_t slaves[NB_SLAVES];
    int current_slave = 0;

    /* -------------------------------------------------------
     * Q12 : Connexion préalable à chaque esclave (handshake)
     * Le maître établit NB_SLAVES connexions AVANT d'accepter
     * des clients, et récupère les infos de chaque esclave.
     * ------------------------------------------------------- */
    printf("Connexion aux esclaves...\n");
    for (int i = 0; i < NB_SLAVES; i++) {
        int port = SLAVE_BASE_PORT + i;
        int fd = -1;

        // Retry jusqu'à ce que l'esclave soit prêt
        while (fd < 0) {
            fd = open_clientfd("127.0.0.1", port);
            if (fd < 0) {
                fprintf(stderr, "Esclave %d (%d) pas encore prêt, nouvelle tentative...\n", i, port);
                sleep(1);
            }
        }

        // Envoi du HELLO
        request_t req;
        memset(&req, 0, sizeof(req));
        req.type = HELLO;
        Rio_writen(fd, &req, sizeof(request_t));

        // Réception : response_t + slave_info_t
        rio_t rio;
        Rio_readinitb(&rio, fd);

        response_t res;
        Rio_readnb(&rio, &res, sizeof(response_t));

        if (res.code == SUCCES) {
            Rio_readnb(&rio, &slaves[i], sizeof(slave_info_t));
            // Forcer l'IP à 127.0.0.1 car getsockname retourne 0.0.0.0 en local
            strcpy(slaves[i].ip, "127.0.0.1");
            slaves[i].port = port;
            printf("Esclave %d confirmé : %s:%d\n", i, slaves[i].ip, slaves[i].port);
        } else {
            fprintf(stderr, "Esclave %d a refusé le handshake\n", i);
            Close(fd);
            exit(1);
        }

        Close(fd);
    }

    printf("Tous les esclaves prêts. Serveur maître démarré sur le port %d\n", MASTER_PORT);

    /* -------------------------------------------------------
     * Boucle principale : redirection Round-Robin des clients
     * ------------------------------------------------------- */
    listenfd = Open_listenfd(MASTER_PORT);
    clientlen = sizeof(clientaddr);

    while (1) {
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        printf("Client connecté, redirection vers l'esclave %d (%s:%d)\n",
               current_slave, slaves[current_slave].ip, slaves[current_slave].port);

        // Q13 : Envoi des infos de l'esclave au client
        Rio_writen(connfd, &slaves[current_slave], sizeof(slave_info_t));

        Close(connfd);

        // Round-Robin
        current_slave = (current_slave + 1) % NB_SLAVES;
    }

    return 0;
}