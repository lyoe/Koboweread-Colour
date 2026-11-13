// SPDX-License-Identifier: Apache-2.0
/*
 * hwcomposer.default for the Kobo Libra 2 software Android stack (HWC 1.1).
 *
 * Without a HWC, SurfaceFlinger composes every frame with software GLES
 * (libagl): about 68 ms of CPU per frame at 1264x1680. This module:
 *  - marks a lone, full-screen, unscaled, untransformed RGBA/RGBX layer
 *    (opaque, or premultiplied over the black background) as HWC_OVERLAY and
 *    copies its buffer straight into the virtual framebuffer, so no GL runs;
 *  - leaves every other frame to GLES and copies the GLES result instead;
 *  - skips the copy and the publish when that lone layer still holds the
 *    buffer it held in the last published frame: while an activity starts,
 *    the window manager makes SurfaceFlinger recompose unchanged layers at
 *    ~30 fps for seconds, and each copy (10 ms) plus the host bridge's
 *    conversion of the published frame took ~35% of the CPU WeRead needed.
 *    A new app frame always arrives in another buffer. Off when
 *    /data/local/tmp/kobo-hwcskip-disabled exists at open.
 *
 * The virtual framebuffer is the file /dev/graphics/fb0 (two halves) plus
 * /lab/virtual-fb.state {magic,w,h,stride,yoffset,sequence}, read by the host
 * display bridge, which only ever reads the half named by yoffset. We always
 * write the other half and publish afterwards, exactly like a page flip.
 * The fast shim (libkobo_fb-hwc.so) reports a single buffer to SurfaceFlinger
 * while this file exists, so gralloc keeps its framebuffer-target buffers in
 * ordinary memory and never touches the two halves. Open refuses to run if
 * gralloc did not end up with a single buffer.
 *
 * Single buffer (2026-11-08; only when /data/local/tmp/kobo-fbsingle-enabled
 * exists at open, which the 1.8.8 session creates unless fast-v188/
 * fbsingle-disabled exists; a bridge that does not know BUSY must never see it): every frame goes into the
 * first half, so the second half of the tmpfs file is never touched and costs
 * no memory (8.3 MB). The bridge may be converting that half while we
 * write, so the state is first republished with BUSY in the sequence word; the
 * bridge skips a busy state and drops a conversion whose state changed while
 * it ran (it already compared the state before and after), then retries.
 *
 * Bionic is the target libc but the headers come from musl, whose time_t and
 * off_t are 64-bit; the few libc calls that take them are declared here with
 * the 32-bit bionic ABI. The code also avoids 64-bit division, integer
 * modulo and 64-bit/float conversions, so no __aeabi_* runtime helpers
 * (LIBC_PRIVATE in bionic) are needed.
 */
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <hardware/gralloc.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>

#include "../libra2-prototype/device-profile.h"

#define W KOBO_FB_WIDTH
#define H KOBO_FB_HEIGHT
#define FRAME_BYTES ((unsigned)W * H * 4)

struct kts { long sec, nsec; };
struct kpollfd { int fd; short events, revents; };
extern int kobo_clock_gettime(int, struct kts *) __asm__("clock_gettime");
extern int kobo_clock_nanosleep(int, int, const struct kts *, struct kts *) __asm__("clock_nanosleep");
extern void *kobo_mmap(void *, unsigned, int, int, int, long) __asm__("mmap");
extern int kobo_poll(struct kpollfd *, unsigned long, int) __asm__("poll");
/* close/read/write/access come from <unistd.h> (via system/window.h). */
extern int open(const char *, int, ...);
extern void *calloc(size_t, size_t);
extern void free(void *);
extern int snprintf(char *, size_t, const char *, ...);
extern int pthread_create(unsigned long *, const void *, void *(*)(void *), void *);

/* gralloc.default private structures (hardware/libhardware/modules/gralloc). */
struct kobo_private_handle {
	native_handle_t base;
	int fd, magic, flags, size, offset;
	uint64_t addr __attribute__((aligned(8)));
	int pid;
};
struct kobo_private_module {
	gralloc_module_t base;
	void *framebuffer;
	uint32_t flags, numBuffers, bufferMask;
};
/* ABI checks against the 32-bit bionic layouts SurfaceFlinger and gralloc use. */
_Static_assert(sizeof(void *) == 4 && sizeof(size_t) == 4, "ARM32 only");
_Static_assert(sizeof(hwc_layer_1_t) == 96, "hwc_layer_1_t is padded to 96 bytes");
_Static_assert(__builtin_offsetof(struct kobo_private_handle, magic) == 16 &&
               __builtin_offsetof(struct kobo_private_handle, size) == 24 &&
               __builtin_offsetof(struct kobo_private_handle, addr) == 32, "private_handle_t layout");
