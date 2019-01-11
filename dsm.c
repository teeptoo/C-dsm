#include "dsm.h"

int DSM_NODE_NUM; /* nombre de processus dsm */
int DSM_NODE_ID;  /* rang (= numero) du processus */

dsm_proc_conn_t * dsm_conn_array = NULL; /* tableau contenant toutes les infos de connexion des processus dsm */
volatile int dsm_state = RUNNING;

void copy_dsm_conn(dsm_proc_conn_t * dest, dsm_proc_conn_t * src) {
    dest->rang = src->rang;
    strcpy(dest->dist_hostname, src->dist_hostname);
    dest->dist_port = src->dist_port;
}

/* indique l'adresse de debut de la page de numero numpage */
static char *num2address( int numpage ) {
    char *pointer = (char *)(BASE_ADDR+(numpage*(PAGE_SIZE)));

    if( pointer >= (char *)TOP_ADDR ){
        fprintf(stderr,"[%i] Invalid address !\n", DSM_NODE_ID);
        return NULL;
    }
    else return pointer;
}

int address2num(char * addr) {
    return (((long int)(addr-BASE_ADDR))/(PAGE_SIZE));
}

static void dsm_change_info( int numpage, dsm_page_state_t state, dsm_page_owner_t owner) {
    if ((numpage >= 0) && (numpage < PAGE_NUMBER)) {
        if (state != NO_CHANGE )
            table_page[numpage].status = state;
        if (owner >= 0 )
            table_page[numpage].owner = owner;
    }
    else {
        fprintf(stderr,"Invalid page number !\n");
    }
}

static dsm_page_owner_t get_owner( int numpage)
{
    return table_page[numpage].owner;
}

