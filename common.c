#include "common_impl.h"

int creer_socket(int *port_num) {
    /* fonction de creation et d'attachement */
    /* d'une nouvelle socket */
    /* renvoie le numero de descripteur */
    /* et modifie le parametre port_num */
    struct sockaddr_in * sock_addr = NULL;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    int sock;
    // Création de la socket
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == -1) { ERROR_EXIT("socket"); }
    // Remplissage de sockaddr_in
    sock_addr = creer_sockaddr_in(0); // port au choix du système
    // Bind de la socket
    if (-1 == bind(sock, (struct sockaddr *)sock_addr, addrlen)) { ERROR_EXIT("bind"); }
    // Récupération du port utilisé
    if(-1 == getsockname(sock, (struct sockaddr *)sock_addr, &addrlen)) { ERROR_EXIT("getsockname"); }
    *port_num = ntohs(sock_addr->sin_port);
    free(sock_addr);
    return sock;
}

void set_cloexec_flag(int desc) {
    // inspiré de la documentation GNU (https://www.gnu.org/software/libc/manual/html_node/Descriptor-Flags.html)
    int oldflags = fcntl(desc, F_GETFD, 0);
    // si erreur dans la lecture des flags
    if (oldflags < 0)
        ERROR_EXIT("fcntl (F_GETFD)");
    // sinon rajout du flag CLOEXEC
    oldflags |= FD_CLOEXEC;
    if(-1 == fcntl (desc, F_SETFD, oldflags)) { ERROR_EXIT("fcntl (F_SETFD)"); }
}

struct sockaddr_in *creer_sockaddr_in(int port) {
    struct sockaddr_in * sock_addr = NULL;
    sock_addr = malloc(sizeof(struct sockaddr_in));
    memset(sock_addr, 0, sizeof(struct sockaddr_in));
    sock_addr->sin_family = AF_INET;
    if(0 != port)
        sock_addr->sin_port = htons(port);
    else
        sock_addr->sin_port = 0; // choix aléatoire
    sock_addr->sin_addr.s_addr = INADDR_ANY;
    return sock_addr;
}

int do_accept(int sock) {
    struct sockaddr_in sock_temp;
    socklen_t addrlen_temp = sizeof(struct sockaddr_in);
    int fd;
    do {
        fd = accept(sock, (struct sockaddr *)&sock_temp, &addrlen_temp);
    } while((-1 == fd) && (errno == EINTR));
    if(-1 == fd) { ERROR_EXIT("accept"); }
    return fd;
}

void resolve_hostname(char * hostname , char* ip) {
    struct hostent *host;
    struct in_addr **addr_list;
    int i;
    host = gethostbyname(hostname);
    if(NULL == host) { ERROR_EXIT("gethostbyname"); }
    addr_list = (struct in_addr **) host->h_addr_list;
    for(i = 0; addr_list[i] != NULL; i++)
        strcpy(ip , inet_ntoa(*addr_list[i]) );
}

void send_int(int file_des, int to_send) {
    // inspiré de la réponse de garlix (https://stackoverflow.com/questions/9140409/transfer-integer-over-a-socket-in-c)
    int32_t converted_to_send = (int)to_send; // convert the length in a generic type independant from infrastucture for emission over socket
    char * data = (char *)&converted_to_send; // pointer on the remaining converted data to send
    int left = sizeof(converted_to_send);
    int sent = 0;
    do { // sending the length of coming string, based on the size of int32_t (pseudo-protocol)
        sent = write(file_des, data, left);
        if((sent == -1) && (errno != EAGAIN) && (errno != EINTR)) { ERROR_EXIT("send"); }
        else {
            data += sent;
            left -= sent;
        }
    } while(left > 0);
}

int read_int(int file_des) {
    // inspiré de la réponse de garlix (https://stackoverflow.com/questions/9140409/transfer-integer-over-a-socket-in-c)
    int32_t received; // received raw data
    int left=sizeof(received);
    char *data_received = (char *)&received; // pointer on raw data
    int read_count = 0;
    do {
        read_count = read(file_des, data_received, left);
        if((read_count == -1) && (errno != EAGAIN) && (errno != EINTR)) { ERROR_EXIT("read"); }
        else {
            data_received += read_count;
            left -= read_count;
        }
    } while (left>0);
    return (int)received;
}

void read_line(int file_des, void *str) {
    // Reading the length of the string to receive
    int left = read_int(file_des);
    // Receive the string
    int read_count = 0;
    do {
        read_count = read(file_des, str, left); // writing directly into the buffer
        if((read_count == -1) && (errno != EAGAIN) && (errno != EINTR)) { ERROR_EXIT("read"); }
        else {
            str += read_count;
            left -= read_count;
        }
    } while (left>0);
}

void send_line(int file_des, const void *str) {
    // Sending the length of the string
    int str_length = strlen(str); // getting the legnth
    send_int(file_des, str_length);
    // Sending the string
    int left = str_length;
    int read_count = 0;
    do {
        read_count = write(file_des, str, left);
        if((read_count == -1) && (errno != EAGAIN) && (errno != EINTR)) { ERROR_EXIT("send"); }
        else {
            str += read_count;
            left -= read_count;
        }
    } while(left > 0);
}


/* Vous pouvez ecrire ici toutes les fonctions */
/* qui pourraient etre utilisees par le lanceur */
/* et le processus intermediaire. N'oubliez pas */
/* de declarer le prototype de ces nouvelles */
/* fonctions dans common_impl.h */
