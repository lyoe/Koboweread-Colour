/* Low-overhead startup timeline: every INTERVAL ms print uptime, whole-system
 * busy %, and the CPU ticks of each process whose root is ROOT, labelled by
 * its first non-option argument (Java class or program name). Host-side
 * bridge programs whose executable lives under ROOT are included as well.
 *   startup-trace ROOT INTERVAL_MS MAX_SECONDS STOPFILE
 * Stops when STOPFILE exists or after MAX_SECONDS. Read-only.
 */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int read_file(const char *path, char *buf, size_t size) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    size_t n = fread(buf, 1, size - 1, f);
    fclose(f);
    buf[n] = 0;
    return (int)n;
}

static const char *label(int n, char *cmd) {
    const char *best = 0;
    for (char *p = cmd; p < cmd + n; p += strlen(p) + 1) {
        if (!*p || *p == '-') continue;
        const char *base = strrchr(p, '/');
        base = base ? base + 1 : p;
        if (!strcmp(base, "app_process32") || !strcmp(p, "/system/bin")) continue;
        if (!best) best = base;
        else return base == p ? p : base;  /* class name after app_process32 */
    }
    return best ? best : "?";
}

int main(int argc, char **argv) {
    if (argc != 5) return 2;
    const char *root = argv[1];
    int interval = atoi(argv[2]), limit = atoi(argv[3]);
    const char *stopfile = argv[4];
    long long prev_busy = 0, prev_tot = 0, prev_iow = 0;
    char buf[4096], path[64], link[256];
    for (double waited = 0; waited < limit; waited += interval / 1000.0) {
        if (access(stopfile, F_OK) == 0) break;
        long long v[10] = {0}, tot = 0;
        if (read_file("/proc/stat", buf, sizeof buf) > 0)
            sscanf(buf, "cpu %lld %lld %lld %lld %lld %lld %lld %lld", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7]);
        for (int i = 0; i < 8; i++) tot += v[i];
        long long busy = tot - v[3] - v[4];
        char up[32] = "0";
        read_file("/proc/uptime", up, sizeof up);
        *strchr(up, ' ') = 0;
        printf("T %s busy=%lld iowait=%lld", up,
               tot > prev_tot ? 100 * (busy - prev_busy) / (tot - prev_tot) : 0,
               tot > prev_tot ? 100 * (v[4] - prev_iow) / (tot - prev_tot) : 0);
        prev_busy = busy; prev_tot = tot; prev_iow = v[4];
        DIR *d = opendir("/proc");
        struct dirent *e;
        while (d && (e = readdir(d))) {
            if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
            snprintf(path, sizeof path, "/proc/%s/root", e->d_name);
            ssize_t n = readlink(path, link, sizeof link - 1);
            if (n < 0) continue;
            link[n] = 0;
            int host = strcmp(link, root) != 0;
            if (host) {
                snprintf(path, sizeof path, "/proc/%s/exe", e->d_name);
                n = readlink(path, link, sizeof link - 1);
                if (n < 0) continue;
                link[n] = 0;
                size_t rl = strlen(root);
                if (n <= (ssize_t)rl || strncmp(link, root, rl) || link[rl] != '/') continue;
            }
            snprintf(path, sizeof path, "/proc/%s/cmdline", e->d_name);
            int cn = read_file(path, buf, sizeof buf);
            if (cn <= 0) continue;
            char name[128];
            if (host) {
                const char *base = strrchr(link, '/');
                snprintf(name, sizeof name, "host-%s", base ? base + 1 : link);
            } else snprintf(name, sizeof name, "%s", label(cn, buf));
            snprintf(path, sizeof path, "/proc/%s/stat", e->d_name);
            if (read_file(path, buf, sizeof buf) <= 0) continue;
            char *rp = strrchr(buf, ')');
            unsigned long ut = 0, st = 0;
            if (rp) sscanf(rp + 2, "%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu", &ut, &st);
            printf(" %s:%s:%lu", name, e->d_name, ut + st);
        }
        if (d) closedir(d);
        putchar('\n');
        fflush(stdout);
        usleep(interval * 1000);
    }
    return 0;
}
