/* Read listed page ranges of files into the page cache, in list order:
 *   kobo-pageprefetch ROOT LIST
 * LIST lines: "relative/path start-end[,start-end...]" (4 KiB page indexes,
 * inclusive), as written by tests/v188-bootpages.sh from fincore. Each range
 * is read with pread into a scratch buffer (this kernel has no fadvise).
 * Missing files and short reads are skipped. Prints pages read at the end. */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static char buf[1 << 17];
int main(int argc,char **argv){
 if(argc!=3)return 2;
 FILE *list=fopen(argv[2],"r");if(!list)return 3;
 static char line[1 << 16];long pages=0,files=0;
 while(fgets(line,sizeof line,list)){
  char *sp=strchr(line,' ');if(!sp)continue;*sp=0;
  char path[4096];if(snprintf(path,sizeof path,"%s/%s",argv[1],line)>=(int)sizeof path)continue;
  int fd=open(path,O_RDONLY|O_CLOEXEC);if(fd<0)continue;files++;
  for(char *r=strtok(sp+1,",\n");r;r=strtok(0,",\n")){
   long a=0,b=0;if(sscanf(r,"%ld-%ld",&a,&b)!=2||b<a)continue;
   for(long p=a;p<=b;){
    long n=b-p+1;if(n>(long)(sizeof buf/4096))n=sizeof buf/4096;
    ssize_t got=pread(fd,buf,(size_t)n*4096,(off_t)p*4096);if(got<=0)break;
    pages+=(got+4095)/4096;p+=n;
   }
  }
  close(fd);
 }
 fclose(list);printf("KOBO_PAGE_PREFETCH files=%ld pages=%ld mb=%.1f\n",files,pages,pages*4096.0/1048576);
 return 0;
}
