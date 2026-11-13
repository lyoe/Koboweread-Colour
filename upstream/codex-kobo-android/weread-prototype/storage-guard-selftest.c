#define main broker_main
#include "../installation-prototype/process-broker.c"
#undef main
static void check(int expected,const char*label){int got=unavailable_external_storage();printf("STORAGE_GUARD_CASE %s actual=%d expected=%d\n",label,got,expected);fflush(stdout);if(got!=expected)_exit(90);}
int main(void){
 if(close_inherited_fds()<0||geteuid()!=0)return 71;
 char root[]="/tmp/kobo-storage-guard.XXXXXX";if(!mkdtemp(root))return 72;
 pid_t child=fork();
 if(child==0){
  if(chroot(root)<0||chdir("/")<0)_exit(73);
  if(mkdir("/storage",0755)<0||mkdir("/proc",0755)<0)_exit(74);
  FILE*f=fopen("/proc/mounts","w");if(!f)_exit(75);fclose(f);check(1,"empty");
  if(mkdir("/storage/emulated",0700)<0||mkdir("/storage/emulated/legacy",0700)<0)_exit(76);check(1,"root_locked_scaffold");
  chmod("/storage/emulated",0755);check(0,"app_traversable_directory");chmod("/storage/emulated",0700);
  f=fopen("/proc/mounts","w");fputs("none /tmp/kobo-android-runtime/storage/emulated ext2 ro 0 0\n",f);fclose(f);check(0,"mounted_volume");
  f=fopen("/proc/mounts","w");fclose(f);check(1,"unmounted_locked_directory");
  rmdir("/storage/emulated/legacy");rmdir("/storage/emulated");symlink("/tmp","/storage/emulated");check(0,"symlink");unlink("/storage/emulated");
  f=fopen("/storage/unexpected","w");fclose(f);check(0,"unexpected_file");unlink("/storage/unexpected");check(1,"empty_restored");_exit(0);
 }
 int status;if(child<0||waitpid(child,&status,0)!=child)return 77;
 const char*paths[]={"/storage/emulated/legacy","/storage/emulated","/storage/unexpected","/proc/mounts","/storage","/proc",""};
 for(unsigned i=0;i<sizeof(paths)/sizeof(paths[0]);i++){char path[512];snprintf(path,sizeof(path),"%s%s",root,paths[i]);unlink(path);rmdir(path);}
 if(!WIFEXITED(status)||WEXITSTATUS(status)!=0)return 78;
 puts("STORAGE_GUARD_SELFTEST_OK");return 0;
}
