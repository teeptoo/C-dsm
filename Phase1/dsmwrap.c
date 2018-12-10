#include "common_impl.h"
#include <netdb.h>

int main(int argc, char **argv)
{
  int pid = getpid();
  int domaine = AF_INET;
  int type= SOCK_STREAM;
  int protocol= IPPROTO_TCP;
  struct sockaddr_in serv_addr;
  int addr_len = sizeof(serv_addr);
  memset(& serv_addr,'\0',sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(atoi(argv[2]));
  int s_ecoute;
  int s;
  int port = 0;
  int rang;
  int machine_name_size= strlen(argv[3]);
//  int val = 100;
   //char * val = malloc(255);
  inet_aton(argv[1],&serv_addr.sin_addr);

/*  char *arg_truc[10];
  arg_truc[0] = "~/PR204-master/Phase1/bin/truc";
  sprintf(arg_truc[1],"%s",argv[3]);
  arg_truc[2] = NULL;*/
  /* processus intermediaire pour "nettoyer" */
  /* la liste des arguments qu'on va passer */

  /* a la commande a executer vraiment */


  /* creation d'une socket pour se connecter au */
  s = socket(domaine,type,protocol);

  s_ecoute = creer_socket(&port);
  int  serv = connect(s,(struct sockaddr *)&serv_addr,addr_len);
  if (serv != 0) { perror("echec de connexion");}
   else


 recv(s,&rang,sizeof(int),0);
  //execv("~/truc",arg_truc);

   /* au lanceur et envoyer/recevoir les infos */

   /* necessaires pour la phase dsm_init */

   /* Envoi du nom de machine au lanceur */

      send(s,&machine_name_size,sizeof(int),0);

     send_message(s,argv[3],machine_name_size);
   /* Envoi du pid au lanceur */

        send(s,&pid,sizeof(int),0);
        send(s,&port,sizeof(int),0);
        send_message(s,IP_machine(),255);

   /* Creation de la socket d'ecoute pour les */

   /* connexions avec les autres processus dsm */

   /* Envoi du numero de port au lanceur */
   /* pour qu'il le propage à tous les autres */
   /* processus dsm */
  // s_ecoute = creer_socket_ssh(0,&port);
// char * IP = malloc(255);
  //Ip(IP);

  printf("[Proc : %d] : connexion avec la machine %s acceptée \n",rang,argv[3]);
  fflush(stdout);

  while(1){}
   /* on execute la bonne commande */
   return 0;
}
