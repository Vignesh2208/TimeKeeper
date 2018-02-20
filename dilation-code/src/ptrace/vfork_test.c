#include <stdio.h>  
#include <unistd.h>
#include <stdlib.h>


void main()  
 {  
      pid_t pid;  
      printf("Parent\n");  
      pid = vfork();  
      if(pid==0)  
      {  
          execlp("/bin/date","date",NULL);
      }  

      printf("Resuming Parent ...\n");
      execlp("/bin/date","date",NULL);

  }

