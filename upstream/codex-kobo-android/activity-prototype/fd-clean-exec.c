#include <stdio.h>
#include "../runtime-prototype/close-inherited-fds.h"
int main(int argc,char**argv){
 if(argc<2)return 2;
 int count=close_inherited_fds();if(count<0){perror("close inherited descriptors");return 3;}
 fprintf(stderr,"LAB_INHERITED_DESCRIPTORS_CLOSED count=%d\n",count);
 execvp(argv[1],argv+1);perror("exec");return 4;
}
