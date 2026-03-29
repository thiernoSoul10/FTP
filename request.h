#ifndef _REQUEST_
#define _REQUEST_

#include <stdlib.h>
#include <string.h>

#include "typereq.h"
#include "response.h"

typedef  struct request request_t;

struct request{
    /* data */
    typereq_t type;
    char nom[256];       // login ou nom de fichier
    char password[256];  // mot de passe (AUTH uniquement)
    size_t bloc_debut;
    int replicate; // 1 = propager aux autres esclaves, 0 = local seulement
};

request_t* init_request(typereq_t type, char nom[256]);

void setType(request_t *r, typereq_t t);
void setNom(request_t *r, char *nom);

typereq_t getType(request_t *r);
char* getNom(request_t *r);

response_t requestHandler(int connfd);
void set_slave_port(int port);
response_t rm_handler(int connfd, request_t *req, int authenticated);
response_t put_handler(int connfd, rio_t *rp, request_t *req, int authenticated);
response_t auth_handler(int connfd, request_t *req, int *authenticated);

#endif