/* Allocation d'une nouvelle page */
static void dsm_alloc_page( int numpage )
{
    char *page_addr = num2address( numpage );
    mmap(page_addr, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return ;
}

static void dsm_free_page( int numpage )
{
    char *page_addr = num2address( numpage );
    munmap(page_addr, PAGE_SIZE);
}

static void dsm_recv_page(int fd) {
    int asked_page;
    char * buffer = malloc(PAGE_SIZE);

    asked_page = recv_int(fd);
    dsm_alloc_page(asked_page);

    recv_line(fd, buffer);
    memcpy(num2address(asked_page), buffer, PAGE_SIZE);
    dsm_change_info(asked_page, WRITE, DSM_NODE_ID);

    free(buffer);
}

static void dsm_recv_info(int fd) {
    int page;
    int new_owner;

    page = recv_int(fd);
    new_owner = recv_int(fd);

    dsm_change_info(page, WRITE, new_owner);
}

static void dsm_send_info(int fd, int num_page, int owner) {
    send_int(fd, SEND_INFO);
    send_int(fd, num_page);
    send_int(fd, owner);
}

static void dsm_send_page(int dest_proc_num, int page) {
    if(DEBUG_PHASE2) {
        printf("%sEnvoi de la page %i à %i.\n%s",
               COLOR_MAGENTA,
               page,
               dest_proc_num,
               COLOR_RESET);
        fflush(stdout);
    }

    void * buffer = malloc(PAGE_SIZE);
    int index;

    send_int(dsm_conn_array[dest_proc_num].fd_dsm, SEND_PAGE);
    send_int(dsm_conn_array[dest_proc_num].fd_dsm, page);
    memcpy(buffer, num2address(page), PAGE_SIZE);

    send_line(dsm_conn_array[dest_proc_num].fd_dsm, buffer);
    free(buffer);

    dsm_free_page(page);
    for(index = 0; index < PAGE_NUMBER; index ++)
        if (((index % DSM_NODE_NUM) != DSM_NODE_ID) && ((index % DSM_NODE_NUM) != dest_proc_num))
            dsm_send_info(dsm_conn_array[index].fd_dsm, page, dest_proc_num);
}

static void dsm_recv_page_request(int request_sender_id) {
    int wanted_page;
    wanted_page = recv_int(dsm_conn_array[request_sender_id].fd_dsm);
    dsm_send_page(request_sender_id, wanted_page);
}


static void dsm_send_request(int owner_wanted_page, int wanted_page) {
    if(DEBUG_PHASE2) {
        printf("%sEnvoi d'une demande pour la page %i à %i.\n%s",
               COLOR_MAGENTA,
               wanted_page,
               owner_wanted_page,
               COLOR_RESET);
        fflush(stdout);
    }

    send_int(dsm_conn_array[owner_wanted_page].fd_dsm, PAGE_REQUEST);
    send_int(dsm_conn_array[owner_wanted_page].fd_dsm, wanted_page);
}

static void *dsm_comm_daemon( void *arg)
{
    int i;
    int index;
    int ret_poll;
    int request_type = -1;
    struct pollfd poll_sockets[DSM_NODE_NUM];
    memset(poll_sockets, 0, sizeof(struct pollfd)*DSM_NODE_NUM);

    for (i = 0; i < DSM_NODE_NUM; i++) {
        poll_sockets[i].events = POLLIN;
        if (dsm_conn_array[i].rang == DSM_NODE_ID)
            poll_sockets[i].fd = -1; // notre propre rang
        else
            poll_sockets[i].fd = dsm_conn_array[i].fd_dsm;
    }

    while (1) {
        do {
            ret_poll = poll(poll_sockets, (nfds_t)DSM_NODE_NUM, 500);
        } while ((ret_poll == -1) && (errno == EINTR));

        if(ret_poll>0) {
            for (i = 0; i < DSM_NODE_NUM+1; ++i) {
                if(poll_sockets[i].revents & POLLIN) {
                    request_type = recv_int(poll_sockets[i].fd);
                    switch(request_type) {
                        case SEND_PAGE: // je reçois une page
                            if(DEBUG_PHASE2) {
                                printf("%sReception d'une page venant de %i.\n%s", COLOR_MAGENTA, i, COLOR_RESET);
                                fflush(stdout);
                            }
                            dsm_recv_page(poll_sockets[i].fd);
                            break;
                        case SEND_INFO: // je reçois de nouvelles infos
                            if(DEBUG_PHASE2) {
                                printf("%sReception de nouvelles infos de propriétaire envoyées par %i.\n%s", COLOR_MAGENTA, i, COLOR_RESET);
                                fflush(stdout);
                            }
                            dsm_recv_info(poll_sockets[i].fd);
                            break;
                        case PAGE_REQUEST: // je reçois une demande pour envoyer une page
                            if(DEBUG_PHASE2) {
                                printf("%sReception d'une demande de page par %i.\n%s", COLOR_MAGENTA, i, COLOR_RESET);
                                fflush(stdout);
                            }
                            dsm_recv_page_request(i);
                            break;
                    }

                } else if(poll_sockets[i].revents & POLLHUP) {
                    // on bloque ses pages
                    for(index = 0; index < PAGE_NUMBER; index ++){
                        if ((index % DSM_NODE_NUM) == i)
                            dsm_change_info(index, INVALID, index % DSM_NODE_NUM);
                    }
                    // on retire l'éoute sur la socket
                    poll_sockets[i].fd = -1;
                }
            }
        } else if (-1 == ret_poll) { ERROR_EXIT("poll"); }
    }
    return NULL;
}

static void dsm_handler( int faulty_num_page )
{
    int done=0;
    dsm_send_request(get_owner(faulty_num_page), faulty_num_page);

    while(done == 0) { // TODO remplacer par une condition logique
        if(get_owner(faulty_num_page) != DSM_NODE_ID)
            done = 1;
    }
}

/* traitant de signal adequat */
static void segv_handler(int sig, siginfo_t *info, void *context)
{

    /* adresse qui a provoque une erreur */
    void  *addr = info->si_addr;
    /*#ifdef __x86_64__
        void *addr = (void *)(context->uc_mcontext.gregs[REG_CR2]);
    #elif __i386__
        void *addr = (void *)(context->uc_mcontext.cr2);
    #else
       void  addr = info->si_addr;
    #endif */

    /* adresse de la page dont fait partie l'adresse qui a provoque la faute */
    void  *page_addr  = (void *)(((unsigned long) addr) & ~(PAGE_SIZE-1));

    if ((page_addr >= (void *)BASE_ADDR) && (page_addr < (void *)TOP_ADDR))
    {
        if(DEBUG_PHASE2) {

            printf("Accès faulty sur l'adresse \n");
            fflush(stdout);
        }
        dsm_handler(address2num(page_addr));
        sleep(5);
    }
    else
    {
        fprintf(stderr, "Segmentation fault.\n");
        fflush(stderr);

        ERROR_EXIT("SIGSEGV");

    }
}

/* Seules ces deux dernieres fonctions sont visibles et utilisables */
/* dans les programmes utilisateurs de la DSM                       */
char *dsm_init(int argc, char **argv)
{
    struct sigaction act;
    int index;
    int ret_connect;
    struct sockaddr_in sockaddr_dsm;
    char dsm_temp_ip[IP_LENGTH];
    int i;
    int sock_init = 3; // hérité des descripteurs de fichiers de dsmwrap
    int sock_dsm = 4; // hérité des descripteurs de fichiers de dsmwrap
    dsm_proc_conn_t dsm_conn_temp;

    /* reception du nombre de processus dsm envoye */
    /* par le lanceur de programmes (DSM_NODE_NUM)*/
    DSM_NODE_NUM = recv_int(sock_init);

    /* reception de mon numero de processus dsm envoye */
    /* par le lanceur de programmes (DSM_NODE_ID)*/
    DSM_NODE_ID = recv_int(sock_init);

    /* reception des informations de connexion des autres */
    /* processus envoyees par le lanceur : */
    /* nom de machine, numero de port, etc. */
    dsm_conn_array = malloc(sizeof(dsm_proc_conn_t) * DSM_NODE_NUM);
    memset(dsm_conn_array, 0, sizeof(dsm_proc_conn_t) * DSM_NODE_NUM);
    for (i = 0; i < DSM_NODE_NUM; ++i) { // on ne reçoit pas nos propres infos
        if(i != DSM_NODE_ID) {
            memset(&dsm_conn_temp, 0, sizeof(dsm_proc_conn_t));
            recv_dsm_infos(sock_init, &dsm_conn_temp);
            copy_dsm_conn(&dsm_conn_array[i], &dsm_conn_temp);
        }
    }

    if(DEBUG_PHASE1) {
        printf("%s{dsm_init} Tableau des infos connexion pour Proc %d :\n", COLOR_MAGENTA, DSM_NODE_ID);
        for (i = 0; i < DSM_NODE_NUM; ++i) {
            if (i != DSM_NODE_ID) {
                printf("\trang=%d\t port=%d\t hostname=%s\n", dsm_conn_array[i].rang,
                       dsm_conn_array[i].dist_port,
                       dsm_conn_array[i].dist_hostname);
            } // end if
        } // end for
        printf("%s", COLOR_RESET);
        fflush(stdout);
    } // end if DEBUG

    /* fermeture socket d'initialisation */
    close(sock_init);

    /* initialisation des connexions */
    /* avec les autres processus : connect/accept */
    /* listen */
    if (-1 == listen(sock_dsm, DSM_NODE_ID)) {ERROR_EXIT("listen"); }

    /* accept de tous les rangs inférieurs */
    for (i = DSM_NODE_ID - 1; i >= 0; --i) {
        dsm_conn_array[i].fd_dsm = do_accept(sock_dsm);
        if (DEBUG_PHASE2) {

            printf("%s{dsm_init} Accept depuis %d réussi.\n%s", COLOR_MAGENTA, i, COLOR_RESET);
            fflush(stdout);
        }
    }

    /* connect "num_procs-rang" fois */
    for (i = DSM_NODE_ID + 1; i <= DSM_NODE_NUM - 1; ++i) {
        /* création socket*/
        dsm_conn_array[i].fd_dsm = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (-1 == dsm_conn_array[i].fd_dsm) {ERROR_EXIT("socket"); }

        /* remplissage sockaddr_in */
        memset(&sockaddr_dsm, 0, sizeof(struct sockaddr_in));
        sockaddr_dsm.sin_family = AF_INET;
        resolve_hostname(dsm_conn_array[i].dist_hostname, dsm_temp_ip);
        inet_aton(dsm_temp_ip, &sockaddr_dsm.sin_addr);
        sockaddr_dsm.sin_port = htons(dsm_conn_array[i].dist_port);

        /* connect */
        do{
            ret_connect = connect(dsm_conn_array[i].fd_dsm,
                                  (struct sockaddr *) &sockaddr_dsm,
                                  sizeof(struct sockaddr_in));
        } while((-1 == ret_connect) && (errno == ECONNREFUSED));
        if(-1 == ret_connect) { ERROR_EXIT("connect"); }


        if (DEBUG_PHASE2) {
            printf("%s{dsm_init} Connect vers Proc%d réussi.\n%s", COLOR_MAGENTA, i, COLOR_RESET);
            fflush(stdout);

        }
    }

    /* Allocation des pages en tourniquet */
    for(index = 0; index < PAGE_NUMBER; index ++){
        if ((index % DSM_NODE_NUM) == DSM_NODE_ID)
            dsm_alloc_page(index);
        dsm_change_info( index, WRITE, index % DSM_NODE_NUM);
    }

    /* mise en place du traitant de SIGSEGV */
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = segv_handler;
    sigaction(SIGSEGV, &act, NULL);

    /* creation du thread de communication */
    /* ce thread va attendre et traiter les requetes */
    /* des autres processus */
    pthread_create(&comm_daemon, NULL, dsm_comm_daemon, NULL);

    /* Adresse de début de la zone de mémoire partagée */
    return ((char *)BASE_ADDR);
}

void dsm_finalize( void )
{
    int i;
    int sock_dsm = 4; // hérité depuis dsmwrap

    /* fermer proprement les connexions avec les autres processus */
    for (i = 0; i < DSM_NODE_NUM; ++i) {
        if(i != DSM_NODE_ID)
            close(dsm_conn_array[i].fd_dsm);
    }
    if(-1 == close(sock_dsm)) { ERROR_EXIT("close(sock_dsm)"); }

    /* free */
    free(dsm_conn_array);

    /* terminer correctement le thread de communication */
    /* pour le moment, on peut faire : */
    sleep(1);
    dsm_state = QUITTING;
    pthread_join(comm_daemon, NULL);

    printf("FIN");
    fflush(stdout);
}
