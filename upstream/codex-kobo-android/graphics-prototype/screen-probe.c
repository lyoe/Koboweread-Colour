// SPDX-License-Identifier: GPL-3.0-or-later
// Temporary framebuffer display; keeps the existing frame in RAM and restores it.
#include "fbink.h"
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
static volatile sig_atomic_t interrupted;
static void stop(int sig) { (void)sig; interrupted = 1; }
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr,"Usage: screen-probe --info | image.png [seconds]\n"); return 2; }
    FBInkConfig cfg = {0};
    cfg.is_quiet = true;
    int fd = fbink_open();
    if (fd < 0) return 3;
    if (fbink_init(fd, &cfg) < 0) { fbink_close(fd); return 4; }
    FBInkDump saved = {0};
    if (fbink_dump(fd, &saved) < 0) { fbink_close(fd); return 5; }
    printf("Framebuffer snapshot: %zu bytes\n", saved.size);
    if (!strcmp(argv[1],"--info")) {
        fbink_free_dump_data(&saved); fbink_close(fd); return 0;
    }
    unsigned seconds = argc > 2 ? (unsigned)strtoul(argv[2],NULL,10) : 5;
    if (seconds > 30) seconds = 30;
    signal(SIGINT,stop); signal(SIGTERM,stop); signal(SIGHUP,stop);
    cfg.is_cleared = true; cfg.is_flashing = true;
    cfg.halign = CENTER; cfg.valign = CENTER;
    cfg.scaled_width = -1; cfg.scaled_height = -1;
    int rc = fbink_print_image(fd,argv[1],0,0,&cfg);
    if (rc >= 0) {
        struct timespec delay = {1,0};
        for (unsigned i=0;i<seconds && !interrupted;i++) nanosleep(&delay,NULL);
    }
    cfg.is_cleared = false;
    int restored = fbink_restore(fd,&cfg,&saved);
    FBInkDump check = {0};
    int verified = restored >= 0 && fbink_dump(fd,&check) >= 0
        && check.size == saved.size && memcmp(check.data,saved.data,saved.size) == 0;
    fbink_free_dump_data(&check);
    fbink_free_dump_data(&saved); fbink_close(fd);
    if (restored < 0) { fprintf(stderr,"Framebuffer restore failed: %d\n",restored); return 7; }
    if (rc < 0) { fprintf(stderr,"Image display failed: %d\n",rc); return 6; }
    if (!verified) { fprintf(stderr,"Framebuffer byte comparison failed\n"); return 8; }
    puts("FRAMEBUFFER_RESTORE_BYTES_MATCH");
    puts("SCREEN_IMAGE_DISPLAYED_AND_RESTORED"); return 0;
}
