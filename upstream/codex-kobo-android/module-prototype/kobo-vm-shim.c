// Only this explicitly preloaded Android process uses the fallback.
// Declarations are for the 32-bit ARM Android/Bionic ABI.
typedef unsigned int uint32_t;
typedef unsigned long size_t;
extern long syscall(long, ...);
extern int open(const char *, int, ...);
extern int close(int);
extern int ioctl(int, unsigned long, ...);
extern int *__errno(void);
struct kobo_request { uint32_t start, length, advice; };
int madvise(void *address, size_t length, int advice)
{
 int before = *__errno();
 int rc = (int)syscall(220L, address, length, advice);
 if (rc == 0 || *__errno() != 38 || (advice != 4 && (advice < 0 || advice > 2))) return rc;
 struct kobo_request req = {(uint32_t)address, (uint32_t)length, (uint32_t)advice};
 int fd = open("/dev/kobo_madvise", 2 | 02000000);
 if (fd < 0) return -1;
 rc = ioctl(fd, advice == 4 ? 0x40084b41UL : 0x400c4b42UL, &req);
 int error = *__errno();
 close(fd);
 *__errno() = rc == 0 ? before : error;
 return rc;
}
