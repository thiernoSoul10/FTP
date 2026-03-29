#include "csapp.h"
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "request.h"
#include "filereader.h"

#define USERS_FILE "users.txt"

#define DIR_SERVER    "dirServer/"
#define NB_SLAVES     2
#define SLAVE_BASE_PORT 3131

// Port de cet esclave (initialisé par set_slave_port depuis ftpslave.c)
static int my_slave_port = 0;

void set_slave_port(int p) {
    my_slave_port = p;
}

/* ------------------------------------------------------------------ */
/*  Propagation vers les autres esclaves                               */
/* ------------------------------------------------------------------ */

static void replicate_rm(const char *nom) {
    for (int i = 0; i < NB_SLAVES; i++) {
        int port = SLAVE_BASE_PORT + i;
        if (port == my_slave_port) continue; // ne pas se propager à soi-même

        int fd = open_clientfd("127.0.0.1", port);
        if (fd < 0) { fprintf(stderr, "Replication rm: esclave %d injoignable\n", port); continue; }

        request_t req;
        memset(&req, 0, sizeof(req));
        req.type = RM;
        strncpy(req.nom, nom, 255);
        req.replicate = 0; // éviter la propagation infinie

        Rio_writen(fd, &req, sizeof(req));

        // Lire la réponse pour ne pas casser l'autre esclave avec SIGPIPE
        response_t dummy;
        rio_t r;
        Rio_readinitb(&r, fd);
        Rio_readnb(&r, &dummy, sizeof(response_t));
        Close(fd);
    }
}

static void replicate_put(const char *nom, const char *data, size_t size) {
    for (int i = 0; i < NB_SLAVES; i++) {
        int port = SLAVE_BASE_PORT + i;
        if (port == my_slave_port) continue;

        int fd = open_clientfd("127.0.0.1", port);
        if (fd < 0) { fprintf(stderr, "Replication put: esclave %d injoignable\n", port); continue; }

        request_t req;
        memset(&req, 0, sizeof(req));
        req.type = PUT;
        strncpy(req.nom, nom, 255);
        req.replicate = 0;

        Rio_writen(fd, &req, sizeof(req));
        Rio_writen(fd, &size, sizeof(size_t));
        Rio_writen(fd, (void *)data, size);

        // Lire la réponse
        response_t dummy;
        rio_t r;
        Rio_readinitb(&r, fd);
        Rio_readnb(&r, &dummy, sizeof(response_t));
        Close(fd);
    }
}

/* ------------------------------------------------------------------ */
/*  Handlers                                                           */
/* ------------------------------------------------------------------ */

// Liste le contenu de DIR_SERVER sans fork (pas de conflit SIGCHLD)
response_t ls_handler(int connfd) {
    response_t res;
    char buf[4096];
    size_t total = 0;

    DIR *dir = opendir(DIR_SERVER);
    if (dir == NULL) {
        res.code = ERREUR;
        Rio_writen(connfd, &res, sizeof(res));
        return res;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        size_t len = strlen(entry->d_name);
        if (total + len + 1 < sizeof(buf)) {
            memcpy(buf + total, entry->d_name, len);
            total += len;
            buf[total++] = '\n';
        }
    }
    closedir(dir);

    res.code = SUCCES;
    Rio_writen(connfd, &res, sizeof(res));
    Rio_writen(connfd, &total, sizeof(size_t));
    Rio_writen(connfd, buf, total);
    return res;
}

// Vérifie les credentials dans users.txt
response_t auth_handler(int connfd, request_t *req, int *authenticated) {
    response_t res;
    res.code = ERREUR;

    FILE *f = fopen(USERS_FILE, "r");
    if (f == NULL) {
        fprintf(stderr, "auth: impossible d'ouvrir %s\n", USERS_FILE);
        Rio_writen(connfd, &res, sizeof(res));
        return res;
    }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        char stored_login[256], stored_pass[256];
        if (sscanf(line, "%255[^:]:%255s", stored_login, stored_pass) == 2) {
            if (strcmp(req->nom, stored_login) == 0 &&
                strcmp(req->password, stored_pass) == 0) {
                *authenticated = 1;
                res.code = SUCCES;
                printf("auth: utilisateur '%s' authentifié\n", stored_login);
                break;
            }
        }
    }
    fclose(f);

    if (res.code != SUCCES)
        fprintf(stderr, "auth: échec pour l'utilisateur '%s'\n", req->nom);

    Rio_writen(connfd, &res, sizeof(res));
    return res;
}

