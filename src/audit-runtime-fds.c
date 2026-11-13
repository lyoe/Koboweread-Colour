/* Native equivalent of lab/audit-runtime-fds.sh: same checks, messages and
 * exit codes, without forking readlink for every descriptor.
 * Every process whose root is the lab chroot must hold no block device and
 * no host framebuffer, input, eMMC or memory device descriptor; at least five
 * such processes and one UID 10000 process must exist.
 */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *root = "/tmp/kobo-android-runtime";

static int host_device(const char *t) {
    return !strncmp(t, "/dev/fb", 7) || !strncmp(t, "/dev/input/", 11) ||
           !strncmp(t, "/dev/mmcblk", 11) || !strcmp(t, "/dev/mem") || !strcmp(t, "/dev/kmem");
}

int main(void) {
    int count = 0, app_seen = 0;
    char path[96], link[4096];
    DIR *proc = opendir("/proc");
    if (!proc) return 1;
    struct dirent *p;
    while ((p = readdir(proc))) {
        if (p->d_name[0] < '0' || p->d_name[0] > '9') continue;
        snprintf(path, sizeof path, "/proc/%s/root", p->d_name);
        ssize_t n = readlink(path, link, sizeof link - 1);
        if (n < 0) continue;
        link[n] = 0;
        if (strcmp(link, root)) continue;
        count++;
        snprintf(path, sizeof path, "/proc/%s/status", p->d_name);
        FILE *st = fopen(path, "r");
        if (st) {
            char line[256];
            while (fgets(line, sizeof line, st))
                if (!strncmp(line, "Uid:", 4)) { if (atoi(line + 4) == 10000) app_seen = 1; break; }
            fclose(st);
        }
        snprintf(path, sizeof path, "/proc/%s/fd", p->d_name);
        DIR *fds = opendir(path);
        if (!fds) continue;
        struct dirent *f;
        while ((f = readdir(fds))) {
            if (f->d_name[0] == '.') continue;
            char fd[128];
            snprintf(fd, sizeof fd, "/proc/%s/fd/%s", p->d_name, f->d_name);
            n = readlink(fd, link, sizeof link - 1);
            if (n <= 0) continue;
            link[n] = 0;
            struct stat sb;
            if (stat(fd, &sb) == 0 && S_ISBLK(sb.st_mode)) {
                printf("LAB_BLOCK_DESCRIPTOR_FOUND /proc/%s %s\n", p->d_name, link);
                return 40;
            }
            if (host_device(link)) {
                printf("LAB_HOST_DEVICE_DESCRIPTOR_FOUND /proc/%s %s\n", p->d_name, link);
                return 41;
            }
        }
        closedir(fds);
    }
    closedir(proc);
    if (count < 5 || !app_seen) {
        printf("LAB_DESCRIPTOR_AUDIT_MISSING_PROCESSES count=%d app=%d\n", count, app_seen);
        return 42;
    }
    printf("LAB_RUNTIME_DESCRIPTOR_AUDIT_OK processes=%d app_uid=10000\n", count);
    return 0;
}
