/* Upright/upside-down follow (resident). The kernel reports orientation
 * changes of the kx122 as EV_MSC/MSC_RAW on gpio-keys (KOReader's values).
 * Only the two portrait orientations are followed: WeRead keeps its pages
 * portrait, so landscape (and face up/down) reports are ignored.
 * Android is rotated with packet 7 once the new orientation has held for
 * ROT_SETTLE seconds and no contact is in progress; touches and physical page
 * keys follow the orientation Android acknowledged. The first due rotation
 * syncs Android to upright at the start of every foreground period. */
#define ROT_UPRIGHT 0x18
#define ROT_UPSIDE_DOWN 0x17
#define ROT_SETTLE 0.5
#define ROT_RETRY 5.0
#define ROT_SWITCH "/mnt/onboard/.adds/kobo-android-lab/resident-v188/rotate-disabled"
struct rotate_follow { int enabled,applied,wanted;double since;unsigned reports,changes,failures; };
static void rotate_init(struct rotate_follow *r,int resident){
 memset(r,0,sizeof(*r));r->enabled=resident&&access(ROT_SWITCH,F_OK)!=0;
 r->applied=resident?-1:0;r->wanted=0;
}
/* A gyro report (or the test hook); value is the raw MSC_RAW code. */
static void rotate_report(struct rotate_follow *r,int value,double t){
 r->reports++;if(!r->enabled)return;
 int w=value==ROT_UPRIGHT?0:value==ROT_UPSIDE_DOWN?2:-1;
 if(w>=0 && w!=r->wanted){r->wanted=w;r->since=t;}
}
static int rotate_due(const struct rotate_follow *r,double t){return r->wanted!=r->applied && t-r->since>=ROT_SETTLE;}
static void rotate_result(struct rotate_follow *r,int ok,double t){
 if(ok){if(r->applied>=0)r->changes++;r->applied=r->wanted;}
 else{r->failures++;r->since=t+ROT_RETRY-ROT_SETTLE;}
}
/* Physical portrait screen pixels to Android's logical ones. */
static void rotate_point(const struct rotate_follow *r,int *x,int *y){
 if(r->applied==2){*x=KOBO_FB_WIDTH-1-*x;*y=KOBO_FB_HEIGHT-1-*y;}
}
/* Test hook (resident): a root-owned regular file test-rotate holding 0 or 2
 * acts as a gyro report of that orientation. */
static int test_rotate(void){
 static const char path[]="/tmp/kobo-android-runtime/lab/test-rotate";
 struct stat st;if(lstat(path,&st)||!S_ISREG(st.st_mode)||st.st_uid!=0||st.st_size>8)return -1;
 FILE *f=fopen(path,"r");int v=-1,ok=f&&fscanf(f,"%d",&v)==1;if(f)fclose(f);unlink(path);
 return ok&&v==0?ROT_UPRIGHT:ok&&v==2?ROT_UPSIDE_DOWN:-1;
}
