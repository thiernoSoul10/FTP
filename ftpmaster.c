#include "csapp.h"
#include "request.h"
#include "slave_addr.h"

// Q11 : constantes du serveur maitre
#define NB_SLAVES 1        // nombre d'esclaves (1 pour tester sur une seule machine)
#define PORT_MAITRE 2121   // port sur lequel les clients se connectent
#define PORT_ESCLAVE 2122  // port sur lequel les esclaves ecoutent (different de 2121)

// Q12 : infos d'un esclave que le maitre garde en memoire
typedef struct {
    int fd;      // socket de connexion entre le maitre et l'esclave
    char ip[64]; // adresse IP de l'esclave
    int port;    // port de l'esclave
} slave_info_t;

// Q14 : tester si un esclave repond encore
// open_clientfd minuscule retourne -1 si ca marche pas (pas d'exit comme la version majuscule)
int esclave_vivant(slave_info_t *e) {
    int fd = open_clientfd(e->ip, e->port);
    if (fd >= 0) {
        Close(fd);
        return 1; // esclave accessible
    }
    return 0; // esclave hors service
}

int main(int argc, char **argv)
{
    // le maitre attend les paires "ip port" pour chaque esclave
    // ex : ./ftpmaster localhost 2122 localhost 2123
    if (argc != 1 + NB_SLAVES * 2) {
        fprintf(stderr, "usage: %s <ip1> <port1> <ip2> <port2> ...\n", argv[0]);
        exit(1);
    }

    // Q12 : se connecter a chaque esclave et garder leurs infos
    // le maitre doit faire ca AVANT d'accepter des clients
    slave_info_t esclaves[NB_SLAVES];

    for (int i = 0; i < NB_SLAVES; i++) {
        char *host = argv[1 + i * 2];
        int port   = atoi(argv[2 + i * 2]);

        esclaves[i].fd = Open_clientfd(host, port);
        strncpy(esclaves[i].ip, host, 63);
        esclaves[i].ip[63] = '\0';
        esclaves[i].port = port;
        printf("Connecte a l'esclave %d : %s:%d\n", i, esclaves[i].ip, esclaves[i].port);
    }

    printf("Tous les esclaves connectes, pret a accepter des clients\n");

    // Q13 : le maitre ecoute les clients sur PORT_MAITRE
    int listenfd = Open_listenfd(PORT_MAITRE);
    printf("Maitre en ecoute sur le port %d\n", PORT_MAITRE);

    socklen_t clientlen = (socklen_t)sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;

    // tourniquet : on distribue les clients entre les esclaves a tour de role
    int tour = 0;

    while (1) {
        int connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        // Q14 : chercher un esclave vivant en partant du tourniquet
        slave_addr_t addr;
        addr.port = 0; // port=0 signifie aucun esclave dispo

        for (int essai = 0; essai < NB_SLAVES; essai++) {
            int idx = (tour + essai) % NB_SLAVES;
            if (esclave_vivant(&esclaves[idx])) {
                strncpy(addr.ip, esclaves[idx].ip, 63);
                addr.ip[63] = '\0';
                addr.port = esclaves[idx].port;
                tour = (idx + 1) % NB_SLAVES; // avancer le tourniquet
                break;
            }
        }

        if (addr.port == 0) {
            printf("Aucun esclave disponible\n");
        } else {
            printf("Client redirige vers %s:%d\n", addr.ip, addr.port);
        }

        // envoyer les infos au client (port=0 si aucun esclave dispo)
        Rio_writen(connfd, &addr, sizeof(slave_addr_t));

        // le maitre ferme sa connexion, le client se connecte directement a l'esclave
        Close(connfd);
    }

    return 0;
}
