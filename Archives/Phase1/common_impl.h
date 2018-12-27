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

#define DEBUG 1

#define ERROR_EXIT(str) {perror(str);exit(EXIT_FAILURE);}

#define IP_LENGTH 16
#define HOSTNAME_MAX_LENGTH 255 // norme POSIX
#define PORT_MAX_LENGTH 6 // port maximal est 65535 (16bit non signé)
#define BUFFER_LENGTH 512
#define SSH_ARGS_MAX_COUNT 20

/* definition du type des infos */
/* de connexion des processus dsm */
struct dsm_proc_conn  {
    int conn_fd;
    char dist_hostname[HOSTNAME_MAX_LENGTH];
    int dist_port;
};
typedef struct dsm_proc_conn dsm_proc_conn_t;

/* definition du type des infos */
/* d'identification des processus dsm */
struct dsm_proc {
    int rank;
    pid_t pid;
    dsm_proc_conn_t connect_info;
};
typedef struct dsm_proc dsm_proc_t;

int creer_socket(int *port_num);
struct sockaddr_in *creer_sockaddr_in(int port);
int do_accept(int sock);
void resolve_hostname(char * hostname , char* ip);
void send_int(int file_des, int to_send);
int read_int(int file_des);
void read_line(int file_des, void *str);
void send_line(int file_des, const void *str);
