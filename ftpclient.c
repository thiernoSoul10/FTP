#include "csapp.h"
#include "request.h"
#include <sys/time.h>

#define MASTER_PORT 2121
#define DIR_CLIENT  "dirClient/"
#define BLOCK_SIZE  512

typedef struct {
    char ip[INET_ADDRSTRLEN];
    int  port;
} slave_info_t;

/* ------------------------------------------------------------------ */
/*  Q14 : Connexion au master puis à l'esclave désigné                 */
/* ------------------------------------------------------------------ */

static char g_host[256]; // adresse du master (argv[1])

/*
 * Se connecte au master, récupère les infos d'un esclave,
 * puis se connecte à cet esclave.
 * Met à jour *clientfd et *rio.
 * Retourne 0 en cas de succès, -1 en cas d'échec.
 */
static int connect_to_slave(int *clientfd, rio_t *rio)
{
    int masterfd = open_clientfd(g_host, MASTER_PORT);
    if (masterfd < 0) {
        fprintf(stderr, "Impossible de contacter le master\n");
        return -1;
    }

    slave_info_t slave;
    rio_t rio_master;
    Rio_readinitb(&rio_master, masterfd);
    if (Rio_readnb(&rio_master, &slave, sizeof(slave_info_t)) <= 0) {
        fprintf(stderr, "Impossible de récupérer les infos d'un esclave\n");
        Close(masterfd);
        return -1;
    }
    printf("Redirige vers esclave : %s:%d\n", slave.ip, slave.port);
    Close(masterfd);

    *clientfd = open_clientfd(slave.ip, slave.port);
    if (*clientfd < 0) {
        fprintf(stderr, "Impossible de se connecter à l'esclave %s:%d\n",
                slave.ip, slave.port);
        return -1;
    }
    Rio_readinitb(rio, *clientfd);
    printf("client connected to slave\n");
    return 0;
}

/*
 * Q14 : Détecte une panne, ferme l'ancienne connexion et
 * se reconnecte automatiquement à un esclave disponible.
 * Retourne 0 si la reconnexion a réussi, -1 sinon.
 */
static int handle_reconnect(int *clientfd, rio_t *rio)
{
    printf("\n[Q14] Esclave en panne, reconnexion automatique...\n");
    Close(*clientfd);
    *clientfd = -1;

    // Réessaye jusqu'à 5 fois
    for (int attempt = 1; attempt <= 5; attempt++) {
        sleep(1);
        printf("[Q14] Tentative %d/5...\n", attempt);
        if (connect_to_slave(clientfd, rio) == 0) {
            printf("[Q14] Reconnexion réussie. Relancez votre commande.\n");
            return 0;
        }
    }
    fprintf(stderr, "[Q14] Échec de la reconnexion après 5 tentatives.\n");
    return -1;
}

