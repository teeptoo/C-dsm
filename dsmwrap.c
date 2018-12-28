#include "common_impl.h"

void check_localhost(char * dsmexec_hostname) {
    /* vérifie si on est en local, si c'est le cas, dsmexec_hostname */
    /* remplacé par localhost pour coincider avec le machine_file */
    char mon_hostname[HOSTNAME_MAX_LENGTH];
    gethostname(mon_hostname, HOSTNAME_MAX_LENGTH);
    if(0 == strcmp(mon_hostname, dsmexec_hostname))
        strcpy(dsmexec_hostname, "localhost");
}

int main(int argc, char **argv)
{
    /* processus intermediaire pour "nettoyer" */
    /* la liste des arguments qu'on va passer */
    /* a la commande a executer vraiment */

    /* déclarations */
    struct sockaddr_in sockaddr_dsmexec; // pour la socket d'initialisation
    pid_t pid = getpid();
    int i;
    int sock_init;
    int sock_dsm;
    int dsmexec_port;
    int dsmwrap_port;
    char dsmexec_ip[IP_LENGTH];
    char dsmwrap_hostname[HOSTNAME_MAX_LENGTH];
    char * args_exec[argc-2];

    /* traitement des arguments pour dsmwrap */
    strcpy(dsmwrap_hostname, argv[1]);
    dsmexec_port = atoi(argv[2]);

    /* traitement des arguments pour executable final */
    memset(args_exec, 0, sizeof(char *) * (argc - 2));
    args_exec[0] = argv[3];
    for (i = 0; i < (argc-4); ++i)
        args_exec[i+1] = argv[i+4];
    args_exec[i+2] = NULL;

    /* remplissage de sockaddr_dsmexec */
    memset(&sockaddr_dsmexec, 0, sizeof(struct sockaddr_in));
    resolve_hostname(dsmwrap_hostname, dsmexec_ip);
    check_localhost(dsmwrap_hostname);
    sockaddr_dsmexec.sin_family = AF_INET;
    inet_aton(dsmexec_ip, &sockaddr_dsmexec.sin_addr);
    sockaddr_dsmexec.sin_port = htons(dsmexec_port);

    /* creation d'une socket pour se connecter au */
    /* au lanceur et envoyer/recevoir les infos */
    /* necessaires pour la phase dsm_init */
    sock_init = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(-1 == connect(sock_init, (struct sockaddr *)&sockaddr_dsmexec, sizeof(struct sockaddr_in))) { ERROR_EXIT("connect"); }

    /* Envoi du nom de machine au lanceur */
    send_line(sock_init, dsmwrap_hostname);

    /* Envoi du pid au lanceur */
    send_int(sock_init, pid);

    /* Creation de la socket d'ecoute pour les */
    /* connexions avec les autres processus dsm */
    sock_dsm = creer_socket(&dsmwrap_port);

    /* Envoi du numero de port au lanceur */
    /* pour qu'il le propage à tous les autres */
    /* processus dsm */
    send_int(sock_init, dsmwrap_port);

    if(DEBUG) {
        printf("[dsm|wrapper] Lancement executable=%s (args=", argv[3]);
        for (i = 0; i < (argc-4); ++i)
            printf("%s,", argv[i+4]);
        printf(").\n");
        fflush(stdout);
    }

    /* réception signal synchro */
    read_int(sock_init);
    /* on execute la bonne commande */
    execvp(argv[3], args_exec);

    /* si problème execvp : nettoyage + erreur */
    close(sock_init);
    close(sock_dsm);
    ERROR_EXIT("execvp");
}
