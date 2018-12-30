#include "common_impl.h"

#define DSMWRAP_PATH "./bin/dsmwrap"

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

void force_abs_path(char * path, char * abs_path) {
    if(NULL == realpath(path, abs_path)) {
        fprintf(stderr, "Le chemin '%s' n'a pas pu être résolu.\n", path);
        ERROR_EXIT("realpath");
    }
}

void sigchld_handler(int sig) {
    /* on traite les fils qui se terminent */
    /* pour eviter les zombies */
    pid_t ret_wait;
    do {
        ret_wait = waitpid((pid_t) -1, NULL, WNOHANG);
        if((-1 == ret_wait) && (errno != ECHILD)) { ERROR_EXIT("waitpid (handler)"); }
    } while(ret_wait > 0);
}

void wait_all(int num_procs) {
    int i;
    pid_t wait_ret;
    for (i = 0; i < num_procs; ++i) {
        wait_ret = waitpid(dsm_array[i].pid, NULL, WNOHANG);
        if((-1 == wait_ret) && (errno != ECHILD)) { ERROR_EXIT("waitpid"); }
    }
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

void close_fds_dsm_procs(int num_procs) {
    int i;
    for (i = 0; i < num_procs; ++i)
        close(dsm_array[i].fd_init);
}

int main(int argc, char *argv[])
{
    if (argc < 3)
        usage();
    else {
        /* déclarations pile */
        struct sigaction sigchld_sigaction; // pour traitant de signal sur SIGCHILD
        int num_procs = 0; // nombre de processus à créer
        int i, j; // compteurs
        int ret_pol; // code de retour du poll
        int sock_init; // socket d'initialisation
        int sock_init_port; // port attribué pour la socket d'initialisation
        int fd_temp; // descripteur de fichier stocké temporairement après le accept
        int rang_temp; // rang temporaire pour les accept et le poll
        pid_t pid; // usage temporaire lors de la création des fils
        ssize_t read_count; // compteur lors de lecture avec des fonctions de type read/write
        char dsmwrap_abs_path[FILE_NAME_MAX_LENGTH];
        char executable_abs_path[FILE_NAME_MAX_LENGTH];
        char current_machine_hostname[HOSTNAME_MAX_LENGTH];
        char rang_string[INT_MAX_LENGTH];
        char sock_init_port_string[INT_MAX_LENGTH]; // sock_init_port sous forme de chaine
        char buffer[BUFFER_LENGTH];

        /* récupération du nombre de machines distantes */
        check_file_existence(argv[1]); // machine_file
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
        arg_ssh = malloc(sizeof(char * ) * (argc + 7));
        machines = malloc(sizeof(char *) * num_procs);
        tubes_stdout = alloc_tubes(num_procs);
        tubes_stderr = alloc_tubes(num_procs);
        poll_tubes = malloc(sizeof(struct pollfd)*num_procs*2);

        /* mémoires partagées */
        dsm_array = alloc_dsm_array(num_procs);
        num_procs_creat = alloc_num_procs_creat();

        /* traitement des chemins */
        force_abs_path(DSMWRAP_PATH, dsmwrap_abs_path);
        check_file_existence(dsmwrap_abs_path);
        force_abs_path(argv[2], executable_abs_path);
        check_file_existence(executable_abs_path);

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
        set_cloexec_flag(sock_init);
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
                /* pré-remplissage de dsm_array avec affectation du rang */
                strcpy(dsm_array[i].connect_info.dist_hostname, machines[i]);
                dsm_array[i].connect_info.rang = i;
                sprintf(rang_string, "%d", i);

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
                memset(arg_ssh, 0, sizeof(char *) * (argc + 7));
                arg_ssh[0] = "dsmwrap"; // pour commande execvp
                arg_ssh[1] = machines[i]; // machine distante pour ssh
                arg_ssh[2] = dsmwrap_abs_path; // chemin de dsmwrap supposé connu
                /* dsmwrap args */
                arg_ssh[3] = current_machine_hostname;
                arg_ssh[4] = sock_init_port_string;
                arg_ssh[5] = rang_string; // rang actuel
                arg_ssh[6] = executable_abs_path; // programme final à executer
                /* program args */
                for (j = 0; j < (argc-3); ++j)
                    arg_ssh[j+7] = argv[j+3];
                /* fin args */
                arg_ssh[j+8]= NULL ; // pour commande execvp
                /* execution */
                ++(*num_procs_creat);
                if(DEBUG_PHASE1) { printf("[dsm|lanceur(fils)] Lancement de dsmwrap sur %s.\n", machines[i]); fflush(stdout); }
                execvp("ssh", arg_ssh);
                ERROR_EXIT("execvp");
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

            /* On recupere le rang du processus qui vient de se connecter */
            rang_temp = recv_int(fd_temp);

            /* On remplit dsm_array */
            dsm_array[rang_temp].fd_init = fd_temp;

            /* On recupere le pid du processus distant  */
            dsm_array[rang_temp].pid = recv_int(fd_temp);

            /* On recupere le numero de port de la socket */
            /* d'ecoute des processus distants */
            dsm_array[rang_temp].connect_info.dist_port = recv_int(fd_temp);

            if(DEBUG_PHASE1) {
                printf("[dsm|lanceur] Rang=%d, Hostname=%s, PID distant=%d, Port dsm=%d.\n",
                        dsm_array[rang_temp].connect_info.rang,
                        dsm_array[rang_temp].connect_info.dist_hostname,
                        dsm_array[rang_temp].pid,
                        dsm_array[rang_temp].connect_info.dist_port);
                fflush(stdout);
            }
        }

        for(i = 0; i < num_procs ; i++) {
            /* envoi signal synchro */
            send_int(dsm_array[i].fd_init, 0);

            /* envoi du nombre de processus aux processus dsm*/
            send_int(dsm_array[i].fd_init, num_procs);

            /* envoi des rangs aux processus dsm */
            send_int(dsm_array[i].fd_init, dsm_array[i].connect_info.rang);

            /* envoi des infos de connexion aux processus */
            /* sauf ses propres infos */
            for (j = 0; j < num_procs; ++j) {
             if(dsm_array[j].connect_info.rang != dsm_array[i].connect_info.rang)
                 send_dsm_infos(dsm_array[i].fd_init ,dsm_array[j].connect_info);
            }
        }

        /* fermeture de la socket d'initialisation */
        close_fds_dsm_procs(num_procs);
        close(sock_init);

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
                        } while ((-1 == read_count) && ((errno == EAGAIN) || (errno == EINTR)));
                        if(-1 == read_count){ ERROR_EXIT("read"); }

                        if(i%2) // contenu sur stderr
                            fprintf(stderr, "[stderr|%d|%s] %s", i/2, dsm_array[rang_temp].connect_info.dist_hostname, buffer);
                        else // contenu sur stdout
                            fprintf(stdout, "[stdout|%d|%s] %s", i/2, dsm_array[rang_temp].connect_info.dist_hostname, buffer);

                        fflush(stdout);
                        fflush(stderr);
                    }

                    else if(poll_tubes[i].revents & POLLHUP) {
                        if(DEBUG_PHASE1) { printf("[dsm|lanceur] Fermeture tube %i.\n", i); fflush(stdout); }
                        poll_tubes[i].fd = -1; // on retire le tube du poll en l'ignorant (norme POSIX)
                        if(i%2) { // puis on vérifie si c'est le dernier des deux tubes, si oui on reduit num_procs_creat
                            if(-1 == poll_tubes[i-1].fd)
                                --(*num_procs_creat);
                        } else {
                            if(-1 == poll_tubes[i+1].fd)
                                --(*num_procs_creat);
                        }

                    } // end else if POLLHUP

                } // end for i de 0 à 2*numprocs-1
            } // end if ret_pol>0
            else if (-1 == ret_pol) { ERROR_EXIT("poll"); }
        }; // end while(num_procs_creat)

        /* flush des sorties */
        fflush(stdout);
        fflush(stderr);

        /* on attend les processus fils */
        /* qui n'auraient pas déjà été pris en compte par le signal handler */
        wait_all(num_procs);

        /* on ferme les descripteurs proprement */
        close_tous_tubes_lecture(tubes_stdout, num_procs);
        close_tous_tubes_lecture(tubes_stderr, num_procs);


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
