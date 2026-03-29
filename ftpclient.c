#include "csapp.h"
#include "request.h"
#include "slave_addr.h"
#include <sys/time.h>

static int port = 2121;
#define DIR_CLIENT "dirClient/"
#define BLOCK_SIZE 512

int main(int argc, char **argv)
{
    int clientfd;
    FILE *fout; // fichier de sortie pour sauvgarder le fichier recu
    char *host, buf[MAXLINE]; // host = adresse de serveur, buf = buffer de lecture
    char filename[512]; // chemin complet du fichier à sauvgarder
    char cmd[MAXLINE], arg[MAXLINE]; // cmd = "get", arg = nom du fichier
    // Q10 : chemin du fichier de progression (ex: dirClient/.lorem.txt.prog)
    char prog_filename[512];
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

    // Q14 : on ignore SIGPIPE pour detecter les pannes sans mourir
    signal(SIGPIPE, SIG_IGN);

    clientfd = Open_clientfd(host, port); // connexion au maitre sur le port 2121
    printf("client connected to server OS\n");

    // Q13 : le maitre envoie l'adresse de l'esclave qui va traiter ce client
    slave_addr_t slave_addr;
    Rio_readinitb(&rio, clientfd);
    Rio_readnb(&rio, &slave_addr, sizeof(slave_addr_t));

    // on ferme la connexion avec le maitre
    Close(clientfd);

    // Q14 : port=0 signifie qu'aucun esclave n'est disponible
    if (slave_addr.port == 0) {
        printf("Aucun esclave disponible, arret\n");
        exit(1);
    }

    // on se connecte directement a l'esclave
    clientfd = Open_clientfd(slave_addr.ip, slave_addr.port);
    printf("redirige vers l'esclave %s:%d\n", slave_addr.ip, slave_addr.port);

    Rio_readinitb(&rio, clientfd); // initialise le buffer rio sur le socket client

    while (Fgets(buf, MAXLINE, stdin) != NULL) { // lit une ligne depuis le clavier jusqu'à EOF ou bye 
        
        buf[strcspn(buf, "\r\n")] = '\0'; // enlever le retour à la ligne (Windows ou Linux)
        
        int parsed = sscanf(buf, "%s %s", cmd, arg);
        
        // commande bye donc on prévient le serveur puis on quitte
        if (parsed >= 1 && strcmp(cmd, "bye") == 0) {
            request_t req;
            req.type = BYE;
            Rio_writen(clientfd, &req, sizeof(request_t));
            break;
        }
        
        if (parsed == 2) { // découper la saisie
            request_t req;
            if (strcmp(cmd, "get") == 0 || strcmp(cmd, "GET") == 0) { // vérifier que la commande est bien "GET" ou "get"

                // Q10 : fichier qui garde la progression ex: "dirClient/.lorem.txt.prog"
                snprintf(prog_filename, sizeof(prog_filename), "%s.%.255s.prog", DIR_CLIENT, arg);

                // Q10 : si le fichier .prog existe, on reprend depuis le dernier bloc sauvegardé
                size_t offset = 0; // 0 = nouveau téléchargement
                FILE *fprog = fopen(prog_filename, "rb");
                if (fprog != NULL) {
                    fread(&offset, sizeof(size_t), 1, fprog);
                    fclose(fprog);
                    printf("Reprise depuis le bloc %zu\n", offset);
                }

                req.type = GET;
                strncpy(req.nom, arg, 255); // copie le nom de fichier
                req.nom[255] = '\0';
                req.offset = offset; // Q10 : blocs deja recus (0 si nouveau)


                // Q14 : rio_writen minuscule pour detecter si l'esclave est tombe
                if (rio_writen(clientfd, &req, sizeof(request_t)) < 0) {
                    printf("Esclave hors service, reconnexion au maitre...\n");
                    Close(clientfd);

                    // se reconnecter au maitre pour avoir un nouvel esclave
                    clientfd = Open_clientfd(host, port);
                    Rio_readinitb(&rio, clientfd);
                    Rio_readnb(&rio, &slave_addr, sizeof(slave_addr_t));
                    Close(clientfd);

                    if (slave_addr.port == 0) {
                        printf("Aucun esclave disponible, arret\n");
                        exit(1);
                    }

                    clientfd = Open_clientfd(slave_addr.ip, slave_addr.port);
                    Rio_readinitb(&rio, clientfd);
                    printf("Connecte au nouvel esclave %s:%d\n", slave_addr.ip, slave_addr.port);

                    // renvoyer la meme requete au nouvel esclave
                    if (rio_writen(clientfd, &req, sizeof(request_t)) < 0) {
                        printf("Echec de reconnexion\n");
                        break;
                    }
                }

                // Lecture de la reponse_t envoyée par le serveur
                response_t res;
                if (Rio_readnb(&rio, &res, sizeof(response_t)) > 0) {
                    if (res.code == SUCCES) {

                        strcpy(filename, DIR_CLIENT); // construire le chemin de sauvgarde
                        strcat(filename, arg);

                        // Q10 : si reprise on continue le fichier, sinon on repart de zero
                        if (offset > 0) {
                            fout = fopen(filename, "ab"); // append
                        } else {
                            fout = fopen(filename, "wb"); // nouveau fichier
                        }

                        if (fout == NULL) {
                            fprintf(stderr, "Impossible d'ouvrir %s en écriture\n", filename);
                        } else {
                            // on calcule le temps de téléchargement c'est à dire le temps entre le premiere octet reçu et le dernier
                            struct timeval start, end;
                            ssize_t n;
                            size_t total_bytes = 0;

                            gettimeofday(&start, NULL); // enregistre l'heure de début

                            // Reception du nombre de blocs envoyé par le serveur
                            size_t nb_blocs;
                            Rio_readnb(&rio, &nb_blocs, sizeof(size_t));

                            // Q10 : nombre de blocs qu'il reste a recevoir
                            size_t nb_blocs_a_recevoir = (offset < nb_blocs) ? (nb_blocs - offset) : 0;

                            char bloc[BLOCK_SIZE];

                            // on lit les blocs manquants
                            for (size_t i = 0; i < nb_blocs_a_recevoir; i++) {

                                n = Rio_readnb(&rio, bloc, BLOCK_SIZE); // nombre d'octets lus dans le bloc courant
                                fwrite(bloc, 1, n, fout);
                                total_bytes += n;

                                // Q10 : on sauvegarde la progression apres chaque bloc (en cas de crash)
                                size_t blocs_recus_total = offset + i + 1;
                                FILE *fp_prog = fopen(prog_filename, "wb");
                                if (fp_prog != NULL) {
                                    fwrite(&blocs_recus_total, sizeof(size_t), 1, fp_prog);
                                    fclose(fp_prog);
                                }
                            }

                            fclose(fout);

                            // Q10 : transfert fini, on supprime le fichier de progression
                            remove(prog_filename);

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
        } else if (parsed == 1 && (strcmp(cmd, "ls") == 0 || strcmp(cmd, "LS") == 0)) {
            // Q15 : demander la liste des fichiers du serveur
            request_t req;
            memset(&req, 0, sizeof(request_t)); // tout a zero pour eviter les valeurs parasites
            req.type = LS;

            Rio_writen(clientfd, &req, sizeof(request_t));

            response_t res;
            if (Rio_readnb(&rio, &res, sizeof(response_t)) > 0 && res.code == SUCCES) {
                size_t n;
                char buf[4096];
                Rio_readnb(&rio, &n, sizeof(size_t));
                if (n > 0) {
                    Rio_readnb(&rio, buf, n);
                    buf[n] = '\0';
                    printf("%s", buf);
                }
            } else {
                printf("Erreur lors du ls\n");
            }
        } else {
            printf("Usage : get <nom_fichier> | ls | bye\n"); // erreur dans les arguments
        }
    }

    Close(clientfd);
    exit(0);
}
