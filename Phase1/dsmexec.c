#include "common_impl.h"

/* variables globales */
#define HOSTNAME_MAX_LENGTH 255 // norme POSIX
#define BUFFER_LENGTH 256
#define SSH_ARGS_MAX_COUNT 20

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
  int status;
  do {
    waitpid(-1, &status, WNOHANG);
    if(-1 == status) { ERROR_EXIT("waitpid:"); }
  } while(status != 0);
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
  char chaine[HOSTNAME_MAX_LENGTH];
  // mallocs
  for (i = 0; i < num_procs; i++)
  machines[i] = malloc(sizeof(char) * HOSTNAME_MAX_LENGTH);
  // lecture fichier
  for (i = 0; i < num_procs; i++) {
    fgets(chaine, HOSTNAME_MAX_LENGTH, file);
    chaine[strlen(chaine)-1] = '\0'; // enlever le \n
    strcpy(machines[i], chaine);
  }
}

void free_machines(char * machines[], int num_procs) {
  int i;
  for (i = 0; i < num_procs; i++)
	  free(machines[i]);
  free(machines);
}

void free_tubes(int ** tubes, int num_procs) {
	int i;
	for (i = 0; i < num_procs; i++)
		  free(tubes[i]);
	free(tubes);
}

int main(int argc, char *argv[])
{
  if (argc < 3)
	  usage();
  else {
    /* déclarations pile */
    pid_t pid;
    struct sigaction sigchld_sigaction;
    int num_procs = 0;
    int i, j;
    int read_count;
    int ret_pol;
    char *arg_ssh[SSH_ARGS_MAX_COUNT];

    FILE *  machine_file = fopen("machine_file", "r");
    if(NULL == machine_file) { ERROR_EXIT("fopen:"); }
    num_procs = count_lines(machine_file);

    /* déclarations pour tas */
    char ** machines = NULL;
    char * buffer = NULL;
    char * buffer_cursor = NULL;
    int ** tubes_stdout = NULL;
    int ** tubes_stderr = NULL;
    struct pollfd * poll_tubes = NULL;

    /* mallocs */
    machines = malloc(sizeof(char *) * num_procs);
    buffer = malloc(BUFFER_LENGTH);

    tubes_stdout = malloc(sizeof(int *)*num_procs);
    for (i = 0; i < num_procs; i++)
      tubes_stdout[i] = malloc(sizeof(int)*2);

    tubes_stderr = malloc(sizeof(int *)*num_procs);
    for (i = 0; i < num_procs; i++)
      tubes_stderr[i] = malloc(sizeof(int)*2);

    poll_tubes = malloc(sizeof(struct pollfd)*num_procs*2);

    /* Mise en place d'un traitant pour recuperer les fils zombies*/
    memset(&sigchld_sigaction, 0, sizeof(struct sigaction));
    sigchld_sigaction.sa_handler = sigchld_handler;
    if (-1 == sigaction(SIGCHLD, &sigchld_sigaction, NULL)) { ERROR_EXIT("sigaction:"); }

    /* lecture du fichier de machines */
    /* 1- on recupere le nombre de processus a lancer */
    /* 2- on recupere les noms des machines : le nom de */
    /* la machine est un des elements d'identification */
    if(-1 == fseek(machine_file, 0, SEEK_SET)) { ERROR_EXIT("fseek:"); }
    read_machine_file(machine_file, machines, num_procs);
    fclose(machine_file);

    /* creation de la socket d'ecoute */
    /* + ecoute effective */


    /* creation des fils */
    for(i = 0; i < num_procs ; i++) {
      /* creation du tube pour rediriger stdout */
      if(-1 == pipe(tubes_stdout[i])) { ERROR_EXIT("pipe:"); }

      /* creation du tube pour rediriger stderr */
      if(-1 == pipe(tubes_stderr[i])) { ERROR_EXIT("pipe:"); }
      pid = fork();
      if(pid == -1) ERROR_EXIT("fork");
      if (pid == 0) { /* fils */
        /* redirection stdout */
        close(STDOUT_FILENO);
        close(tubes_stdout[i][0]);
        dup(tubes_stdout[i][1]);

        /* fermetures stdout des autres processus */
        for(j = 0; j < i ; j++) {
        	close(tubes_stdout[j][0]);
        	close(tubes_stdout[j][1]);
        }

        /* redirection stderr */
        close(STDERR_FILENO);
        close(tubes_stderr[i][0]);
        dup(tubes_stderr[i][1]);

        /* fermetures stderr des autres processus */
        for(j = 0; j < i ; j++) {
        	close(tubes_stdout[j][0]);
        	close(tubes_stdout[j][1]);
        }

        /* Creation du tableau d'arguments pour le ssh */
        memset(arg_ssh, 0, SSH_ARGS_MAX_COUNT*sizeof(char *));
        arg_ssh[0] = "echo";
        arg_ssh[1] = machines[i]; // execution machine
        arg_ssh[2] = "~/echo";
        arg_ssh[3] = machines[i]; // arg for echo programm
        arg_ssh[4] = NULL;

        /* jump to new prog : */
        execvp("ssh", arg_ssh);
        break;
      } else  if(pid > 0) { /* pere */
        num_procs_creat++;
        /* fermeture des extremites des tubes non utiles */
        close(tubes_stdout[i][1]);
        close(tubes_stderr[i][1]);
      } // end else (père)
    } // end for créations processus
    for(i = 0; i < num_procs ; i++){
      /* on accepte les connexions des processus dsm */

      /*  On recupere le nom de la machine distante */
      /* 1- d'abord la taille de la chaine */
      /* 2- puis la chaine elle-meme */

      /* On renum_procscupere le pid du processus distant  */

      /* On recupere le numero de port de la socket */
      /* d'ecoute des processus distants */
    }

    /* envoi du nombre de processus aux processus free_machinesdsm*/

    /* envoi des rangs aux processus dsm */

    /* envoi des infos de connexion aux processus */

    /* gestion des E/S : on recupere les caracteres */
    /* sur les tubes de redirection de stdout/stderr */
    memset(poll_tubes, 0, sizeof(struct pollfd)*num_procs*2);
    for (i = 0; i < num_procs; i++) {
      poll_tubes[2*i].fd = tubes_stdout[i][0];
      poll_tubes[2*i+1].fd = tubes_stderr[i][0];
      poll_tubes[2*i].events = POLLIN;
      poll_tubes[2*i+1].events = POLLIN;
      poll_tubes[2*i].revents = 0;
      poll_tubes[2*i+1].revents = 0;
    }

    while(1)
    {
      /* je recupere les infos sur les tubes de redirection
      jusqu'à ce qu'ils soient inactifs (ie fermes par les
      processus dsm ecrivains de l'autre cote ...) */
      ret_pol = poll(poll_tubes, 2*num_procs, 100);
      if (ret_pol > 0) {
        for (i = 0; i < 2*num_procs; i++) {
          if (poll_tubes[i].revents & POLLIN) {
            memset(buffer, 0, BUFFER_LENGTH);
            buffer_cursor = buffer;
            read_count = 0;
            do {
              read_count = read(poll_tubes[i].fd, buffer_cursor, BUFFER_LENGTH);
              buffer_cursor += read_count;
            } while ((read_count == -1) && ((errno == EAGAIN) || (errno == EINTR)));
            if((read_count == -1) && (errno != EAGAIN) && (errno != EINTR)){ ERROR_EXIT("read"); }

            printf("[Sortie fd %i, machine %s] %s", i, machines[i/num_procs], buffer);
            fflush(stdout);
          }
          else if(poll_tubes[i].revents & POLLHUP) {
            poll_tubes[i].fd = -1; // on retire le tube du poll
          }
        }
      }
      else if (-1 == ret_pol)
        ERROR_EXIT("poll");
    };

    /* on attend les processus fils */

    /* on ferme les descripteurs proprement */

    /* on ferme la socket d'ecoute */
    free(buffer);
    free_machines(machines, num_procs);
    free_tubes(tubes_stdout, num_procs);
    free_tubes(tubes_stderr, num_procs);
    free(poll_tubes);
  }
  exit(EXIT_SUCCESS);
}
