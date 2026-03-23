#include "csapp.h"
#include "request.h"
#include <sys/time.h>

static int port = 2121;
#define DIR_CLIENT "dirClient/"

int main(int argc, char **argv)
{
    int clientfd;
    FILE *fout; // fichier de sortie pour sauvgarder le fichier recu 
    char *host, buf[MAXLINE]; // host = adresse de serveur, buf = buffer de lecture
    char filename[512]; // chemin complet du fichier à sauvgarder 
    char cmd[MAXLINE], arg[MAXLINE]; // cmd = "get", arg = nom du fichier 
    rio_t rio;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <host>\n", argv[0]);
        exit(0);
    }

    host = argv[1];

    /*
     * Note that the 'host' can be a name or an IP address.
     * If necessary, Open_clientfd will perform the name resolution
     * to obtain the IP address.
     */
     
    clientfd = Open_clientfd(host, port); // établit la connexion TCP avec le serveur sur le port 2121
    
    /*
     * At this stage, the connection is established between the client
     * and the server OS ... but it is possible that the server application
     * has not yet called "Accept" for this connection
     */
    
    printf("client connected to server OS\n"); 
    
    
    Rio_readinitb(&rio, clientfd); // initialise le buffer rio sur le socket client 

    if (Fgets(buf, MAXLINE, stdin) != NULL) { // lit une ligne depuis le clavier 
        
        buf[strcspn(buf, "\n")] = '\0'; // enlever le retour à la ligne 
        
        
        if (sscanf(buf, "%s %s", cmd, arg) == 2) { // découper la saise 
            request_t req;
            if (strcmp(cmd, "get") == 0 || strcmp(cmd, "GET") == 0) { // vérifier que la commande est bien "GET" ou "get"
                req.type = GET;
                strncpy(req.nom, arg, 255); // copie le nom de fichier 
                req.nom[255] = '\0';
                
                
                Rio_writen(clientfd, &req, sizeof(request_t)); // envoyer la structure au serveur 

                // Lecture de la reponse_t envoyée par le serveur
                response_t res;
                if (Rio_readnb(&rio, &res, sizeof(response_t)) > 0) {
                    if (res.code == SUCCES) {
                    
                        strcpy(filename, DIR_CLIENT); // construire le chemin de sauvgarde 
                        strcat(filename, arg);
                        fout = fopen(filename, "wb"); // ouvrir le fichier en écriture binaire 
                        
                        if (fout == NULL) {
                            fprintf(stderr, "Impossible d'ouvrir %s en écriture\n", filename);
                        } else {
                            // on calcule le temps de téléchargement c'est à dire le temps entre le premiere octet reçu et le dernier 
                            struct timeval start, end;
                            size_t n;
                            size_t total_bytes = 0;
                            
                            gettimeofday(&start, NULL); // enregistre l'heure de début
                            
                            while ((n = Rio_readnb(&rio, buf, MAXLINE)) > 0) {
                                fwrite(buf, 1, n, fout);
                                total_bytes += n; // On compte les octets reçus
                            }
                            fclose(fout);
                            
                            gettimeofday(&end, NULL); // arret chrono 
                            
                            //  Calcul des statistiques : 
                            double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000;
                            // Calcul la vitesse en Ko/s 
                            // si le transfert est instantané donc elapsed = 0, on mets 0 pour eviter la division par 0
                            double kbps = (elapsed > 0) ? (total_bytes / 1024) / elapsed : 0;
                            
                            // l'affichage demandé dans le sujet : 
                            printf("Transfer successfully complete.\n");
                            printf("%zu bytes received in %.4f seconds (%.2f Kbytes/s).\n", 
                                   total_bytes, elapsed, kbps);
                        }
                    } else {
                        printf("Le serveur a répondu : Erreur (fichier introuvable ou commande invalide)\n");
                    }
                } else {
                    printf("Erreur de connexion avec le serveur\n");
                }
            } else {
                printf("Commande n'est pas supporte par le client\n"); // commande différente de "get" ou "GET"
            }
        } else {
            printf("Usage : get <nom_fichier>\n"); // erreur dans les arguments 
        }
    }

    Close(clientfd);
    exit(0);
}
