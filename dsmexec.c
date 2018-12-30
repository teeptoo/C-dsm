#include "common_impl.h"

/* ************************************************************ */
/* Variables globales                                           */
/* ************************************************************ */

// Chemin vers l'executable dsmwrap supposé connu et partagé
#define DSMWRAP_PATH "./bin/dsmwrap"

// Un tableau gérant les infos d'indentification des processus dsm
dsm_proc_t *dsm_array = NULL;

// Nombre de processus effectivement créées
int * num_procs_creat = NULL;

/* ************************************************************ */
/* Fonctions générales                                          */
/* ************************************************************ */

// Informe l'utilisateur sur les arguments nécessaires pour la dsm
void usage(void) {
    fprintf(stdout,"Usage : dsmexec machine_file executable arg1 arg2 ...\n");
    fflush(stdout);
    exit(EXIT_FAILURE);
}

/* ************************************************************ */
/* Gestion de fichiers                                          */
/* ************************************************************ */

// Vérifie l'existence d'un fichier
void check_file_existence(char * path) {
    if (-1 == access(path, F_OK)) {
        fprintf(stderr, "Le chemin '%s' n'est pas correct.\n", path);
        ERROR_EXIT("access");
    }
}

// Transforme les chemins relatifs en chemins absolus
void force_abs_path(char * path, char * abs_path) {
    if(NULL == realpath(path, abs_path)) {
        fprintf(stderr, "Le chemin '%s' n'a pas pu être résolu.\n", path);
        ERROR_EXIT("realpath");
    }
}

// Compte les lignes d'un fichier en détectant les caractères de fin de ligne
int count_lines(FILE * file) {
    char temp_char;
    int i=0;

    for(temp_char=getc(file); temp_char!= EOF; temp_char=getc(file))
        if(temp_char == '\n')
            i++;

    return i;
}

// Remplit un tableau de chaines de caractères à l'aide du machine_file avec les hostname avec allocation de mémoire
void read_machine_file(FILE * file, char * machines[], int num_procs) {
    int i;
    char chaine[HOSTNAME_MAX_LENGTH];

    for (i = 0; i < num_procs; i++) {
        machines[i] = malloc(sizeof(char) * HOSTNAME_MAX_LENGTH);
        fgets(chaine, HOSTNAME_MAX_LENGTH, file);
        chaine[strlen(chaine)-1] = '\0'; // enlève le \n
        strcpy(machines[i], chaine);
    }
}

/* ************************************************************ */
/* Traitant de signal & Gestion des processus fils              */
/* ************************************************************ */

// Récupère les processus fils terminés et effectue un join pour libération des ressources
void sigchld_handler(int sig) {
    pid_t ret_wait;

    do {
        ret_wait = waitpid((pid_t) -1, NULL, WNOHANG);
        if((-1 == ret_wait) && (errno != ECHILD)) { ERROR_EXIT("waitpid (handler)"); }
    } while(ret_wait > 0);
}

// Vérifie une seconde fois la terminaison de tous les fils en parcourant dsm_array et en joinant tous les PID
void wait_all(int num_procs) {
    int i;
    pid_t wait_ret;
    for (i = 0; i < num_procs; ++i) {
        wait_ret = waitpid(dsm_array[i].pid, NULL, WNOHANG);
        if((-1 == wait_ret) && (errno != ECHILD)) { ERROR_EXIT("waitpid (wait_all)"); }
    }
}

/* ************************************************************ */
/* Gestion de la mémoire                                        */
/* ************************************************************ */

// Désallocation du tableau de chaines de caractères des machines
void free_machines(char * machines[], int num_procs) {
    int i;
    for (i = 0; i < num_procs; i++)
        free(machines[i]);
    free(machines);
}

// Allocation d'un tableau de tubes pour tous les processus
int ** alloc_tubes(int num_procs) {
    int i;
    int ** tubes;
    tubes = malloc(sizeof(int *) * num_procs);
    memset(tubes, 0, sizeof(int *) * num_procs);
    for (i = 0; i < num_procs; i++)
        tubes[i] = malloc(sizeof(int)*2);
    return tubes;
}

// Désallocation d'un tableau de tubes
void free_tubes(int ** tubes, int num_procs) {
    int i;
    for (i = 0; i < num_procs; i++)
        free(tubes[i]);
    free(tubes);
}

