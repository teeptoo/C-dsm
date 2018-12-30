#include "common_impl.h"

int main(int argc, char **argv)
{
    /* ************************************************************ */
    /* Déclarations                                                 */
    /* ************************************************************ */

    struct sockaddr_in sockaddr_dsmexec; // pour la socket d'initialisation
    pid_t pid = getpid();
    int i;
    int rang;
    int sock_init;
    int sock_dsm;
    int dsmexec_port;
    int dsmwrap_port;
    char dsmexec_ip[IP_LENGTH];
    char dsmexec_hostname[HOSTNAME_MAX_LENGTH];
    char * args_exec[argc-2];

    /* ************************************************************ */
    /* Traitement des arguments utiles à dsmwrap                    */
    /* ************************************************************ */

    strcpy(dsmexec_hostname, argv[1]);
    dsmexec_port = atoi(argv[2]);
    rang = atoi(argv[3]);

    /* ************************************************************ */
    /* Traitement des arguments pour l'executable final             */
    /* ************************************************************ */

    memset(args_exec, 0, sizeof(char *) * (argc - 2));
    args_exec[0] = argv[4];
    for (i = 0; i < (argc-5); ++i)
        args_exec[i+1] = argv[i+5];
    args_exec[i+2] = NULL;

    /* ************************************************************ */
    /* Remplissage de sockaddr_dsmexec                              */
    /* ************************************************************ */

    memset(&sockaddr_dsmexec, 0, sizeof(struct sockaddr_in));
    resolve_hostname(dsmexec_hostname, dsmexec_ip);
    sockaddr_dsmexec.sin_family = AF_INET;
    inet_aton(dsmexec_ip, &sockaddr_dsmexec.sin_addr);
    sockaddr_dsmexec.sin_port = htons(dsmexec_port);

    /* ************************************************************ */
    /* Création de la socket d'initialisation                       */
    /* ************************************************************ */

    sock_init = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(-1 == connect(sock_init, (struct sockaddr *)&sockaddr_dsmexec, sizeof(struct sockaddr_in))) { ERROR_EXIT("connect"); }

    /* ************************************************************ */
    /* Envoi de notre rang au lanceur pour reconaissance            */
    /* ************************************************************ */

    send_int(sock_init, rang);

    /* ************************************************************ */
    /* Envoi du pid au lanceur                                      */
    /* ************************************************************ */

    send_int(sock_init, pid);

    /* ************************************************************ */
    /* Creation de la socket d'ecoute pour les connexions avec les  */
    /* autres processus dsm                                         */
    /* ************************************************************ */

    sock_dsm = creer_socket(&dsmwrap_port);

    /* ************************************************************ */
    /* Envoi du numero de port au lanceur                           */
    /* ************************************************************ */

    send_int(sock_init, dsmwrap_port);

    if(DEBUG_PHASE1) {
        printf("[dsm|wrapper] Lancement executable=%s (args=", argv[4]);
        for (i = 0; i < (argc-5); ++i)
            printf("%s,", args_exec[i+1]);
        printf(").\n");
    }

    // Réception signal synchro (appel bloquant)
    recv_int(sock_init);

    /* ************************************************************ */
    /* Executable final                                             */
    /* ************************************************************ */
    fflush(stdout);
    execvp(argv[4], args_exec);

    /* ************************************************************ */
    /* Si problème execvp : nettoyage + erreur                      */
    /* ************************************************************ */

    close(sock_init);
    close(sock_dsm);
    ERROR_EXIT("execvp");
}
