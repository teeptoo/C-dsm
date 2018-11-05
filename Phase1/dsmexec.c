#include "common_impl.h"

/* variables globales */

/* un tableau gerant les infos d'identification */
/* des processus dsm */
dsm_proc_t *proc_array = NULL;

/* le nombre de processus effectivement crees */
volatile int num_procs_creat = 0;

void usage(void)
{
  fprintf(stdout,"Usage : dsmexec machine_file executable arg1 arg2 ...\n");
  fflush(stdout);
  exit(EXIT_FAILURE);
}

void sigchld_handler(int sig)
{
   /* on traite les fils qui se terminent */
   /* pour eviter les zombies */
}

int count_lines(FILE * file) {
  /* on compte le nombre de lignes */
  /* pour connaitre le nombre de machines */
  char temp_char;
  int i=0;
  for(temp_char=getc(file); temp_char!= EOF; temp_char=getc(file))
    if(temp_char == '\n')
      i++;
  return i;
}

void read_machine_file(FILE * file, char * machines[], int num_procs) {
  // déclarations
  int i;
  char chaine[255];
  // mallocs
  for (i = 0; i < num_procs; i++)
    machines[i] = malloc(sizeof(char) * 255);
  // lecture fichier
  for (i = 0; i < num_procs; i++) {
    fgets(chaine, 255, file);
    chaine[strlen(chaine)-1] = '\0'; // enlever le \n
    strcpy(machines[i], chaine);
  }
}

int main(int argc, char *argv[])
{
  if (argc < 3){
    usage();
  } else {
    /* déclarations */
     pid_t pid;
     int num_procs = 0;
     int i;
     FILE * machine_file;

     /* Mise en place d'un traitant pour recuperer les fils zombies*/
     /* XXX.sa_handler = sigchld_handler; */

     /* lecture du fichier de machines */
     machine_file = fopen("machine_file", "r");
     if(NULL == machine_file) { ERROR_EXIT("foepn:"); }
     /* 1- on recupere le nombre de processus a lancer */
     num_procs = count_lines(machine_file);
     /* 2- on recupere les noms des machines : le nom de */
     /* la machine est un des elements d'identification */
     char * machines[num_procs];
     if(-1 == fseek(machine_file, 0, SEEK_SET)) { ERROR_EXIT("fseek:"); }
     read_machine_file(machine_file, machines, num_procs);
     for ( i = 0; i<num_procs;i++)
   	{
   		printf("%s",machines[i]);
   	}
     fclose(machine_file);

     /* creation de la socket d'ecoute */
     /* + ecoute effective */

     /* creation des fils */
     for(i = 0; i < num_procs ; i++) {

	/* creation du tube pour rediriger stdout */

	/* creation du tube pour rediriger stderr */

//	pid = fork();
//	if(pid == -1) ERROR_EXIT("fork");

	if (pid == 0) { /* fils */

	   /* redirection stdout */

	   /* redirection stderr */

	   /* Creation du tableau d'arguments pour le ssh */

	   /* jump to new prog : */
	   /* execvp("ssh",newargv); */

	} else  if(pid > 0) { /* pere */
	   /* fermeture des extremites des tubes non utiles */
	   num_procs_creat++;
	}
     }


     for(i = 0; i < num_procs ; i++){

	/* on accepte les connexions des processus dsm */

	/*  On recupere le nom de la machine distante */
	/* 1- d'abord la taille de la chaine */
	/* 2- puis la chaine elle-meme */

	/* On recupere le pid du processus distant  */

	/* On recupere le numero de port de la socket */
	/* d'ecoute des processus distants */
     }

     /* envoi du nombre de processus aux processus dsm*/

     /* envoi des rangs aux processus dsm */

     /* envoi des infos de connexion aux processus */

     /* gestion des E/S : on recupere les caracteres */
     /* sur les tubes de redirection de stdout/stderr */
     /* while(1)
         {
            je recupere les infos sur les tubes de redirection
            jusqu'à ce qu'ils soient inactifs (ie fermes par les
            processus dsm ecrivains de l'autre cote ...)

         };
      */

     /* on attend les processus fils */

     /* on ferme les descripteurs proprement */

     /* on ferme la socket d'ecoute */
  }
   exit(EXIT_SUCCESS);
}