/* ------------------------------------------------------------------ */
/*  Macro de vérification d'écriture avec failover                     */
/* ------------------------------------------------------------------ */
#define SAFE_WRITE(fd, buf, n, clientfd_ptr, rio_ptr, label)       \
    do {                                                            \
        if (Rio_writen((fd), (buf), (n)) < 0) {                    \
            if (handle_reconnect((clientfd_ptr), (rio_ptr)) < 0)   \
                goto fatal_error;                                   \
            goto label;                                             \
        }                                                           \
    } while(0)

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
    int clientfd;
    FILE *fout;
    char buf[MAXLINE];
    char filename[8704];
    char cmd[MAXLINE], arg[MAXLINE];
    rio_t rio;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <host>\n", argv[0]);
        exit(0);
    }

    strncpy(g_host, argv[1], sizeof(g_host) - 1);

    // Connexion initiale (Q13)
    if (connect_to_slave(&clientfd, &rio) < 0) {
        fprintf(stderr, "Impossible de démarrer. Vérifiez que master et esclave tournent.\n");
        exit(1);
    }

    while (Fgets(buf, MAXLINE, stdin) != NULL) {

        buf[strcspn(buf, "\r\n")] = '\0';

        int parsed = sscanf(buf, "%s %s", cmd, arg);

        /* ---- bye ---- */
        if (parsed >= 1 && strcmp(cmd, "bye") == 0) {
            request_t req;
            memset(&req, 0, sizeof(req));
            req.type = BYE;
            Rio_writen(clientfd, &req, sizeof(request_t));
            break;
        }

        /* ---- auth ---- */
        char arg2[MAXLINE];
        if (parsed >= 1 && strcmp(cmd, "auth") == 0) {
            if (sscanf(buf, "%s %s %s", cmd, arg, arg2) < 3) {
                printf("Usage : auth <login> <motdepasse>\n");
                continue;
            }
            request_t req;
            memset(&req, 0, sizeof(req));
            req.type = AUTH;
            strncpy(req.nom, arg, 255);
            req.nom[255] = '\0';
            strncpy(req.password, arg2, 255);
            req.password[255] = '\0';

        retry_auth:
            if (rio_writen(clientfd, &req, sizeof(request_t)) < 0) {
                if (handle_reconnect(&clientfd, &rio) < 0) goto fatal_error;
                goto retry_auth;
            }
            response_t res;
            Rio_readnb(&rio, &res, sizeof(response_t));
            if (res.code == SUCCES)
                printf("Authentification réussie. Bienvenue, %s!\n", arg);
            else
                printf("Authentification échouée. Login ou mot de passe incorrect.\n");
            continue;
        }

        /* ---- ls ---- */
        if (parsed >= 1 && (strcmp(cmd, "ls") == 0 || strcmp(cmd, "LS") == 0)) {
            request_t req;
            memset(&req, 0, sizeof(req));
            req.type = LS;

        retry_ls:
            if (rio_writen(clientfd, &req, sizeof(request_t)) < 0) {
                if (handle_reconnect(&clientfd, &rio) < 0) goto fatal_error;
                goto retry_ls;
            }
            response_t res;
            Rio_readnb(&rio, &res, sizeof(response_t));
            if (res.code == SUCCES) {
                size_t len;
                Rio_readnb(&rio, &len, sizeof(size_t));
                char *lsbuf = malloc(len + 1);
                Rio_readnb(&rio, lsbuf, len);
                lsbuf[len] = '\0';
                printf("%s", lsbuf);
                free(lsbuf);
            } else {
                printf("Erreur lors de l'exécution de ls\n");
            }
            continue;
        }

        if (parsed == 2) {
            request_t req;
            memset(&req, 0, sizeof(req));

            /* ---- get ---- */
            if (strcmp(cmd, "get") == 0 || strcmp(cmd, "GET") == 0) {
                req.type = GET;
                strncpy(req.nom, arg, 255);
                req.nom[255] = '\0';

                char state_file[MAXLINE + 32];
                snprintf(state_file, sizeof(state_file), "%s%s.state", DIR_CLIENT, arg);
                size_t bloc_debut = 0;
                FILE *fs = fopen(state_file, "r");
                if (fs) { fscanf(fs, "%zu", &bloc_debut); fclose(fs); }
                req.bloc_debut = bloc_debut;

            retry_get:
                if (rio_writen(clientfd, &req, sizeof(request_t)) < 0) {
                    if (handle_reconnect(&clientfd, &rio) < 0) goto fatal_error;
                    goto retry_get;
                }
                response_t res;
                if (Rio_readnb(&rio, &res, sizeof(response_t)) > 0) {
                    if (res.code == SUCCES) {
                        snprintf(filename, sizeof(filename), "%s%s", DIR_CLIENT, arg);
                        fout = (bloc_debut > 0) ? fopen(filename, "ab") : fopen(filename, "wb");
                        if (fout) {
                            struct timeval start, end;
                            size_t nb_blocs, total_bytes = 0;
                            gettimeofday(&start, NULL);
                            Rio_readnb(&rio, &nb_blocs, sizeof(size_t));
                            char bloc[BLOCK_SIZE];
                            FILE *fs_state = fopen(state_file, "w");
                            for (size_t i = 0; i < nb_blocs; i++) {
                                usleep(50);
                                ssize_t n = Rio_readnb(&rio, bloc, BLOCK_SIZE);
                                fwrite(bloc, 1, n, fout);
                                total_bytes += n;
                                if (fs_state) {
                                    rewind(fs_state);
                                    fprintf(fs_state, "%zu       ", bloc_debut + i + 1);
                                    fflush(fs_state);
                                }
                            }
                            if (fs_state) fclose(fs_state);
                            remove(state_file);
                            fclose(fout);
                            gettimeofday(&end, NULL);
                            double elapsed = (end.tv_sec - start.tv_sec)
                                           + (end.tv_usec - start.tv_usec) / 1e6;
                            double kbps = (elapsed > 0) ? (total_bytes / 1024.0) / elapsed : 0;
                            printf("Transfer successfully complete.\n");
                            printf("%zu bytes received in %.4f seconds (%.2f Kbytes/s).\n",
                                   total_bytes, elapsed, kbps);
                        }
                    } else {
                        printf("Erreur : fichier introuvable ou commande invalide.\n");
                    }
                }

            /* ---- rm ---- */
            } else if (strcmp(cmd, "rm") == 0 || strcmp(cmd, "RM") == 0) {
                req.type = RM;
                strncpy(req.nom, arg, 255);
                req.nom[255] = '\0';
                req.replicate = 1;

            retry_rm:
                if (rio_writen(clientfd, &req, sizeof(request_t)) < 0) {
                    if (handle_reconnect(&clientfd, &rio) < 0) goto fatal_error;
                    goto retry_rm;
                }
                response_t res;
                Rio_readnb(&rio, &res, sizeof(response_t));
                if (res.code == SUCCES)
                    printf("Fichier '%s' supprimé avec succès.\n", arg);
                else
                    printf("Erreur : impossible de supprimer '%s'.\n", arg);

            /* ---- put ---- */
            } else if (strcmp(cmd, "put") == 0 || strcmp(cmd, "PUT") == 0) {
                char local_path[8704];
                snprintf(local_path, sizeof(local_path), "%s%s", DIR_CLIENT, arg);
                FILE *fin = fopen(local_path, "rb");
                if (!fin) {
                    fprintf(stderr, "Impossible d'ouvrir '%s'\n", local_path);
                } else {
                    fseek(fin, 0, SEEK_END);
                    size_t file_size = (size_t)ftell(fin);
                    rewind(fin);
                    char *data = malloc(file_size);
                    fread(data, 1, file_size, fin);
                    fclose(fin);

                    req.type = PUT;
                    strncpy(req.nom, arg, 255);
                    req.nom[255] = '\0';
                    req.replicate = 1;

                    struct timeval start, end;
                    gettimeofday(&start, NULL);

                retry_put:
                    if (rio_writen(clientfd, &req, sizeof(request_t)) < 0) {
                        if (handle_reconnect(&clientfd, &rio) < 0) { free(data); goto fatal_error; }
                        goto retry_put;
                    }
                    Rio_writen(clientfd, &file_size, sizeof(size_t));
                    Rio_writen(clientfd, data, file_size);
                    free(data);

                    response_t res;
                    Rio_readnb(&rio, &res, sizeof(response_t));
                    gettimeofday(&end, NULL);
                    if (res.code == SUCCES) {
                        double elapsed = (end.tv_sec - start.tv_sec)
                                       + (end.tv_usec - start.tv_usec) / 1e6;
                        double kbps = (elapsed > 0) ? (file_size / 1024.0) / elapsed : 0;
                        printf("Transfer successfully complete.\n");
                        printf("%zu bytes sent in %.4f seconds (%.2f Kbytes/s).\n",
                               file_size, elapsed, kbps);
                    } else {
                        printf("Erreur lors de l'envoi de '%s'.\n", arg);
                    }
                }
            } else {
                printf("Commande non supportée : %s\n", cmd);
            }
        } else {
            printf("Usage : get/rm/put <nom_fichier> | ls | auth <login> <mdp> | bye\n");
        }
    }

    Close(clientfd);
    exit(0);

fatal_error:
    fprintf(stderr, "Erreur fatale : impossible de se reconnecter. Arrêt.\n");
    if (clientfd >= 0) Close(clientfd);
    exit(1);
}
