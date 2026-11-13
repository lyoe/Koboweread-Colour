/* Page cache of a loop-mounted image's backing file.
 *
 * Pages read through ext4-on-loop are cached twice: once in the ext4 files'
 * own page cache, once in the backing file's page cache on vfat. The second
 * copy is redundant while the first exists and halves the useful cache.
 *
 *   loop-cache count FILE                 print cached KiB of FILE
 *   loop-cache drop FILE INTERVAL SECONDS OWNER-PID [DEVICE]
 *       every INTERVAL seconds ask the kernel to drop clean cached pages of
 *       FILE, for at most SECONDS (31 days), stopping early once OWNER-PID
 *       is gone.
 *       With DEVICE (the kobo_pagecache module's node) its ioctl is used;
 *       otherwise POSIX_FADV_DONTNEED, which this Kobo kernel lacks (ENOSYS).
 *       Dirty, writeback, mapped and locked pages are always left alone.
 *
 * Advisory only: no data is modified and the loop driver re-reads anything it
 * needs from storage.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define KOBO_PAGECACHE_DROP _IOW('k', 0x50, int)

static volatile sig_atomic_t stop;
static void on_signal(int sig) { (void)sig; stop = 1; }

static long long cached_kib(int fd) {
    struct stat st;
    if (fstat(fd, &st) != 0) return -1;
    long page = sysconf(_SC_PAGESIZE);
    const off_t chunk = 64LL << 20;
    unsigned char *vec = malloc(chunk / page);
    if (!vec) return -1;
    long long pages = 0;
    for (off_t off = 0; off < st.st_size; off += chunk) {
        size_t len = st.st_size - off < chunk ? (size_t)(st.st_size - off) : (size_t)chunk;
        void *map = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, off);
        if (map == MAP_FAILED) { free(vec); return -1; }
        if (mincore(map, len, vec) == 0)
            for (size_t i = 0; i < (len + page - 1) / page; i++) pages += vec[i] & 1;
        munmap(map, len);
    }
    free(vec);
    return pages * page / 1024;
}

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main(int argc, char **argv) {
    if (argc < 3) return 2;
    int fd = open(argv[2], O_RDONLY | O_CLOEXEC);
    if (fd < 0) { perror("open"); return 3; }
    if (!strcmp(argv[1], "count") && argc == 3) {
        long long kib = cached_kib(fd);
        if (kib < 0) return 4;
        printf("LOOP_CACHE_KIB=%lld\n", kib);
        return 0;
    }
    if (strcmp(argv[1], "drop") || (argc != 6 && argc != 7)) return 2;
    int interval = atoi(argv[3]), limit = atoi(argv[4]);
    pid_t owner = (pid_t)atoi(argv[5]);
    int dev = argc == 7 ? open(argv[6], O_RDWR | O_CLOEXEC) : -1;
    if (argc == 7 && dev < 0) { perror("open device"); return 3; }
    if (interval < 1 || interval > 60 || limit < 1 || limit > 2678400) return 2;  /* up to 31 days (resident) */
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);
    signal(SIGHUP, on_signal);
    double start = now();
    long rounds = 0;
    long long dropped = 0;
    long long before = cached_kib(fd);
    while (!stop && now() - start < limit) {
        if (owner > 0 && kill(owner, 0) != 0 && errno == ESRCH) break;
        if (dev >= 0) {
            long n = ioctl(dev, KOBO_PAGECACHE_DROP, fd);
            if (n < 0 && rounds == 0) perror("LOOP_CACHE_IOCTL_ERROR");
            if (n > 0) dropped += n;
        } else {
            int rc = posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
            if (rc != 0 && rounds == 0) fprintf(stderr, "LOOP_CACHE_FADVISE_ERROR=%d\n", rc);
        }
        rounds++;
        sleep(interval);
    }
    printf("LOOP_CACHE_DROP rounds=%ld dropped_kib=%lld before_kib=%lld after_kib=%lld seconds=%.1f\n",
           rounds, dropped * (sysconf(_SC_PAGESIZE) / 1024), before, cached_kib(fd), now() - start);
    return 0;
}
