#include "common_impl.h"

// Création d'une socket + bind sur un port aléatoire qui est inscrit dans (int) port_num
int creer_socket(int *port_num) {
    struct sockaddr_in * sock_addr = NULL;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    int sock;

    // Création de la socket
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == -1) { ERROR_EXIT("socket"); }

    // Remplissage de sockaddr_in
    sock_addr = creer_sockaddr_in_ecoute(); // port au choix du système

    // Bind de la socket
    if (-1 == bind(sock, (struct sockaddr *)sock_addr, addrlen)) { ERROR_EXIT("bind"); }

    // Récupération du port utilisé
    if(-1 == getsockname(sock, (struct sockaddr *)sock_addr, &addrlen)) { ERROR_EXIT("getsockname"); }
    *port_num = ntohs(sock_addr->sin_port);
    free(sock_addr);

    return sock;
}

// Ajout du flag CLOEXEC sur un descripteur de fichier (permet la fermeture automatique lors d'un exec)
// inspiré de la documentation GNU (https://www.gnu.org/software/libc/manual/html_node/Descriptor-Flags.html)
void set_cloexec_flag(int desc) {
    int oldflags = fcntl(desc, F_GETFD, 0);

    // si erreur dans la lecture des flags
    if (oldflags < 0)
        ERROR_EXIT("fcntl (F_GETFD)");

    // sinon rajout du flag CLOEXEC
    oldflags |= FD_CLOEXEC;
    if(-1 == fcntl (desc, F_SETFD, oldflags)) { ERROR_EXIT("fcntl (F_SETFD)"); }
}

// Créé et remplit une structure sockaddr_in sur un port aléatoire avec allocation de mémoire
struct sockaddr_in *creer_sockaddr_in_ecoute() {
    struct sockaddr_in * sock_addr = NULL;
    sock_addr = malloc(sizeof(struct sockaddr_in));
    memset(sock_addr, 0, sizeof(struct sockaddr_in));

    sock_addr->sin_family = AF_INET;
    sock_addr->sin_port = 0; // choix aléatoire
    sock_addr->sin_addr.s_addr = INADDR_ANY;

    return sock_addr;
}

// Accepte une connexion et renvoie le nouveau descripteur de fichier associé à cette connexion
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

// Cherche l'IP correspondante au hostname donné et l'inscrit dans char ip[]
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

// Envoi d'un entier via socket
// inspiré de la réponse de garlix (https://stackoverflow.com/questions/9140409/transfer-integer-over-a-socket-in-c)
void send_int(int file_des, int to_send) {
    int32_t converted_to_send = (int)to_send; // conversion de l'entier dans un format de longueur générique
    char * data = (char *)&converted_to_send; // pointeur sur les données restantes à envoyer
    int left = sizeof(converted_to_send);
    int sent = 0;

    do {
        sent = write(file_des, data, left);
        if((sent == -1) && (errno != EAGAIN) && (errno != EINTR)) { ERROR_EXIT("send"); }
        else {
            data += sent;
            left -= sent;
        }
    } while(left > 0);
}

// Réception d'un entier via socket
int recv_int(int file_des) {
    int32_t received; // données reçues
    int left=sizeof(received);
    char *data_received = (char *)&received; // pointeur sur les données
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

// Envoi d'une ligne via socket avec vérification de la longueur
void send_line(int file_des, const void *str) {
    int str_length = strlen(str); // taille de la chaine à envoyer
    send_int(file_des, str_length); // on transmet la taille

    // Transmission de la chaine
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

// Réception d'une ligne via socket avec vérification de la longueur
void recv_line(int file_des, void *str) {
    // Reception de la taille de la chaine
    int left = recv_int(file_des);
    // Reception de la chaine
    int read_count = 0;
    do {
        read_count = read(file_des, str, left); // écriture directe dans le buffer
        if((read_count == -1) && (errno != EAGAIN) && (errno != EINTR)) { ERROR_EXIT("read"); }
        else {
            str += read_count;
            left -= read_count;
        }
    } while (left>0);
}

// Envoi d'une structure de type dsm_proc_conn via socket
void send_dsm_infos(int fd, dsm_proc_conn_t proc_conn) {
    send_int(fd, proc_conn.rang);
    send_line(fd, proc_conn.dist_hostname);
    send_int(fd, proc_conn.dist_port);
}

// Réception d'une structure de type dsm_proc_conn
void recv_dsm_infos(int fd, dsm_proc_conn_t * proc_conn) {
    proc_conn->rang = recv_int(fd);
    recv_line(fd, proc_conn->dist_hostname);
    proc_conn->dist_port = recv_int(fd);
}

