# Projet de Système et Réseaux : Serveur de fichiers FTP

L'objectif de ce projet est de construire un serveur de fichiers inspiré du protocole FTP. Il intègre une gestion avec plusieurs serveurs pour répartir le travail, traiter de multiples clients et gérer les pannes éventuelles.

---

## 🛠 Compilation

Le projet utilise un outil automatique (le `Makefile`) pour compiler tous les fichiers.

```bash
# Pour effacer les anciens fichiers générés
make clean

# Pour recompiler tout le projet
make
```

Une fois compilé, vous obtenez 3 programmes exécutables principaux :
- `ftpmaster` : Le serveur principal qui répartit les clients.
- `ftpslave` : Le serveur secondaire qui stocke vraiment les fichiers et répond aux commandes.
- `ftpclient` : Le programme pour l'utilisateur.

---

## 🚀 Étape de Lancement

Pour tester l'ensemble du système correctement, il faut ouvrir **4 terminaux différents** et lancer les programmes dans **cet ordre précis** (car le serveur principal a besoin de voir les serveurs secondaires avant d'accepter des utilisateurs) :

### 1. Démarrer les Serveurs Esclaves (Secondaires)
Ouvrez deux terminaux et lancez les serveurs esclaves sur les ports statiques prévus (3131 et 3132).

**Terminal 1 (Esclave 1)** :
```bash
./ftpslave 3131
```

**Terminal 2 (Esclave 2)** :
```bash
./ftpslave 3132
```

### 2. Démarrer le Serveur Maître (Principal)
Une fois les deux serveurs esclaves lancés, démarrez le serveur maître. Il va s'assurer que les esclaves sont là.

**Terminal 3 (Serveur Maître)** :
```bash
./ftpmaster
```

### 3. Démarrer le Client
Enfin, lancez l'application client en lui indiquant l'adresse locale du serveur principal (`127.0.0.1`).

**Terminal 4 (Client)** :
```bash
./ftpclient 127.0.0.1
```

---

## 🧪 Scénarios de Test étape par étape

Une fois dans le terminal du client, vous pouvez tester les fonctionnalités demandées dans le TP :

### Test 1 : Lecture simple (Sans mot de passe)
Demandez à voir les fichiers disponibles sur le serveur, ou téléchargez-en un.
```bash
ls                  # Affiche les fichiers disponibles sur le serveur (dans le dossier dirServer)
get fichier.txt     # Récupère le fichier et l'enregistre sur l'ordinateur (dans le dossier dirClient)
```

### Test 2 : Authentification et Modification
Les commandes qui modifient le serveur (`put` pour envoyer, `rm` pour effacer) sont protégées. Les identifiants autorisés (login et mot de passe) sont listés dans le fichier `users.txt`.

```bash
# 1. Tester sans s'authentifier (doit afficher une erreur)
rm secret.txt

# 2. S'authentifier auprès du serveur
auth alice password

# 3. Tester après s'être authentifié (doit fonctionner)
put nouveau_fichier.txt   # Envoie le fichier depuis le client vers le serveur
rm secret.txt             # Efface le fichier du serveur
```
*Si vous ouvrez un deuxième client et faites `ls`, vous verrez que les ajouts et suppressions ont bien été répercutés automatiquement sur tous les serveurs esclaves !*

### Test 3 : Gestion d'une panne du serveur (Question Bonus)
Pour vérifier que le client gère la perte d'un serveur :
1. Sur le client, tapez un simple `ls`.
2. Allez dans le Terminal du serveur esclave sur lequel le client discutait et **arrêtez-le** (avec `Ctrl+C`).
3. Retournez sur le client et tapez une autre commande (exemple `ls`).
4. Le client va vous informer qu'il y a eu une panne, se reconnecter tout seul au serveur maître, obtenir l'adresse de l'autre serveur esclave encore en vie, puis vous demandera de retaper votre commande.
5. Retapez la commande, elle s'exécutera normalement sur l'esclave survivant.

### Test 4 : Reprise de téléchargement (Panne du client)
1. Commencez à télécharger un gros fichier avec `get gros_fichier.bin`.
2. Fermez brusquement le client en plein milieu du téléchargement avec `Ctrl+C`.
3. Relancez le client (`./ftpclient 127.0.0.1`) puis retapez `get gros_fichier.bin`.
4. Le transfert reprendra exactement là où il a été coupé (sans tout retélécharger depuis le début).
