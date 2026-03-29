#ifndef _FILE_READER_
#define _FILE_READER_
#include "response.h"

// Q10 : offset = nombre de blocs déjà reçus (0 = téléchargement complet depuis le début)
response_t filereader(int connfd, char fichier[256], size_t offset);

// Q15 : envoie le contenu du repertoire serveur au client
response_t filels(int connfd);

#endif