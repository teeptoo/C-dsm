#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <sys/mman.h>
#include <limits.h>

#define ERROR_EXIT(str) {perror(str);exit(EXIT_FAILURE);}

/* ************************************************************ */
/* Paramètres de DEBUG : mettre à 1 pour afficher les messages  */
/* de débug correspondants.                                     */
/* ************************************************************ */

#define DEBUG_PHASE1 0
#define DEBUG_PHASE2 1

/* ************************************************************ */
/* Constantes de longueurs                                      */
/* ************************************************************ */

#define IP_LENGTH 16 // IP V4
#define HOSTNAME_MAX_LENGTH 255 // norme POSIX
#define INT_MAX_LENGTH 6 // port maximal est 65535 (16bit non signé)
#define BUFFER_LENGTH 512
#define FILE_NAME_MAX_LENGTH 255 // sur la plupart des systèmes de fichiers

/* ************************************************************ */
/* Couleurs pour affichage dans le terminal                     */
/* ************************************************************ */

#define COLOR_RED       "\x1b[31m"
#define COLOR_GREEN     "\x1b[32m"
#define COLOR_YELLOW    "\x1b[33m"
#define COLOR_BLUE      "\x1b[34m"
#define COLOR_MAGENTA   "\x1b[35m"
#define COLOR_CYAN      "\x1b[36m"
#define COLOR_RESET     "\x1b[0m"

/* ************************************************************ */
/* Définition des infos de connexion des processus dsm          */
/* ************************************************************ */

struct dsm_proc_conn {
    int rang;
    int fd_dsm;
    char dist_hostname[HOSTNAME_MAX_LENGTH];
    int dist_port;
};
typedef struct dsm_proc_conn dsm_proc_conn_t;

/* ************************************************************ */
/* Définition du type des infos d'identification des processus  */
/* dsm                                                          */
/* ************************************************************ */

struct dsm_proc {
    int fd_init;
    pid_t pid;
    dsm_proc_conn_t connect_info;
};
typedef struct dsm_proc dsm_proc_t;

/* ************************************************************ */
/* Prototypes                                                   */
/* ************************************************************ */

// Création d'une socket + bind sur un port aléatoire qui est inscrit dans (int) port_num
int creer_socket(int *port_num);

// Ajout du flag CLOEXEC sur un descripteur de fichier (permet la fermeture automatique lors d'un exec)
void set_cloexec_flag(int desc);

// Créé et remplit une structure sockaddr_in sur un port aléatoire avec allocation de mémoire
struct sockaddr_in *creer_sockaddr_in_ecoute();

// Accepte une connexion et renvoie le nouveau descripteur de fichier associé à cette connexion
int do_accept(int sock);

// Cherche l'IP correspondante au hostname donné et l'inscrit dans char ip[]
void resolve_hostname(char * hostname , char* ip);

// Envoi d'un entier via socket
void send_int(int file_des, int to_send);

// Réception d'un entier via socket
int recv_int(int file_des);

// Envoi d'une ligne via socket avec vérification de la longueur
void send_line(int file_des, const void *str);

// Réception d'une ligne via socket avec vérification de la longueur
void recv_line(int file_des, void *str);

// Envoi d'une structure de type dsm_proc_conn via socket
void send_dsm_infos(int fd, dsm_proc_conn_t proc_conn);

// Réception d'une structure de type dsm_proc_conn
void recv_dsm_infos(int fd, dsm_proc_conn_t * proc_conn);