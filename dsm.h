#define _GNU_SOURCE
#include "common_impl.h"
#include <signal.h>
#include <pthread.h>
/* fin des includes */

#define TOP_ADDR    (0x40000000)
#define PAGE_NUMBER (100)
#define PAGE_SIZE   (sysconf(_SC_PAGE_SIZE))
#define BASE_ADDR   (TOP_ADDR - (PAGE_NUMBER * PAGE_SIZE))

typedef enum
{   
   INVALID,
   WRITE,
   NO_CHANGE  
} dsm_page_state_t;

typedef enum
{
    RUNNING,
    QUITTING
} dsm_process_state_t;

typedef enum
{
    SEND_PAGE,
    SEND_INFO,
    PAGE_REQUEST
} dsm_message_t;

typedef int dsm_page_owner_t;

typedef struct
{ 
   dsm_page_state_t status;
   dsm_page_owner_t owner;
} dsm_page_info_t;

dsm_page_info_t table_page[PAGE_NUMBER];

pthread_t comm_daemon;
extern int DSM_NODE_ID;
extern int DSM_NODE_NUM;

char *dsm_init( int argc, char **argv);
void  dsm_finalize( void );
