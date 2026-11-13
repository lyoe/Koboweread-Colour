// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef KOBO_READER_IDENTITY_H
#define KOBO_READER_IDENTITY_H
struct reader {pid_t pid,parent;unsigned long long started;int stopped;char state;};
static int identity(pid_t pid,struct reader *out,int require_sleeping){
 char path[64],target[256],comm[64],line[4096],state;int parent;unsigned long long started=0;
 snprintf(path,sizeof(path),"/proc/%d/exe",pid);ssize_t n=readlink(path,target,sizeof(target)-1);
 if(n<0)return -1;target[n]=0;if(strcmp(target,READER_EXE))return -1;
 snprintf(path,sizeof(path),"/proc/%d/comm",pid);FILE *f=fopen(path,"r");if(!f)return -1;
 int ok=fgets(comm,sizeof(comm),f)!=NULL;fclose(f);if(!ok || strcmp(comm,"reader.lua\n"))return -1;
 snprintf(path,sizeof(path),"/proc/%d/stat",pid);f=fopen(path,"r");if(!f)return -1;
 ok=fgets(line,sizeof(line),f)!=NULL;fclose(f);if(!ok)return -1;
 char *fields=strrchr(line,')');if(!fields || sscanf(fields+2,"%c %d",&state,&parent)!=2)return -1;
 // Fields following comm begin with state (#3); starttime is #22.
 char *save=NULL,*token=strtok_r(fields+2," ", &save);int field=3;
 while(token){if(field==22){started=strtoull(token,NULL,10);break;}field++;token=strtok_r(NULL," ",&save);}
 if(!started || (require_sleeping && state!='S'))return -1;
 out->pid=pid;out->parent=parent;out->started=started;out->state=state;return 0;
}
static inline int same_reader(const struct reader *r){struct reader actual={0};return identity(r->pid,&actual,0)==0 && actual.started==r->started;}
static int find_readers(struct reader readers[2],char required_state){
 DIR *dir=opendir("/proc");if(!dir)return -1;struct dirent *entry;int count=0;
 while((entry=readdir(dir))){char *end;long pid=strtol(entry->d_name,&end,10);if(*end || pid<=0)continue;
  struct reader r={0};if(identity((pid_t)pid,&r,0)<0)continue;
  if(r.state!=required_state){closedir(dir);return -1;}
  if(count==2){closedir(dir);return -1;}readers[count++]=r;
 }
 closedir(dir);
 return count==2 && (readers[0].parent==readers[1].pid || readers[1].parent==readers[0].pid)?0:-1;
}
#endif
