#include "common_impl.h"
int creer_socket(int prop, int *port_num)
{
  int fd = 0;
  int domaine = AF_INET;
  int type= SOCK_STREAM;
  int protocol= IPPROTO_TCP;
  SOCKADDR_IN sin = { 0 };
  sin.sin_addr.s_addr = htonl(INADDR_ANY);
  sin.sin_family = AF_INET;
  sin.sin_port = htons(*port_num);


  /* fonction de creation et d'attachement */
  /* d'une nouvelle socket */
  int s = socket(AF_INET,SOCK_STREAM,protocol);
  if (s == -1) { perror("socket:");
  int b =  bind (s, (SOCKADDR *) &sin, sizeof sin) ;
  if (b == -1) { perror("bind:");
  //   int liu = listen(s,20);
  //   if (liu==-1) {perror("listen");

  /* renvoie le numero de descripteur */
  /* et modifie le parametre port_num */
  s  = fd;
  return fd;
}

/* Vous pouvez ecrire ici toutes les fonctions */
/* qui pourraient etre utilisees par le lanceur */
/* et le processus intermediaire. N'oubliez pas */
/* de declarer le prototype de ces nouvelles */
/* fonctions dans common_impl.h */
