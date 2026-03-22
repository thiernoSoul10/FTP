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
    char *nom;
};

request_t* init_request(typereq_t type, char *nom);

void setType(request_t *r, typereq_t t);
void setNom(request_t *r, char *nom);

typereq_t getType(request_t *r);
char* getNom(request_t *r);

response_t requestHandler(int connfd);

#endif