#define GRALLOC_MAGIC 0x3141592
#define PRIV_FLAGS_FRAMEBUFFER 1

struct kobo_hwc {
	hwc_composer_device_1_t dev;
	const hwc_procs_t *procs;
	const gralloc_module_t *gralloc;
	unsigned char *fb;
	int front;            /* half named by the last published state */
	unsigned sequence;
	volatile int vsync_enabled;
	int32_t period_ns;
	unsigned long frames, overlay, gles, skipped, unchanged, since_log;
	const void *last_overlay; /* handle of the last published overlay frame */
	int skip_unchanged;
	int single; /* write only the first half, guarded by the BUSY state */
	unsigned long multi_layer, ineligible; /* why frames stayed on GLES */
	double copy_s;
};

static void say(const char *text)
{
	write(2, text, strlen(text));
}

/* Thread CPU time: on one core, wall time would include preemption by the app. */
static double now_s(void)
{
	struct kts t;
	kobo_clock_gettime(3 /* CLOCK_THREAD_CPUTIME_ID */, &t);
	return (double)t.sec + (double)t.nsec * 1e-9;
}

static void wait_fence(int *fd)
{
	if (*fd >= 0) {
		struct kpollfd p = { *fd, 1 /* POLLIN */, 0 };
		kobo_poll(&p, 1, 3000);
		close(*fd);
		*fd = -1;
	}
}

/* Exact size gralloc.default gives a W x H 32-bit buffer (stride == W). */
static int full_screen_buffer(buffer_handle_t handle)
{
	const struct kobo_private_handle *h = (const void *)handle;
	unsigned expect = ((FRAME_BYTES + 4) + 4095u) & ~4095u;
	return h && h->magic == GRALLOC_MAGIC && !(h->flags & PRIV_FLAGS_FRAMEBUFFER) &&
	       (unsigned)h->size == expect;
}

static int overlay_candidate(const hwc_layer_1_t *l)
{
	/* compositionType persists while geometry is unchanged: re-check either. */
	return (l->compositionType == HWC_FRAMEBUFFER || l->compositionType == HWC_OVERLAY) &&
	       !(l->flags & HWC_SKIP_LAYER) &&
	       l->transform == 0 && l->planeAlpha == 255 &&
	       (l->blending == HWC_BLENDING_NONE || l->blending == HWC_BLENDING_PREMULT) &&
	       l->displayFrame.left == 0 && l->displayFrame.top == 0 &&
	       l->displayFrame.right == W && l->displayFrame.bottom == H &&
	       /* HWC < 1.3: SurfaceFlinger fills the integer crop of the union. */
	       l->sourceCropi.left == 0 && l->sourceCropi.top == 0 &&
	       l->sourceCropi.right == W && l->sourceCropi.bottom == H &&
	       full_screen_buffer(l->handle);
}

static int hwc_prepare(hwc_composer_device_1_t *dev, size_t n, hwc_display_contents_1_t **displays)
{
	if (n < 1 || !displays[0])
		return 0;
	struct kobo_hwc *ctx = (struct kobo_hwc *)dev;
	hwc_display_contents_1_t *c = displays[0];
	/* One layer plus the framebuffer target: let it bypass GLES. */
	if (c->numHwLayers != 2)
		ctx->multi_layer++;
	else if (c->hwLayers[1].compositionType == HWC_FRAMEBUFFER_TARGET && overlay_candidate(&c->hwLayers[0]))
		c->hwLayers[0].compositionType = HWC_OVERLAY;
	else if (c->hwLayers[0].compositionType == HWC_OVERLAY)
		c->hwLayers[0].compositionType = HWC_FRAMEBUFFER;
	if (c->numHwLayers == 2 && c->hwLayers[0].compositionType != HWC_OVERLAY && ++ctx->ineligible <= 3) {
		const hwc_layer_1_t *l = &c->hwLayers[0];
		const struct kobo_private_handle *h = (const void *)l->handle;
		char line[320];
		snprintf(line, sizeof(line),
		         "KOBO_HWC_INELIGIBLE type=%d flags=%#x transform=%u blending=%#x alpha=%u frame=%d,%d,%d,%d crop=%d,%d,%d,%d handle=%d magic=%#x hflags=%#x size=%d\n",
		         l->compositionType, l->flags, l->transform, l->blending, l->planeAlpha,
		         l->displayFrame.left, l->displayFrame.top, l->displayFrame.right, l->displayFrame.bottom,
		         l->sourceCropi.left, l->sourceCropi.top, l->sourceCropi.right, l->sourceCropi.bottom,
		         h != NULL, h ? h->magic : 0, h ? h->flags : 0, h ? h->size : 0);
		say(line);
	}
	return 0;
}