// Supprime un fichier et propage si replicate=1
response_t rm_handler(int connfd, request_t *req, int authenticated) {
    response_t res;
    if (!authenticated) {
        fprintf(stderr, "rm: accès refusé — authentification requise\n");
        res.code = ERREUR;
        Rio_writen(connfd, &res, sizeof(res));
        return res;
    }
    char path[512];
    snprintf(path, sizeof(path), "%s%s", DIR_SERVER, req->nom);

    if (unlink(path) < 0) {
        fprintf(stderr, "rm: impossible de supprimer %s\n", path);
        res.code = ERREUR;
    } else {
        res.code = SUCCES;
        printf("rm: %s supprimé\n", path);
        if (req->replicate)
            replicate_rm(req->nom);
    }

    Rio_writen(connfd, &res, sizeof(res));
    return res;
}

// Reçoit un fichier depuis le client, le sauvegarde et propage si replicate=1
response_t put_handler(int connfd, rio_t *rp, request_t *req, int authenticated) {
    response_t res;
    if (!authenticated) {
        fprintf(stderr, "put: accès refusé — authentification requise\n");
        // On doit quand même lire les données pour ne pas bloquer le client
        size_t file_size;
        Rio_readnb(rp, &file_size, sizeof(size_t));
        char *data = malloc(file_size);
        if (data) { Rio_readnb(rp, data, file_size); free(data); }
        res.code = ERREUR;
        Rio_writen(connfd, &res, sizeof(res));
        return res;
    }
    size_t file_size;

    // 1. Recevoir la taille du fichier
    Rio_readnb(rp, &file_size, sizeof(size_t));

    // 2. Recevoir le contenu
    char *data = malloc(file_size);
    if (data == NULL) {
        res.code = ERREUR;
        Rio_writen(connfd, &res, sizeof(res));
        return res;
    }
    Rio_readnb(rp, data, file_size);

    // 3. Écrire sur le disque
    char path[512];
    snprintf(path, sizeof(path), "%s%s", DIR_SERVER, req->nom);

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        fprintf(stderr, "put: impossible d'ouvrir %s\n", path);
        res.code = ERREUR;
        free(data);
        Rio_writen(connfd, &res, sizeof(res));
        return res;
    }
    write(fd, data, file_size);
    close(fd);
    printf("put: %s reçu (%zu octets)\n", path, file_size);

    res.code = SUCCES;

    // 4. Propager aux autres esclaves
    if (req->replicate)
        replicate_put(req->nom, data, file_size);

    free(data);
    Rio_writen(connfd, &res, sizeof(res));
    return res;
}

/* ------------------------------------------------------------------ */
/*  Utilitaires request_t                                              */
/* ------------------------------------------------------------------ */

request_t* init_request(typereq_t type, char nom[256]){
    request_t *r = (request_t *) calloc(1, sizeof(request_t));
    if (!r) return NULL;
    r->type = type;
    if (nom) { strncpy(r->nom, nom, 255); r->nom[255] = '\0'; }
    return r;
}

void setType(request_t *r, typereq_t t){ if(r != NULL) r->type = t;}

void setNom(request_t *r, char *nom){
    if(r == NULL || nom == NULL) return;
    strncpy(r->nom, nom, 255);
    r->nom[255] = '\0';
}

typereq_t getType(request_t *r){ if(r != NULL) return r->type; return UNKNOWN; }
char* getNom(request_t *r){ if(r != NULL) return r->nom; return NULL; }

/* ------------------------------------------------------------------ */
/*  Dispatch principal                                                 */
/* ------------------------------------------------------------------ */

response_t requestHandler(int connfd){
    ssize_t n;
    rio_t rio;
    request_t req;
    response_t res;
    int authenticated = 0; // flag de session

    Rio_readinitb(&rio, connfd);

    while ((n = Rio_readnb(&rio, &req, sizeof(request_t))) > 0) {
        if (req.type == GET) {
            res = filereader(connfd, getNom(&req), req.bloc_debut);
        } else if (req.type == LS) {
            res = ls_handler(connfd);
        } else if (req.type == AUTH) {
            res = auth_handler(connfd, &req, &authenticated);
        } else if (req.type == RM) {
            res = rm_handler(connfd, &req, authenticated);
        } else if (req.type == PUT) {
            res = put_handler(connfd, &rio, &req, authenticated);
        } else if (req.type == HELLO) {
            // Handshake Q12 : l'esclave envoie son IP+port au maître
            typedef struct { char ip[16]; int port; } slave_info_t;
            slave_info_t info;
            struct sockaddr_in sa;
            socklen_t len = sizeof(sa);
            getsockname(connfd, (struct sockaddr *)&sa, &len);
            inet_ntop(AF_INET, &sa.sin_addr, info.ip, sizeof(info.ip));
            info.port = my_slave_port;
            res.code = SUCCES;
            Rio_writen(connfd, &res, sizeof(res));
            Rio_writen(connfd, &info, sizeof(info));
            break; // connexion de handshake terminée
        } else if (req.type == BYE) {
            res.code = SUCCES;
            break;
        } else {
            res.code = ERREUR;
        }
    }

    return res;
}