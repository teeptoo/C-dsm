#include "common_impl.h"


int creer_socket(int prop, int *port_num)
{
  int fd = 0;
  int domaine = AF_INET;
  int type= SOCK_STREAM;
  int protocol= IPPROTO_TCP;
  struct sockaddr_in *serv_addr = malloc(sizeof(struct sockaddr_in));
  struct sockaddr_in my_addr;
  int len = sizeof(struct sockaddr_in);
  serv_addr->sin_family = AF_INET;
  serv_addr->sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr->sin_port = htons(*port_num);
  memset(serv_addr,'\0',sizeof(struct sockaddr_in));
  /* fonction de creation et d'attachement */
  /* d'une nouvelle socket */
  int s = socket(domaine,type,protocol);
  if (s == -1) { perror("socket:");}
  int b =   bind(s, (struct sockaddr *) serv_addr,sizeof(struct sockaddr_in)); ;
  if (b == -1) { perror("bind:");}
  len = sizeof(my_addr);
  getsockname(s, (struct sockaddr *) &my_addr, &len);
  *port_num = ntohs(my_addr.sin_port);

  /* renvoie le numero de descripteur */
  /* et modifie le parametre port_num */
  fd  = s;
  return fd;
}
void ip(char *IP)
{
  char s[256];
  if (!gethostname(s, sizeof s))
  {  {
    struct hostent *host= gethostbyname(s);
    if (host  != NULL)
    {
  struct in_addr **adr;
  for (adr = (struct in_addr **)host->h_addr_list; *adr; adr++)
  {
  sprintf( IP,"%s", inet_ntoa(**adr));
  }  }}}}

  int send_message(int sock,char *buffer,int length){  // permet d'écrire sur la socket
  int a = send(sock,buffer,length,0);
  if (a==-1){
    perror("erreur de send");
  }
  return a;
  }
  int do_accept(int sock,struct sockaddr *addr,socklen_t* addrlen){ // accepte une connection
    int accepted = accept(sock,addr,addrlen);
    if(accepted==-1){
      perror("Error while accecountpting client");
      exit(EXIT_FAILURE);
    }}




    

/* Vous pouvez ecrire ici toutes les fonctions */
/* qui pourraient etre utilisees par le lanceur */
/* et le processus intermediaire. N'oubliez pas */
/* de declarer le prototype de ces nouvelles */
/* fonctions dans common_impl.h */