#define STATE_BUSY 0x80000000u

static void write_state(struct kobo_hwc *ctx, int half, uint32_t sequence)
{
	uint32_t state[6] = { 0x4b464230, W, H, W * 4, half ? H : 0, sequence };
	int fd = open("/lab/virtual-fb.state", 1 | 64 | 02000000 /* O_WRONLY|O_CREAT|O_CLOEXEC */, 0600);
	if (fd >= 0) {
		write(fd, state, sizeof(state));
		close(fd);
	}
	ctx->front = half;
}

static void publish(struct kobo_hwc *ctx, int half)
{
	write_state(ctx, half, (++ctx->sequence) & ~STATE_BUSY);
}

static int hwc_set(hwc_composer_device_1_t *dev, size_t n, hwc_display_contents_1_t **displays)
{
	struct kobo_hwc *ctx = (struct kobo_hwc *)dev;
	for (size_t d = 0; d < n; d++) {
		hwc_display_contents_1_t *c = displays[d];
		if (!c)
			continue;
		hwc_layer_1_t *source = NULL;
		if (d == 0) {
			for (size_t i = 0; i < c->numHwLayers; i++)
				if (c->hwLayers[i].compositionType == HWC_OVERLAY)
					source = &c->hwLayers[i];
			if (!source && c->numHwLayers &&
			    c->hwLayers[c->numHwLayers - 1].compositionType == HWC_FRAMEBUFFER_TARGET)
				source = &c->hwLayers[c->numHwLayers - 1];
		}
		if (source && source->handle && source->compositionType == HWC_OVERLAY &&
		    ctx->skip_unchanged && source->handle == ctx->last_overlay) {
			ctx->unchanged++;
		} else if (source && source->handle) {
			wait_fence(&source->acquireFenceFd);
			void *pixels = NULL;
			if (full_screen_buffer(source->handle) &&
			    ctx->gralloc->lock(ctx->gralloc, source->handle, GRALLOC_USAGE_SW_READ_OFTEN,
			                       0, 0, W, H, &pixels) == 0 && pixels) {
				double begin = now_s();
				int back = ctx->single ? 0 : !ctx->front;
				if (ctx->single)
					write_state(ctx, 0, ((ctx->sequence + 1) & ~STATE_BUSY) | STATE_BUSY);
				memcpy(ctx->fb + (unsigned)back * FRAME_BYTES, pixels, FRAME_BYTES);
				ctx->gralloc->unlock(ctx->gralloc, source->handle);
				publish(ctx, back);
				ctx->last_overlay = source->compositionType == HWC_OVERLAY ? (const void *)source->handle : NULL;
				ctx->copy_s += now_s() - begin;
				ctx->frames++;
				if (source->compositionType == HWC_OVERLAY)
					ctx->overlay++;
				else
					ctx->gles++;
			} else {
				ctx->skipped++;
			}
		}
		for (size_t i = 0; i < c->numHwLayers; i++) {
			wait_fence(&c->hwLayers[i].acquireFenceFd);
			c->hwLayers[i].releaseFenceFd = -1;
		}
		c->retireFenceFd = -1;
	}
	if (ctx->frames && ++ctx->since_log >= 20) {
		char line[160];
		ctx->since_log = 0;
		snprintf(line, sizeof(line), "KOBO_HWC frames=%lu overlay=%lu gles=%lu (multi_layer=%lu ineligible=%lu) skipped=%lu unchanged=%lu copy_cpu_ms_avg=%.1f\n",
		         ctx->frames, ctx->overlay, ctx->gles, ctx->multi_layer, ctx->ineligible, ctx->skipped, ctx->unchanged,
		         ctx->copy_s * 1000.0 / (double)ctx->frames);
		say(line);
	}
	return 0;
}

