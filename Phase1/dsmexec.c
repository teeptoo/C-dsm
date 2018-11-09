#include "common_impl.h"

/* variables globales */
#define HOSTNAME_MAX_LENGTH 255 // norme POSIX
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
  // déclarations// ATTENTION bien modifier les numéros des descripteurs en dessous
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
}

int main(int argc, char *argv[])
{
  if (argc < 3)
  usage();
  else {
    /* déclarations */
    pid_t pid;
    struct sigaction sigchld_sigaction;
    int num_procs = 0;

    FILE *  machine_file = fopen("machine_file", "r");
    if(NULL == machine_file) { ERROR_EXIT("fopen:"); }
    num_procs = count_lines(machine_file);

    char * machines[num_procs]; // TODO enlever tableaux dynamiques (à la fin)
    char *arg_ssh[SSH_ARGS_MAX_COUNT]; // nombre arbitraire
    char * buffer;
    char * buffer_cursor;
    int tubes_stdout[num_procs][2];
    int tubes_stderr[num_procs][2];
    struct pollfd poll_tubes[num_procs*2];
    int i;
    int read_count;
    int ret_pol;

    buffer = malloc(256);

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
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        /* redirection stdout */
        close(tubes_stdout[i][0]);
        dup(tubes_stdout[i][1]);
        /* redirection stderr */
        close(tubes_stderr[i][0]);
        dup(tubes_stderr[i][1]);

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
    for (i = 0; i < 2*num_procs; i++) {
      poll_tubes[i].fd = tubes_stdout[i][0];
      poll_tubes[i].events = POLLIN;
      poll_tubes[i].revents = 0;
      poll_tubes[num_procs+i].fd = tubes_stderr[i][0];
      poll_tubes[num_procs+i].events = POLLIN;
      poll_tubes[num_procs+i].revents = 0;
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
            memset(buffer, 0, 256);
            buffer_cursor = buffer;
            do {
              if((read_count == -1) && (errno != EAGAIN) && (errno != EINTR)){ ERROR_EXIT("read"); }
              else
                buffer_cursor += read_count;
            } while ((read_count=read(poll_tubes[i].fd, buffer_cursor, 256)) != 0);
            printf("[Sortie fd %i, machine %s] %s\n", i, machines[i], buffer);
            fflush(stdout);
          }
          if(poll_tubes[i].revents & POLLHUP) {
            //printf("Fermeture %i\n", i); // TODO nettoyage
          }
        }
      }
      else if (-1 == ret_pol)
        ERROR_EXIT("poll:");
    };

    /* on attend les processus fils */

    /* on ferme les descripteurs proprement */

    /* on ferme la socket d'ecoute */
    free(buffer);
    free_machines(machines, num_procs);
  }
  exit(EXIT_SUCCESS);
}
