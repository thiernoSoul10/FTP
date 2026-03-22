#include "csapp.h"

#include "request.h"
#include "filereader.h"

request_t* init_request(typereq_t type, char *nom){
    request_t *r = (request_t *) malloc(sizeof(request_t));

    r->type = type;
    r->nom = strdup(nom);

    return r;
}

void setType(request_t *r, typereq_t t){ if(r != NULL) r->type = t;}
void setNom(request_t *r, char *nom){
    if(r == NULL || nom == NULL) return;
    free(r->nom);    
    r->nom = strdup(nom);
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
    size_t n;
    char buf[MAXLINE];
    rio_t rio;
    char cmd[5], arg[256];
    response_t res;

    Rio_readinitb(&rio, connfd);

    if ((n = Rio_readlineb(&rio, buf, MAXLINE)) <= 0){
        res.code = ERREUR;
        return res;
    }

    /* enlever le \n */
    buf[strcspn(buf, "\n")] = '\0';

    sscanf(buf, "%s %s", cmd, arg);

    if(strcmp(cmd, "GET") == 0)
        res = filereader(connfd, arg);
    else if(strcmp(cmd, "PUT"))
        res.code = ERREUR;
    else if(strcmp(cmd, "LS"))
        res.code = ERREUR;

    return res;
}