static int hwc_event_control(hwc_composer_device_1_t *dev, int disp, int event, int enabled)
{
	struct kobo_hwc *ctx = (struct kobo_hwc *)dev;
	if (disp != HWC_DISPLAY_PRIMARY || event != HWC_EVENT_VSYNC)
		return -EINVAL;
	ctx->vsync_enabled = enabled != 0;
	return 0;
}

static int hwc_blank(hwc_composer_device_1_t *dev, int disp, int blank)
{
	(void)dev; (void)blank;
	return disp == HWC_DISPLAY_PRIMARY ? 0 : -EINVAL;
}

static int hwc_query(hwc_composer_device_1_t *dev, int what, int *value)
{
	struct kobo_hwc *ctx = (struct kobo_hwc *)dev;
	switch (what) {
	case HWC_BACKGROUND_LAYER_SUPPORTED: *value = 0; return 0;
	case HWC_VSYNC_PERIOD: *value = ctx->period_ns; return 0;
	case HWC_DISPLAY_TYPES_SUPPORTED: *value = HWC_DISPLAY_PRIMARY_BIT; return 0;
	default: return -EINVAL;
	}
}

static void hwc_register_procs(hwc_composer_device_1_t *dev, hwc_procs_t const *procs)
{
	((struct kobo_hwc *)dev)->procs = procs;
}

static void hwc_dump(hwc_composer_device_1_t *dev, char *buff, int len)
{
	struct kobo_hwc *ctx = (struct kobo_hwc *)dev;
	if (len > 0)
		snprintf(buff, (size_t)len, "kobo hwc: frames=%lu overlay=%lu gles=%lu skipped=%lu\n",
		         ctx->frames, ctx->overlay, ctx->gles, ctx->skipped);
}

static int hwc_get_configs(hwc_composer_device_1_t *dev, int disp, uint32_t *configs, size_t *num)
{
	(void)dev;
	if (disp != HWC_DISPLAY_PRIMARY)
		return -EINVAL;
	if (*num > 0)
		configs[0] = 0;
	*num = 1;
	return 0;
}

static int hwc_get_attributes(hwc_composer_device_1_t *dev, int disp, uint32_t config,
                              const uint32_t *attributes, int32_t *values)
{
	struct kobo_hwc *ctx = (struct kobo_hwc *)dev;
	if (disp != HWC_DISPLAY_PRIMARY || config != 0)
		return -EINVAL;
	for (int i = 0; attributes[i] != HWC_DISPLAY_NO_ATTRIBUTE; i++) {
		switch (attributes[i]) {
		case HWC_DISPLAY_VSYNC_PERIOD: values[i] = ctx->period_ns; break;
		case HWC_DISPLAY_WIDTH: values[i] = W; break;
		case HWC_DISPLAY_HEIGHT: values[i] = H; break;
		/* Same as gralloc's fb HAL: pixels * 25.4 / millimetres, in dpi * 1000. */
		case HWC_DISPLAY_DPI_X: values[i] = (int32_t)(W * 25.4f / KOBO_FB_WIDTH_MM * 1000.0f); break;
		case HWC_DISPLAY_DPI_Y: values[i] = (int32_t)(H * 25.4f / KOBO_FB_HEIGHT_MM * 1000.0f); break;
		default: return -EINVAL; /* pre-1.5: SurfaceFlinger retries without colour transform */
		}
	}
	return 0;
}

static void *vsync_thread(void *arg)
{
	struct kobo_hwc *ctx = arg;
	struct kts t;
	kobo_clock_gettime(1, &t);
	for (;;) {
		t.nsec += ctx->period_ns;
		while (t.nsec >= 1000000000L) {
			t.nsec -= 1000000000L;
			t.sec++;
		}
		while (kobo_clock_nanosleep(1, 1 /* TIMER_ABSTIME */, &t, NULL) == EINTR)
			;
		if (ctx->vsync_enabled && ctx->procs && ctx->procs->vsync)
			ctx->procs->vsync(ctx->procs, HWC_DISPLAY_PRIMARY, (int64_t)t.sec * 1000000000LL + t.nsec);
	}
	return NULL;
}

