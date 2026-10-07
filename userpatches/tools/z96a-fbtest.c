/* Tiny aarch64 fb pattern drawer for Z96A bring-up initramfs. */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/fb.h>

static void fill(uint32_t *fb, int w, int h, int stride_px, uint32_t c)
{
	for (int y = 0; y < h; y++) {
		uint32_t *row = fb + y * stride_px;
		for (int x = 0; x < w; x++)
			row[x] = c;
	}
}

static void bars(uint32_t *fb, int w, int h, int stride_px)
{
	/* XRGB8888 color bars */
	const uint32_t cols[] = {
		0x00FF0000, /* R */
		0x0000FF00, /* G */
		0x000000FF, /* B */
		0x00FFFFFF, /* W */
		0x00000000, /* K */
		0x00FFFF00, /* Y */
		0x0000FFFF, /* C */
		0x00FF00FF, /* M */
	};
	int n = (int)(sizeof(cols) / sizeof(cols[0]));
	int bw = w / n;
	if (bw < 1)
		bw = 1;

	for (int y = 0; y < h; y++) {
		uint32_t *row = fb + y * stride_px;
		for (int x = 0; x < w; x++)
			row[x] = cols[(x / bw) % n];
	}
}

int main(int argc, char **argv)
{
	const char *path = "/dev/fb0";
	const char *mode = "bars";
	int fd;
	struct fb_var_screeninfo v;
	struct fb_fix_screeninfo f;
	size_t len;
	void *map;
	uint32_t *fb;
	int stride_px;

	if (argc > 1)
		mode = argv[1];
	if (argc > 2)
		path = argv[2];

	fd = open(path, O_RDWR);
	if (fd < 0) {
		perror(path);
		return 1;
	}
	if (ioctl(fd, FBIOGET_VSCREENINFO, &v) ||
	    ioctl(fd, FBIOGET_FSCREENINFO, &f)) {
		perror("ioctl");
		return 1;
	}

	printf("fb %s %ux%u bpp=%u line_len=%u\n",
	       path, v.xres, v.yres, v.bits_per_pixel, f.line_length);

	if (v.bits_per_pixel != 32) {
		fprintf(stderr, "need 32bpp, got %u\n", v.bits_per_pixel);
		return 1;
	}

	len = (size_t)f.line_length * v.yres_virtual;
	if (len < (size_t)f.line_length * v.yres)
		len = (size_t)f.line_length * v.yres;

	map = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (map == MAP_FAILED) {
		perror("mmap");
		return 1;
	}

	fb = map;
	stride_px = (int)(f.line_length / 4);

	if (!strcmp(mode, "black"))
		fill(fb, (int)v.xres, (int)v.yres, stride_px, 0x00000000);
	else if (!strcmp(mode, "white"))
		fill(fb, (int)v.xres, (int)v.yres, stride_px, 0x00FFFFFF);
	else if (!strcmp(mode, "red"))
		fill(fb, (int)v.xres, (int)v.yres, stride_px, 0x00FF0000);
	else if (!strcmp(mode, "green"))
		fill(fb, (int)v.xres, (int)v.yres, stride_px, 0x0000FF00);
	else if (!strcmp(mode, "blue"))
		fill(fb, (int)v.xres, (int)v.yres, stride_px, 0x000000FF);
	else
		bars(fb, (int)v.xres, (int)v.yres, stride_px);

	msync(map, len, MS_SYNC);
	munmap(map, len);
	close(fd);
	printf("drew %s\n", mode);
	return 0;
}
