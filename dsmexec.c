#include "common_impl.h"

#define DSMWRAP_PATH "/home/theo/PR204/bin/dsmwrap"

/* un tableau gerant les infos d'identification */
/* des processus dsm */
dsm_proc_t *dsm_array = NULL;

/* le nombre de processus effectivement crees */
int * num_procs_creat = NULL;

void usage(void) {
    fprintf(stdout,"Usage : dsmexec machine_file executable arg1 arg2 ...\n");
    fflush(stdout);
    exit(EXIT_FAILURE);
}

void check_file_existence(char * path) {
    if (-1 == access(path, F_OK)) {
        fprintf(stderr, "Le chemin '%s' n'est pas correct.\n", path);
        ERROR_EXIT("access");
    }
}

void sigchld_handler(int sig) {
    /* on traite les fils qui se terminent */
    /* pour eviter les zombies */
    int status;
    do {
        waitpid(-1, &status, WNOHANG);
        --(*num_procs_creat);
        if(-1 == status) { ERROR_EXIT("waitpid"); }
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
    int i;
    char chaine[HOSTNAME_MAX_LENGTH];
    // lecture fichier
    for (i = 0; i < num_procs; i++) {
        machines[i] = malloc(sizeof(char) * HOSTNAME_MAX_LENGTH);
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

int ** alloc_tubes(int num_procs) {
    int i;
    int ** tubes;
    tubes = malloc(sizeof(int *) * num_procs);
    memset(tubes, 0, sizeof(int *) * num_procs);
    for (i = 0; i < num_procs; i++)
        tubes[i] = malloc(sizeof(int)*2);
    return tubes;
}

void free_tubes(int ** tubes, int num_procs) {
    int i;
    for (i = 0; i < num_procs; i++)
        free(tubes[i]);
    free(tubes);
}

dsm_proc_t * alloc_dsm_array(int num_procs) {
    dsm_proc_t * array;
    array = mmap(NULL, sizeof(dsm_proc_t) * num_procs, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if((void *) -1 == dsm_array) { ERROR_EXIT("mmap"); }
    memset(array, 0, sizeof(dsm_proc_t) * num_procs);
    return array;
}

void free_dsm_array(int num_procs) {
    if(-1 == munmap(dsm_array, sizeof(dsm_proc_t) * num_procs)) { ERROR_EXIT("munmap"); }
}

int * alloc_num_procs_creat(void) {
    int * num;
    num = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if((void *) -1 == num) { ERROR_EXIT("mmap"); }
    *num = 0;
    return num;
}

void free_num_procs_creat(void) {
    if(-1 == munmap(num_procs_creat, sizeof(int))) { ERROR_EXIT("munmap"); }
}

void close_tubes_autres_process(int ** tubes, int rang_actuel) {
    int i;
    for (i = 0; i < rang_actuel; ++i) {
        close(tubes[i][0]);
        close(tubes[i][1]);
    }
}

void close_tous_tubes_lecture(int ** tubes, int num_procs) {
    int i;
    for (i = 0; i < num_procs; ++i)
        close(tubes[i][0]);
}

int get_rank_from_hostname_available(char * hostname, int num_procs) {
    /* Renvoie le rang de la première instance lancée sur hostname qui n'a pas encore de fd affecté */
    int i, result_comp;
    for (i = 0; i < num_procs; ++i) {
        result_comp = strncmp(hostname, dsm_array[i].connect_info.dist_hostname, strlen(hostname)+1);
        if((0 == result_comp) && (0 == dsm_array[i].connect_info.conn_fd))
            return i;
    }
    ERROR_EXIT("get_rank_from_hostname_available"); // incohérence dans dsm_array
}

int rang_tubes_to_rang(int rang_tube, int num_procs) {
    int i;
    for (i = 0; i < num_procs; ++i) {
        if(dsm_array[i].rang_tubes == rang_tube)
            return dsm_array[i].rang;
    }
    ERROR_EXIT("rang_tubes_to_rang"); // incohérence dans dsm_array
}

void close_fds_dsm_procs(int num_procs) {
    int i;
    for (i = 0; i < num_procs; ++i)
        close(dsm_array[i].connect_info.conn_fd);
}

int main(int argc, char *argv[])
{
    if (argc < 3)
        usage();
    else {
        check_file_existence(DSMWRAP_PATH);
        check_file_existence(argv[1]); // machine_file
        check_file_existence(argv[2]); // executable

        /* déclarations pile */
        struct sigaction sigchld_sigaction; // pour traitant de signal sur SIGCHILD
        int num_procs = 0; // nombre de processus à créer
        int i, j; // compteurs
        int ret_pol; // code de retour du poll
        int sock_init; // socket d'initialisation
        int sock_init_port; // port attribué pour la socket d'initialisation
        int fd_temp; // descripteur de fichier stocké temporairement après le accept
        int rang_temp; // rang temporaire pour les accept et le poll
        int dsmwrap_argc; // nombres d'arguments après le machine_file + executable
        pid_t pid; // usage temporaire lors de la création des fils
        ssize_t read_count; // compteur lors de lecture avec des fonctions de type read/write
        char current_machine_hostname[HOSTNAME_MAX_LENGTH];
        char sock_init_port_string[PORT_MAX_LENGTH]; // sock_init_port sous forme de chaine
        char hostname_temp[HOSTNAME_MAX_LENGTH]; // hostname reçu avant remplissage de la structure dsm_array
        char buffer[BUFFER_LENGTH];

        /* récupération du nombre de machines distantes */
        FILE *  machine_file = fopen("machine_file", "r");
        if(NULL == machine_file) { ERROR_EXIT("fopen"); }
        num_procs = count_lines(machine_file);

        /* déclarations de pointeurs sans malloc */
        char * buffer_cursor = NULL;

        /* déclarations pour tas avec futur malloc */
        char ** arg_ssh; // tableau d'arguments qui sera transmis par SSH
        char ** machines = NULL; // liste des noms des machines distantes
        int ** tubes_stdout = NULL;
        int ** tubes_stderr = NULL;
        struct pollfd * poll_tubes = NULL;

        /* allocations */
        arg_ssh = malloc(sizeof(char * ) * (argc + 6));
        machines = malloc(sizeof(char *) * num_procs);
        tubes_stdout = alloc_tubes(num_procs);
        tubes_stderr = alloc_tubes(num_procs);
        poll_tubes = malloc(sizeof(struct pollfd)*num_procs*2);

        /* mémoires partagées */
        dsm_array = alloc_dsm_array(num_procs);
        num_procs_creat = alloc_num_procs_creat();

        /* Mise en place d'un traitant pour recuperer les fils zombies*/
        memset(&sigchld_sigaction, 0, sizeof(struct sigaction));
        sigchld_sigaction.sa_handler = sigchld_handler;
        if (-1 == sigaction(SIGCHLD, &sigchld_sigaction, NULL)) { ERROR_EXIT("sigaction"); }

        /* lecture du fichier de machines */
        /* 1- on recupere le nombre de processus a lancer */
        /* 2- on recupere les noms des machines : le nom de */
        /* la machine est un des elements d'identification */
        if(-1 == fseek(machine_file, 0, SEEK_SET)) { ERROR_EXIT("fseek"); }
        read_machine_file(machine_file, machines, num_procs);
        fclose(machine_file);

        /* creation de la socket d'ecoute */
        sock_init = creer_socket(&sock_init_port);
        sprintf(sock_init_port_string, "%d", sock_init_port);

        /* creation des fils */
        for(i = 0; i < num_procs ; i++) {
            /* creation du tube pour rediriger stdout */
            if(-1 == pipe(tubes_stdout[i])) { ERROR_EXIT("pipe") }
            /* creation du tube pour rediriger stderr */
            if(-1 == pipe(tubes_stderr[i])) { ERROR_EXIT("pipe"); }
            /* + ecoute effective */
            if(-1 == listen(sock_init, *num_procs_creat)) { ERROR_EXIT("listen"); }

            pid = fork();
            if(pid == -1) { ERROR_EXIT("fork"); }

            if (pid == 0) { /* fils */
                /* pré-remplissage de dsm_procs avec affectation des rangs */
                dsm_array[i].pid = getpid();
                dsm_array[i].rang_tubes = i;
                strcpy(dsm_array[i].connect_info.dist_hostname, machines[i]);

                /* redirection stdout */
                /* + fermetures stdout des autres processus */
                close(STDOUT_FILENO);
                close(tubes_stdout[i][0]);
                dup(tubes_stdout[i][1]);
                close_tubes_autres_process(tubes_stdout, i);

                /* redirection stderr */
                /* + fermetures stderr des autres processus */
                close(STDERR_FILENO);
                close(tubes_stderr[i][0]);
                dup(tubes_stderr[i][1]);
                close_tubes_autres_process(tubes_stderr, i);

                /* Creation du tableau d'arguments pour le ssh */
                /* execv args */
                if(-1 == gethostname(current_machine_hostname, HOSTNAME_MAX_LENGTH) ) { ERROR_EXIT("gethostname"); }
                memset(arg_ssh, 0, sizeof(char *) * (argc + 6));
                arg_ssh[0] = "dsmwrap"; // pour commande execvp
                arg_ssh[1] = machines[i]; // machine distante pour ssh
                arg_ssh[2] = DSMWRAP_PATH; // chemin de dsmwrap supposé connu
                /* dsmwrap args */
                arg_ssh[3] = current_machine_hostname;
                arg_ssh[4] = sock_init_port_string;
                arg_ssh[5] = argv[2]; // programme final à executer
                /* program args */
                for (j = 0; j < (argc-3); ++j) {
                    arg_ssh[j+6] = argv[j+3];
                }
                /* fin args */
                arg_ssh[j+7]= NULL ; // pour commande execvp
                /* execution */
                ++(*num_procs_creat);
                execvp("ssh", arg_ssh);
                break;
            } else  if(pid > 0) { /* pere */
                /* fermeture des extremites des tubes non utiles */
                close(tubes_stdout[i][1]);
                close(tubes_stderr[i][1]);
            } // end else (père)
        } // end for créations processus

        while(*num_procs_creat != num_procs) { ; } // TODO remplacer par une condition logique

        for(i = 0; i < num_procs ; i++){
            /* on accepte les connexions des processus dsm */
            fd_temp = do_accept(sock_init);

            /*  On recupere le nom de la machine distante */
            /* 1- d'abord la taille de la chaine */
            /* 2- puis la chaine elle-meme */
            memset(hostname_temp, 0, HOSTNAME_MAX_LENGTH);
            read_line(fd_temp, hostname_temp);

            /* On affecte un rang définitif au processus  et on remplit dsm_array */
            rang_temp = get_rank_from_hostname_available(hostname_temp, num_procs);
            dsm_array[rang_temp].rang = rang_temp;
            dsm_array[rang_temp].connect_info.conn_fd = fd_temp;

            /* On recupere le pid du processus distant  */
            dsm_array[rang_temp].pid = read_int(fd_temp);

            /* On recupere le numero de port de la socket */
            /* d'ecoute des processus distants */
            dsm_array[rang_temp].connect_info.dist_port = read_int(fd_temp);

            if(DEBUG) {
                printf("[Debug] Rang=%d, Hostname=%s, PID distant=%d, Port dsm=%d.\n",
                        dsm_array[i].rang,
                        dsm_array[i].connect_info.dist_hostname,
                        dsm_array[i].pid,
                        dsm_array[i].connect_info.dist_port);
            }
        }
        /* envoi du nombre de processus aux processus dsm*/

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

        while(*num_procs_creat)
        {
            /* je recupere les infos sur les tubes de redirection
            jusqu'à ce qu'ils soient inactifs (ie fermes par les
            processus dsm ecrivains de l'autre cote ...) */
            do {
                ret_pol = poll(poll_tubes, (nfds_t)2*num_procs, 100);
            } while ((ret_pol == -1) && (errno == EINTR));

            if (ret_pol > 0) {
                for (i = 0; i < 2*num_procs; i++) { // pour tous les tubes

                    if (poll_tubes[i].revents & POLLIN) {
                        memset(buffer, 0, BUFFER_LENGTH);
                        buffer_cursor = buffer;
                        read_count = 0;
                        do {
                            read_count = read(poll_tubes[i].fd, buffer_cursor, BUFFER_LENGTH);
                            buffer_cursor += read_count;
                        } while ((read_count == -1) && ((errno == EAGAIN) || (errno == EINTR)));
                        if((read_count == -1)){ ERROR_EXIT("read"); }

                        rang_temp = rang_tubes_to_rang(i/2, num_procs);
                        if(i%2) // contenu sur stderr
                            fprintf(stderr, "[stderr|%d|%s] %s", rang_temp, dsm_array[rang_temp].connect_info.dist_hostname, buffer);
                        else // contenu sur stdout
                            fprintf(stdout, "[stdout|%d|%s] %s", rang_temp, dsm_array[rang_temp].connect_info.dist_hostname, buffer);
                        fflush(stdout);
                    }

                    else if(poll_tubes[i].revents & POLLHUP) {
                        if(DEBUG) { printf("fermeture tube %i\n", i); }
                        fflush(stdout);
                        poll_tubes[i].fd = -1; // on retire le tube du poll en l'ignorant (norme POSIX)
                    } // end else if POLLHUP

                } // end for i de 0 à 2*numprocs-1
            } // end if ret_pol>0
            else if (-1 == ret_pol) { ERROR_EXIT("poll"); }
        }; // end while(num_procs_creat)

        /* flush des sorties */
        fflush(stdout);
        fflush(stderr);

        /* on ferme les descripteurs proprement */
        close_tous_tubes_lecture(tubes_stdout, num_procs);
        close_tous_tubes_lecture(tubes_stderr, num_procs);
        close_fds_dsm_procs(num_procs);

        /* on ferme la socket d'ecoute */
        close(sock_init);

        /* on ferme les autres mallocs */
        free(arg_ssh);
        free_machines(machines, num_procs);
        free_tubes(tubes_stdout, num_procs);
        free_tubes(tubes_stderr, num_procs);
        free(poll_tubes);

        /* on ferme les memoires partagées */
        free_dsm_array(num_procs);
        free_num_procs_creat();

    } // end if argc ok
    exit(EXIT_SUCCESS);
}
