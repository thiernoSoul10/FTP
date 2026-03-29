#include "csapp.h"

#include "request.h"
#include "filereader.h"

request_t* init_request(typereq_t type, char nom[256]){
    request_t *r = (request_t *) calloc(1, sizeof(request_t));
    if (!r) return NULL;

    r->type = type;
    if (nom) {
        strncpy(r->nom, nom, 255);
        r->nom[255] = '\0';
    }

    return r;
}

void setType(request_t *r, typereq_t t){ if(r != NULL) r->type = t;}

void setNom(request_t *r, char *nom){
    if(r == NULL || nom == NULL) return;
    strncpy(r->nom, nom, 255);
    r->nom[255] = '\0';
}

typereq_t getType(request_t *r){ 
    if(r != NULL) return r->type;
    return UNKNOWN;
}

char* getNom(request_t *r){ 
    if(r != NULL) return r->nom;
    return NULL;
}

response_t requestHandler(int connfd){
    ssize_t n;
    rio_t rio;
    request_t req;
    response_t res;

    Rio_readinitb(&rio, connfd);

    // boucle jusqu'à BYE ou déconnexion
    while ((n = Rio_readnb(&rio, &req, sizeof(request_t))) > 0) {
        if (req.type == GET) {
            // Q10 : on passe l'offset pour que le serveur reprenne au bon bloc si besoin
            res = filereader(connfd, getNom(&req), req.offset);
        } else if (req.type == LS) {
            // Q15 : lister le contenu du repertoire serveur
            res = filels(connfd);
        } else if (req.type == BYE) {
            res.code = SUCCES;
            break;
        } else {
            res.code = ERREUR;
        }
    }

    return res;
}