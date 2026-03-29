#ifndef _SLAVE_ADDR_
#define _SLAVE_ADDR_

// Q13 : infos que le maitre envoie au client pour qu'il se connecte a l'esclave
typedef struct {
    char ip[64]; // IP de l'esclave
    int port;    // port de l'esclave
} slave_addr_t;

#endif
