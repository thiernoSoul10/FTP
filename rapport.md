# Rapport — Projet FTP

## 1. Architecture générale

Le système comporte trois entités : un **client** (`ftpclient`), un **maître** (`ftpmaster`) et un ou plusieurs **esclaves** (`ftpslave`).

```
ftpclient
    |
    |-- (1) connect port 2121 --> ftpmaster
    |<-- slave_addr_t (ip+port) --
    |-- close --
    |
    |-- (2) connect port 2122 --> ftpslave
    |-- request_t (GET/LS/BYE) ->
    |<-- response_t + données ---
    |-- close --
```

Le maître ne fait que rediriger : il reçoit un client, lui envoie l'adresse d'un esclave disponible, puis ferme la connexion. Le client se connecte ensuite directement à l'esclave pour le vrai travail.

Les esclaves utilisent un pool de 10 processus pré-forkés. Chaque fils tourne en boucle et appelle `Accept()` pour prendre en charge les clients.

---

## 2. Protocole

Tous les échanges utilisent des structures binaires transmises avec `Rio_writen` / `Rio_readnb`.

**request_t** (client → esclave)
```
| typereq_t type | char nom[256] | size_t offset |
```
- `type` : GET, LS ou BYE
- `nom` : nom du fichier (GET uniquement)
- `offset` : blocs déjà reçus, 0 si nouveau téléchargement (Q10)

**response_t** (esclave → client)
```
| codeRetour code |   (SUCCES ou ERREUR)
```

**slave_addr_t** (maître → client)
```
| char ip[64] | int port |   (port=0 si aucun esclave dispo)
```

### Séquence GET complète

```
client                      esclave
  |-- request_t (GET, offset=0) -->|
  |<-- response_t (SUCCES) --------|
  |<-- size_t nb_blocs ------------|
  |<-- bloc[0..nb_blocs] (512B) ---|  x nb_blocs
```

### Séquence LS

```
client                      esclave
  |-- request_t (LS) ------------>|
  |<-- response_t (SUCCES) --------|
  |<-- size_t n -------------------|
  |<-- char buf[n] ----------------|   (noms séparés par \n)
```

---

## 3. Reprise sur crash (Q10)

Après chaque bloc reçu, le client écrit un fichier `.nom.prog` dans `dirClient/` contenant le nombre de blocs reçus (`size_t`). Au relancement, il lit cet offset et l'envoie dans la requête. Le serveur fait `lseek(fd, offset * 512, SEEK_SET)` pour sauter les blocs déjà envoyés. Le fichier `.prog` est supprimé quand le transfert est fini.

---

## 4. Détection de panne (Q14)

Le maître vérifie si un esclave répond avant de l'assigner, via une tentative de connexion TCP (`open_clientfd` minuscule, retourne -1 sans `exit`). Si l'esclave tombe pendant un transfert, le client détecte l'erreur via `rio_writen` (retourne -1), se reconnecte au maître, obtient un nouvel esclave, et renvoie la même requête avec l'offset courant.

`SIGPIPE` est ignoré (`SIG_IGN`) dans le client et les fils esclaves pour ne pas mourir sur une connexion coupée.

---

## 5. Compilation et test

### Compilation

```bash
make
```

Produit : `ftpclient`, `ftpmaster`, `ftpslave`.

### Test étape 1 — serveur simple (sans maître)

```bash
# Terminal 1
./ftpserveri

# Terminal 2
./ftpclient localhost
get lorem.txt
bye
```

### Test étape 3 — architecture maître/esclave

```bash
# Terminal 1 — lancer l'esclave
./ftpslave 2122

# Terminal 2 — lancer le maître (lui passer l'adresse de l'esclave)
./ftpmaster localhost 2122

# Terminal 3 — client
./ftpclient localhost
get lorem.txt
ls
bye
```

### Test reprise sur crash (Q10)

```bash
# Lancer le client, démarrer un get d'un gros fichier
./ftpclient localhost
get gros.bin
# Pendant le transfert, faire Ctrl+C
# Relancer le client : le transfert reprend depuis le dernier bloc
./ftpclient localhost
get gros.bin
```

### Ports utilisés

| Processus | Port |
|---|---|
| ftpmaster | 2121 |
| ftpslave  | 2122 (modifiable via argv) |