// Allocation d'un tableau de struct dsm_proc pour la variable globale dsm_array en mémoire partagée
void alloc_dsm_array(int num_procs) {
    dsm_array = mmap(NULL, sizeof(dsm_proc_t) * num_procs, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if((void *) -1 == dsm_array) { ERROR_EXIT("mmap"); }
    memset(dsm_array, 0, sizeof(dsm_proc_t) * num_procs);
}

// Désallocation du tableau de struct dsm_proc de la variable globale
void free_dsm_array(int num_procs) {
    if(-1 == munmap(dsm_array, sizeof(dsm_proc_t) * num_procs)) { ERROR_EXIT("munmap"); }
}

// Allocation d'un entier pour les processus réellement créeer pour la variable globale en mémoire partagée
void alloc_num_procs_creat(void) {
    num_procs_creat = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if((void *) -1 == num_procs_creat) { ERROR_EXIT("mmap"); }
    *num_procs_creat = 0;
}

// Désallocation de l'entier num_procs_creat en variable globale et mémoire partagée
void free_num_procs_creat(void) {
    if(-1 == munmap(num_procs_creat, sizeof(int))) { ERROR_EXIT("munmap"); }
}

/* ************************************************************ */
/* Gestion des descripteurs de fichiers                         */
/* ************************************************************ */

// Ferme les tubes du tableau non utile au processus fils
void close_tubes_autres_process(int ** tubes, int rang_actuel) {
    int i;
    for (i = 0; i < rang_actuel; ++i) {
        close(tubes[i][0]);
        close(tubes[i][1]);
    }
}

// Ferme tous les tubes du tableau de tubes
void close_tous_tubes_lecture(int ** tubes, int num_procs) {
    int i;
    for (i = 0; i < num_procs; ++i)
        close(tubes[i][0]);
}

// Ferme tous les descripteurs de fichiers des sockets d'initialisation avec les processus
void close_fds_dsm_procs(int num_procs) {
    int i;
    for (i = 0; i < num_procs; ++i)
        close(dsm_array[i].fd_init);
}

/* ************************************************************ */
/* main                                                         */
/* ************************************************************ */

int main(int argc, char *argv[])
{
    if (argc < 3)
        usage();
    else {
        /* ************************************************************ */
        /* Déclarations pour la pile                                    */
        /* ************************************************************ */

        struct sigaction sigchld_sigaction; // pour traitant de signal sur SIGCHILD
        int num_procs = 0; // nombre de processus à créer
        int i, j; // compteurs
        int ret_pol; // code de retour du poll
        int sock_init; // socket d'initialisation
        int sock_init_port; // port attribué pour la socket d'initialisation
        int fd_temp; // descripteur de fichier stocké temporairement après le accept
        int rang_temp; // rang temporaire pour les accept
        pid_t pid; // usage temporaire lors de la création des fils
        ssize_t read_count; // compteur lors de lecture avec des fonctions de type read/write
        char dsmwrap_abs_path[FILE_NAME_MAX_LENGTH];
        char executable_abs_path[FILE_NAME_MAX_LENGTH];
        char current_machine_hostname[HOSTNAME_MAX_LENGTH];
        char rang_string[INT_MAX_LENGTH]; // rang au format chaine de caractère
        char sock_init_port_string[INT_MAX_LENGTH]; // sock_init_port sous forme de chaine
        char buffer[BUFFER_LENGTH];

        // pointeurs sans malloc
        char * buffer_cursor = NULL;

        /* ************************************************************ */
        /* Récupération du nombre de machines distantes                 */
        /* ************************************************************ */

        check_file_existence(argv[1]); // machine_file
        FILE *  machine_file = fopen("machine_file", "r");
        if(NULL == machine_file) { ERROR_EXIT("fopen"); }
        num_procs = count_lines(machine_file);

        /* ************************************************************ */
        /* Déclarations pour le tas avec future allocation dynamique   */
        /* ************************************************************ */

        char ** arg_ssh; // tableau d'arguments qui sera transmis par SSH
        char ** machines = NULL; // liste des noms des machines distantes
        int ** tubes_stdout = NULL;
        int ** tubes_stderr = NULL;
        struct pollfd * poll_tubes = NULL;

        /* ************************************************************ */
        /* Allocations dynamiques                                       */
        /* ************************************************************ */

        // mallocs
        arg_ssh = malloc(sizeof(char * ) * (argc + 7));
        machines = malloc(sizeof(char *) * num_procs);
        tubes_stdout = alloc_tubes(num_procs);
        tubes_stderr = alloc_tubes(num_procs);
        poll_tubes = malloc(sizeof(struct pollfd)*num_procs*2);

        // mémoires partagées
        alloc_dsm_array(num_procs);
        alloc_num_procs_creat();

        /* ************************************************************ */
        /* Traitement des chemins                                       */
        /* ************************************************************ */

        // dsmwrap
        force_abs_path(DSMWRAP_PATH, dsmwrap_abs_path);
        check_file_existence(dsmwrap_abs_path);

        // executable final
        force_abs_path(argv[2], executable_abs_path);
        check_file_existence(executable_abs_path);

        /* ************************************************************ */
        /* Mise en place du traitant de signal pour récuperer les fils  */
        /* ************************************************************ */

        memset(&sigchld_sigaction, 0, sizeof(struct sigaction));
        sigchld_sigaction.sa_handler = sigchld_handler;
        if (-1 == sigaction(SIGCHLD, &sigchld_sigaction, NULL)) { ERROR_EXIT("sigaction"); }

        /* ************************************************************ */
        /* Remplissage du tableau machines[] avec les hostname          */
        /* ************************************************************ */

        if(-1 == fseek(machine_file, 0, SEEK_SET)) { ERROR_EXIT("fseek"); }
        read_machine_file(machine_file, machines, num_procs);
        fclose(machine_file);

        /* ************************************************************ */
        /* Création de la socket d'écoute                               */
        /* ************************************************************ */

        sock_init = creer_socket(&sock_init_port);
        set_cloexec_flag(sock_init);
        sprintf(sock_init_port_string, "%d", sock_init_port);

        /* ************************************************************ */
        /* Création des fils                                            */
        /* ************************************************************ */

        for(i = 0; i < num_procs ; i++) {

            // Création du tube pour la redirection de stdout
            if(-1 == pipe(tubes_stdout[i])) { ERROR_EXIT("pipe") }

            // Création du tube pour la redirection de stderr
            if(-1 == pipe(tubes_stderr[i])) { ERROR_EXIT("pipe"); }

            // Ecoute effective
            if(-1 == listen(sock_init, *num_procs_creat)) { ERROR_EXIT("listen"); }

            // Fork
            pid = fork();
            if(pid == -1) { ERROR_EXIT("fork"); }

            if (pid == 0) { // fils
                // Pré-remplissage de dsm_array avec affectation du rang
                strcpy(dsm_array[i].connect_info.dist_hostname, machines[i]);
                dsm_array[i].connect_info.rang = i;
                sprintf(rang_string, "%d", i);

                // Redirection stdout + fermetures stdout des autres processus
                close(STDOUT_FILENO);
                close(tubes_stdout[i][0]);
                dup(tubes_stdout[i][1]);
                close_tubes_autres_process(tubes_stdout, i);

                // Redirection stderr + fermetures stderr des autres processus
                close(STDERR_FILENO);
                close(tubes_stderr[i][0]);
                dup(tubes_stderr[i][1]);
                close_tubes_autres_process(tubes_stderr, i);

                // Création du tableau d'arguments pour l'execv ssh
                if(-1 == gethostname(current_machine_hostname, HOSTNAME_MAX_LENGTH) ) { ERROR_EXIT("gethostname"); }
                memset(arg_ssh, 0, sizeof(char *) * (argc + 7));

                // Commande ssh
                arg_ssh[0] = "dsmwrap"; // pour commande execvp
                arg_ssh[1] = machines[i]; // machine distante pour ssh
                arg_ssh[2] = dsmwrap_abs_path; // chemin de dsmwrap supposé connu
                // dsmwrap args
                arg_ssh[3] = current_machine_hostname;
                arg_ssh[4] = sock_init_port_string;
                arg_ssh[5] = rang_string; // rang actuel
                arg_ssh[6] = executable_abs_path; // programme final à executer
                // executable final args
                for (j = 0; j < (argc-3); ++j)
                    arg_ssh[j+7] = argv[j+3];
                // fin des args
                arg_ssh[j+8]= NULL ; // pour commande execvp

                // execution
                ++(*num_procs_creat);
                if(DEBUG_PHASE1) { printf("[dsm|lanceur(fils)] Lancement de dsmwrap sur %s.\n", machines[i]); fflush(stdout); }
                execvp("ssh", arg_ssh);
                ERROR_EXIT("execvp");

            } else  if(pid > 0) { // Père
                // Fermeture des extremites des tubes non utiles
                close(tubes_stdout[i][1]);
                close(tubes_stderr[i][1]);
            } // end else (père)

        } // end for créations processus

        /* ************************************************************ */
        /* Synchronisation : attente que tous les exec soient faits     */
        /* ************************************************************ */
        while(*num_procs_creat != num_procs) { ; } // TODO remplacer par une condition logique

        /* ************************************************************ */
        /* Récupération des infos de connexion inter-processus dsm      */
        /* ************************************************************ */

        for(i = 0; i < num_procs ; i++){
            // On accepte les connexions des processus dsm
            fd_temp = do_accept(sock_init);

            // On recupere le rang du processus qui vient de se connecter
            rang_temp = recv_int(fd_temp);

            // On remplit dsm_array avec le fd utile pour la phase d'initailisation
            dsm_array[rang_temp].fd_init = fd_temp;

            // On recupere le pid du processus distant
            dsm_array[rang_temp].pid = recv_int(fd_temp);

            // On recupere le numero de port de la socket d'ecoute des processus distants
            dsm_array[rang_temp].connect_info.dist_port = recv_int(fd_temp);

            if(DEBUG_PHASE1) {
                printf("[dsm|lanceur] Rang=%d, Hostname=%s, PID distant=%d, Port dsm=%d.\n",
                        dsm_array[rang_temp].connect_info.rang,
                        dsm_array[rang_temp].connect_info.dist_hostname,
                        dsm_array[rang_temp].pid,
                        dsm_array[rang_temp].connect_info.dist_port);
                fflush(stdout);
            } // end if DEBUG
        } // end for

        /* ************************************************************ */
        /* Envoi des infos de connexions inter-processus dsm à tous les */
        /* processus                                                    */
        /* ************************************************************ */

        for(i = 0; i < num_procs ; i++) {

            // Signal de synchronisation (appel bloquant)
            send_int(dsm_array[i].fd_init, 0);

            // Envoi du nombre de processus aux processus dsm
            send_int(dsm_array[i].fd_init, num_procs);

            // Envoi des rangs aux processus dsm
            send_int(dsm_array[i].fd_init, dsm_array[i].connect_info.rang);

            // Envoi des infos de connexion aux processus sauf ses propres infos
            for (j = 0; j < num_procs; ++j) {
             if(dsm_array[j].connect_info.rang != dsm_array[i].connect_info.rang)
                 send_dsm_infos(dsm_array[i].fd_init ,dsm_array[j].connect_info);
            } // end for j<num_procs

        } // for i<num_procs

        /* ************************************************************ */
        /* Fermeture de la socket d'initialisation                      */
        /* ************************************************************ */

        close_fds_dsm_procs(num_procs);
        close(sock_init);

        /* ************************************************************ */
        /* gestion des E/S : on recupere les caracteres sur les tubes   */
        /* de redirection de stdout/stderr                              */
        /* ************************************************************ */

        // Remplissage de la structure pour le poll
        memset(poll_tubes, 0, sizeof(struct pollfd)*num_procs*2);
        for (i = 0; i < num_procs; i++) {
            poll_tubes[2*i].fd = tubes_stdout[i][0];
            poll_tubes[2*i+1].fd = tubes_stderr[i][0];
            poll_tubes[2*i].events = POLLIN;
            poll_tubes[2*i+1].events = POLLIN;
            poll_tubes[2*i].revents = 0;
            poll_tubes[2*i+1].revents = 0;
        }

        // Tant qu'il reste des processus vivants, je poll et affiche le contenu
        while(*num_procs_creat)
        {
            do {
                ret_pol = poll(poll_tubes, (nfds_t)2*num_procs, 100);
            } while ((ret_pol == -1) && (errno == EINTR));

            // Si activité
            if (ret_pol > 0) {
                for (i = 0; i < 2*num_procs; i++) { // pour tous les tubes

                    if (poll_tubes[i].revents & POLLIN) { // si on recoit du texte à afficher
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

                    else if(poll_tubes[i].revents & POLLHUP) { // un tube a été fermé
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
        }; // end while(*num_procs_creat)

        /* ************************************************************ */
        /* Flush des sorties                                            */
        /* ************************************************************ */

        fflush(stdout);
        fflush(stderr);

        /* ************************************************************ */
        /* Attente des processus fils qui n'auraient pas déjà été pris  */
        /* en compte par le signal handler                              */
        /* ************************************************************ */

        wait_all(num_procs);

        /* ************************************************************ */
        /* Fermeture des descripteurs de fichiers                       */
        /* ************************************************************ */
        close_tous_tubes_lecture(tubes_stdout, num_procs);
        close_tous_tubes_lecture(tubes_stderr, num_procs);


        /* ************************************************************ */
        /* Désallocation                                                */
        /* ************************************************************ */

        // mallocs
        free(arg_ssh);
        free_machines(machines, num_procs);
        free_tubes(tubes_stdout, num_procs);
        free_tubes(tubes_stderr, num_procs);
        free(poll_tubes);

        // mémoires partages
        free_dsm_array(num_procs);
        free_num_procs_creat();

    } // end if argc ok

    exit(EXIT_SUCCESS);
}
