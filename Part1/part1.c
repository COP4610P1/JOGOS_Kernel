#include <unistd.h>
#include <stdio.h>

int main(){
   int i = fork();
   
   int pid = getpid();
  
  
   access("test",F_OK);
   
   printf("Hello world!");
   

  return 0;
}
