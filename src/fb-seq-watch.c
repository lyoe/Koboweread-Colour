/* Log when Android publishes a frame to the virtual framebuffer.
 *   fb-seq-watch STATE SECONDS
 * Every 5 ms reads the 24-byte state file (magic, width, height, stride,
 * buffer, sequence) and prints CLOCK_MONOTONIC time, buffer and sequence
 * whenever the sequence changes; same clock as the display bridge and the
 * touch forwarder logs. Holds STATE open (inside the image): stop it before
 * the session ends. Read-only.
 */
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}

int main(int argc, char **argv) {
    if (argc != 3) return 2;
    int fd = open(argv[1], O_RDONLY | O_CLOEXEC);
    if (fd < 0) return 3;
    double end = now() + atoi(argv[2]);
    setvbuf(stdout, NULL, _IOLBF, 0);
    uint32_t s[6], last = 0;
    int first = 1;
    struct timespec tick = {0, 5000000};
    while (now() < end) {
        if (pread(fd, s, sizeof s, 0) == (ssize_t)sizeof s && s[0] == 0x4b464230 && (first || s[5] != last)) {
            printf("FB_SEQ t=%.3f buffer=%u sequence=%u\n", now(), s[4], s[5]);
            last = s[5]; first = 0;
        }
        nanosleep(&tick, NULL);
    }
    close(fd);
    return 0;
}
