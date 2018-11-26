#include "common_impl.h"
#include <netdb.h>

int main(int argc, char **argv)
{

  int domaine = AF_INET;
  int type= SOCK_STREAM;
  int protocol= IPPROTO_TCP;
  struct sockaddr_in serv_addr;
  int addr_len = sizeof(serv_addr);
  memset(& serv_addr,'\0',sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(atoi(argv[2]));

  int s;
  inet_aton(argv[1],&serv_addr.sin_addr);


  char *arg_truc[10];
  arg_truc[0] = "~/prog_rsys/PR204/Phase1/bin/truc";
  sprintf(arg_truc[1],"%s",argv[3]);
  arg_truc[2] = NULL;
  /* processus intermediaire pour "nettoyer" */
  /* la liste des arguments qu'on va passer */

  /* a la commande a executer vraiment */

  /* creation d'une socket pour se connecter au */
  s = socket(domaine,type,protocol);
  int  serv = connect(s,(struct sockaddr *)&serv_addr,addr_len);
  if (serv != 0) { perror("echec de connexion");}
   else
  printf("connexion avec la machine %s acceptée\n",argv[3]);
  fflush(stdout);
  execv("~/truc",arg_truc);

   /* au lanceur et envoyer/recevoir les infos */
   /* necessaires pour la phase dsm_init */

   /* Envoi du nom de machine au lanceur */

   /* Envoi du pid au lanceur */

   /* Creation de la socket d'ecoute pour les */
   /* connexions avec les autres processus dsm */

   /* Envoi du numero de port au lanceur */
   /* pour qu'il le propage à tous les autres */
   /* processus dsm */

   /* on execute la bonne commande */
   return 0;
}
