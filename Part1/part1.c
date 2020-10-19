#include <unistd.h>
#include <stdio.h>

int main()
{
  //creating a new process
  int i = fork();

  //getting the process
  int pid = getpid();

  //getting acess to file
  access("test", F_OK);

  //printing
  printf("Hello world!");

  return 0;
}
