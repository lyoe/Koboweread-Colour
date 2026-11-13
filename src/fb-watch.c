/* Sample the Android virtual framebuffer and log where dark pixels are.
 *   fb-watch FB WIDTH HEIGHT STRIDE SECONDS LOG
 * Every second, for each of the two buffers, sample an 8-pixel grid, and when
 * the sampled content changed print uptime, buffer, the dark-pixel bounding
 * box and count, plus LOG's size so frames can be matched to log events.
 * No pixels are saved. Read-only.
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc != 7) return 2;
    int w = atoi(argv[2]), h = atoi(argv[3]), stride = atoi(argv[4]), seconds = atoi(argv[5]);
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) return 3;
    size_t len = (size_t)stride * h * 2;
    const unsigned char *fb = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
    if (fb == MAP_FAILED) return 4;
    setvbuf(stdout, NULL, _IOLBF, 0);
    unsigned last[2] = {0, 0};
    for (int t = 0; t < seconds; t++) {
        for (int b = 0; b < 2; b++) {
            int x0 = w, x1 = -1, y0 = h, y1 = -1, n = 0;
            unsigned hash = 2166136261u;
            for (int y = 0; y < h; y += 8) {
                const unsigned char *row = fb + ((size_t)b * h + y) * stride;
                for (int x = 0; x < w; x += 8) {
                    unsigned char g = row[x * 4 + 1];
                    hash = (hash ^ g) * 16777619u;
                    if (g < 128) {
                        n++;
                        if (x < x0) x0 = x;
                        if (x > x1) x1 = x;
                        if (y < y0) y0 = y;
                        if (y > y1) y1 = y;
                    }
                }
            }
            if (hash == last[b]) continue;
            last[b] = hash;
            char up[32] = "0";
            int u = open("/proc/uptime", O_RDONLY);
            if (u >= 0) { ssize_t r = read(u, up, sizeof up - 1); if (r > 0) up[r] = 0; close(u); }
            for (char *p = up; *p; p++) if (*p == ' ') { *p = 0; break; }
            int quarter = n > 20 && x1 - x0 < w * 6 / 10 && y1 - y0 < h * 6 / 10;
            struct stat st;
            long long logsize = stat(argv[6], &st) == 0 ? (long long)st.st_size : -1;
            printf("FB %s buf=%d ink=%d bbox=%d,%d-%d,%d log=%lld%s\n", up, b, n, x0, y0, x1, y1, logsize,
                   quarter ? " SMALL_CONTENT" : "");
        }
        sleep(1);
    }
    return 0;
}