static int hwc_close(hw_device_t *dev)
{
	(void)dev; /* SurfaceFlinger keeps the HWC for its whole life. */
	return 0;
}

static int hwc_open(const hw_module_t *module, const char *name, hw_device_t **device)
{
	if (strcmp(name, HWC_HARDWARE_COMPOSER))
		return -EINVAL;
	const hw_module_t *gm;
	if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &gm))
		return -ENODEV;
	const struct kobo_private_module *pm = (const void *)gm;
	if (pm->numBuffers != 1) {
		say("KOBO_HWC_REFUSED gralloc is page-flipping; keeping the GLES framebuffer path\n");
		return -ENODEV;
	}
	int fd = open("/dev/graphics/fb0", 2 /* O_RDWR */ | 02000000);
	if (fd < 0)
		return -ENODEV;
	unsigned char *fb = kobo_mmap(NULL, FRAME_BYTES * 2, 3 /* PROT_READ|WRITE */, 1 /* MAP_SHARED */, fd, 0);
	close(fd);
	if (fb == (void *)-1)
		return -ENODEV;
	struct kobo_hwc *ctx = calloc(1, sizeof(*ctx));
	if (!ctx)
		return -ENOMEM;
	ctx->gralloc = (const gralloc_module_t *)gm;
	ctx->skip_unchanged = access("/data/local/tmp/kobo-hwcskip-disabled", 0) != 0;
	ctx->fb = fb;
	ctx->single = access("/data/local/tmp/kobo-fbsingle-enabled", 0) == 0;
	/* Continue from the state gralloc's initial FBIOPUT published. */
	uint32_t state[6] = { 0 };
	int sfd = open("/lab/virtual-fb.state", 02000000);
	if (sfd >= 0) {
		if (read(sfd, state, sizeof(state)) == (ssize_t)sizeof(state) && state[0] == 0x4b464230) {
			ctx->front = state[4] == H;
			ctx->sequence = state[5];
		}
		close(sfd);
	}
	/* Refresh rate as gralloc derives it from the shim's pixel clock. */
	unsigned pixclock = access("/lab/display-cadence15-enabled", 0) == 0 ? KOBO_FB_PIXCLOCK_30 * 2 : KOBO_FB_PIXCLOCK_30;
	double millihertz = 1e15 / ((double)W * H * pixclock);
	ctx->period_ns = (int32_t)(1e12 / millihertz);

	ctx->dev.common.tag = HARDWARE_DEVICE_TAG;
	ctx->dev.common.version = HWC_DEVICE_API_VERSION_1_1;
	ctx->dev.common.module = (hw_module_t *)module;
	ctx->dev.common.close = hwc_close;
	ctx->dev.prepare = hwc_prepare;
	ctx->dev.set = hwc_set;
	ctx->dev.eventControl = hwc_event_control;
	ctx->dev.blank = hwc_blank;
	ctx->dev.query = hwc_query;
	ctx->dev.registerProcs = hwc_register_procs;
	ctx->dev.dump = hwc_dump;
	ctx->dev.getDisplayConfigs = hwc_get_configs;
	ctx->dev.getDisplayAttributes = hwc_get_attributes;
	unsigned long tid;
	if (pthread_create(&tid, NULL, vsync_thread, ctx)) {
		free(ctx);
		return -ENOMEM;
	}
	char line[128];
	snprintf(line, sizeof(line), "KOBO_HWC_OPENED version=1.1 vsync_period_ns=%d front=%d single=%d\n",
	         (int)ctx->period_ns, ctx->front, ctx->single);
	say(line);
	*device = &ctx->dev.common;
	return 0;
}

static struct hw_module_methods_t hwc_methods = { .open = hwc_open };

__attribute__((visibility("default"))) hwc_module_t HAL_MODULE_INFO_SYM = {
	.common = {
		.tag = HARDWARE_MODULE_TAG,
		.module_api_version = HWC_MODULE_API_VERSION_0_1,
		.hal_api_version = HARDWARE_HAL_API_VERSION,
		.id = HWC_HARDWARE_MODULE_ID,
		.name = "Kobo software framebuffer HWC",
		.author = "kobo-android fast entry",
		.methods = &hwc_methods,
	},
};
