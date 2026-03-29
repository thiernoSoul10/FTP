#ifndef _FILE_READER_
#define _FILE_READER_
#include "response.h"

response_t filereader(int connfd, char fichier[256], size_t bloc_debut);

#endif