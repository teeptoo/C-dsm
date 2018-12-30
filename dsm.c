#include "dsm.h"

int DSM_NODE_NUM; /* nombre de processus dsm */
int DSM_NODE_ID;  /* rang (= numero) du processus */ 

dsm_proc_conn_t * dsm_conn_array = NULL; /* tableau contenant toutes les infos de connexion des processus dsm */

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


/* fonctions pouvant etre utiles */
static void dsm_change_info( int numpage, dsm_page_state_t state, dsm_page_owner_t owner) {
   if ((numpage >= 0) && (numpage < PAGE_NUMBER)) {	
	if (state != NO_CHANGE )
	table_page[numpage].status = state;
      if (owner >= 0 )
	table_page[numpage].owner = owner;
      return;
   }
   else {
	fprintf(stderr,"[%i] Invalid page number !\n", DSM_NODE_ID);
      return;
   }
}

static dsm_page_owner_t get_owner( int numpage)
{
   return table_page[numpage].owner;
}

static dsm_page_state_t get_status( int numpage)
{
   return table_page[numpage].status;
}

/* Allocation d'une nouvelle page */
static void dsm_alloc_page( int numpage )
{
   char *page_addr = num2address( numpage );
   mmap(page_addr, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
   return ;
}

/* Changement de la protection d'une page */
static void dsm_protect_page( int numpage , int prot)
{
   char *page_addr = num2address( numpage );
   mprotect(page_addr, PAGE_SIZE, prot);
   return;
}

static void dsm_free_page( int numpage )
{
   char *page_addr = num2address( numpage );
   munmap(page_addr, PAGE_SIZE);
   return;
}

static void *dsm_comm_daemon( void *arg)
{  
   while(1)
     {
	/* a modifier */
	printf("[%i] Waiting for incoming reqs \n", DSM_NODE_ID);
	sleep(2);
     }
   return;
}

static int dsm_send(int dest,void *buf,size_t size)
{
   /* a completer */
}

static int dsm_recv(int from,void *buf,size_t size)
{
   /* a completer */
}

static void dsm_handler( void )
{  
   /* A modifier */
   printf("[%i] FAULTY  ACCESS !!! \n",DSM_NODE_ID);
   abort();
}

/* traitant de signal adequat */
static void segv_handler(int sig, siginfo_t *info, void *context)
{
   /* A completer */
   /* adresse qui a provoque une erreur */
   void  *addr = info->si_addr;   
  /* Si ceci ne fonctionne pas, utiliser a la place :*/
  /*
   #ifdef __x86_64__
   void *addr = (void *)(context->uc_mcontext.gregs[REG_CR2]);
   #elif __i386__
   void *addr = (void *)(context->uc_mcontext.cr2);
   #else
   void  addr = info->si_addr;
   #endif
   */
   /*
   pour plus tard (question ++):
   dsm_access_t access  = (((ucontext_t *)context)->uc_mcontext.gregs[REG_ERR] & 2) ? WRITE_ACCESS : READ_ACCESS;   
  */   
   /* adresse de la page dont fait partie l'adresse qui a provoque la faute */
   void  *page_addr  = (void *)(((unsigned long) addr) & ~(PAGE_SIZE-1));

   if ((addr >= (void *)BASE_ADDR) && (addr < (void *)TOP_ADDR))
     {
	dsm_handler();
     }
   else
     {
	/* SIGSEGV normal : ne rien faire*/
     }
}

/* Seules ces deux dernieres fonctions sont visibles et utilisables */
/* dans les programmes utilisateurs de la DSM                       */
char *dsm_init(int argc, char **argv)
{   
   struct sigaction act;
   int index;

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
        printf("Tableau des infos connexion pour Proc %d :\n", DSM_NODE_ID);
        for (i = 0; i < DSM_NODE_NUM; ++i) {
            if (i != DSM_NODE_ID) {
                printf("\trang=%d\t port=%d\t hostname=%s\n", dsm_conn_array[i].rang,
                       dsm_conn_array[i].dist_port,
                       dsm_conn_array[i].dist_hostname);
            } // end if
        } // end for
    } // end if DEBUG

    /* fermeture socket d'initialisation */
    close(sock_init);

    /* initialisation des connexions */
   /* avec les autres processus : connect/accept */

   /* listen */
    if(-1 == listen(sock_dsm, DSM_NODE_ID)) { ERROR_EXIT("listen"); }

    /* accept de tous les rangs supérieurs */
    for (i = DSM_NODE_ID-1; i >= 0; --i) {
        dsm_conn_array[i].fd_dsm = do_accept(sock_dsm);
        if(DEBUG_PHASE2) { printf("Accept depuis %d réussi.\n", i); }
    }

   /* connect "num_procs-rang" fois */
   for (i = DSM_NODE_ID+1; i <= DSM_NODE_NUM-1; ++i) {
       /* création socket*/
       dsm_conn_array[i].fd_dsm = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
       if(-1 == dsm_conn_array[i].fd_dsm) { ERROR_EXIT("socket"); }

       /* remplissage sockaddr_in */
       memset(&sockaddr_dsm, 0, sizeof(struct sockaddr_in));
       sockaddr_dsm.sin_family = AF_INET;
       resolve_hostname(dsm_conn_array[i].dist_hostname, dsm_temp_ip);
       inet_aton(dsm_temp_ip, &sockaddr_dsm.sin_addr);
       sockaddr_dsm.sin_port = htons(dsm_conn_array[i].dist_port);

       /* connect */
       if(-1 == connect(dsm_conn_array[i].fd_dsm, (struct sockaddr *)&sockaddr_dsm, sizeof(struct sockaddr_in))) { ERROR_EXIT("connect"); }
       if(DEBUG_PHASE2) { printf("Connect vers Proc%d réussi.\n", i); }
   }
   
   /* Allocation des pages en tourniquet */
   for(index = 0; index < PAGE_NUMBER; index ++){	
     /*if ((index % DSM_NODE_NUM) == DSM_NODE_ID)
       dsm_alloc_page(index);	     
     dsm_change_info( index, WRITE, index % DSM_NODE_NUM);*/
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
    close(sock_dsm);

   /* free */
   free(dsm_conn_array);

   /* terminer correctement le thread de communication */
   /* pour le moment, on peut faire : */
   sleep(1);
   pthread_cancel(comm_daemon);
   
  return;
}

