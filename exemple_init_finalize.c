#include "dsm.h"

int main(int argc, char **argv)
{
    dsm_init(argc,argv);
    printf("Bonjour depuis le bout du tunnel.\n");
    dsm_finalize();
    return 1;
}
