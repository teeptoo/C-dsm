#include <stdio.h>

int main(int argc, char const *argv[]) {
  printf("Bonjour je suis un programme sans DSM !\n");
  if(argc == 2)
    printf("Arg détécté ! Le voici : %s\n", argv[1]);
    
  return 0;
}
