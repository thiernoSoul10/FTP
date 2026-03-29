#include "csapp.h"
#include "request.h"

#define MAX_NAME_LEN 256 
#define NB_PROC 10 // Nb de processus 
static int port = 2121; // Nb de Port 

static pid_t workers[NB_PROC]; // tableau qui stocke les pids des fils 

// Handler 1 : Appeler quand un fils termine 
void child_handler(int sig){
    while (Waitpid(-1, NULL, WNOHANG) > 0); 
    // Sans WNOHANG => le serveur s'arrete est attend qu'un fils meure 
    // Option qui dit à Waitpid ne bloque pas, si aucun fils n'est mort reviens immédiatement 
}

void parent_stop_handler(int sig){
    printf("\nServer shutting down...\n");
    // parcours le tableau des fils et tue chaque fils 
    for (int i = 0; i < NB_PROC; i++) {
        kill(workers[i], SIGTERM);
    }
    // attendre que tous les fils meurent avant de continuer 
    while(wait(NULL) > 0);

    exit(0); // le pére quite
}

/* 
 * Note that this code only works with IPv4 addresses
 * (IPv6 is not supported)
 */
int main(int argc, char **argv)
{
    int listenfd, connfd;
    socklen_t clientlen; // taille de l'adresse client 
    struct sockaddr_in clientaddr; // addresse de client : IP + Port 
    char client_ip_string[INET_ADDRSTRLEN]; // IP client en chaine de char
    char client_hostname[MAX_NAME_LEN]; // nom d'hote
    
    Signal(SIGCHLD, child_handler); 
    Signal(SIGINT, parent_stop_handler);
    
    clientlen = (socklen_t)sizeof(clientaddr);

    listenfd = Open_listenfd(port);
    
        
    for(int i = 0; i < NB_PROC; i++){

        pid_t p = Fork();

        if(p < 0) exit(-3);

        if(p == 0){
            while (1) {   
                // on attends qu'un client se connecte      
                connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
                
                // on affiche les infos du client conecté 
                printf("server connected to %s (%s) port <%d>\n", client_hostname,
                    client_ip_string, port);

                // traitement de la requete 
                response_t res = requestHandler(connfd);
                
                afficherResponse(res); // affiche sur le serveur si la requete a réussi ou échoué 
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

