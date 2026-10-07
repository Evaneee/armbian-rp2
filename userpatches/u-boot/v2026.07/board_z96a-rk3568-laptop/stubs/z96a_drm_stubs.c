/*
 * Weak stubs so Radxa DRM can link on mainline U-Boot without Android/resource/MIPI stack.
 */
#include <common.h>
#include <linux/errno.h>
#include <dm/ofnode.h>
#include <backlight.h>

__attribute__((weak)) int rockchip_read_resource_file(void *buf, const char *name, int offset, int len)
{
	return -ENOENT;
}

__attribute__((weak)) int rockchip_read_resource_file_list(void *buf, const char *name, int offset, int len)
{
	return -ENOENT;
}

__attribute__((weak)) void *rockchip_read_resource_file_list_get(int index)
{
	return NULL;
}

__attribute__((weak)) struct udevice *rockchip_get_bootdev(void)
{
	return NULL;
}

__attribute__((weak)) int of_alias_get_dev(const char *name, int id, struct udevice **devp)
{
	if (devp)
		*devp = NULL;
	return -ENODEV;
}

__attribute__((weak)) int of_property_read_u64(const struct device_node *np, const char *propname, u64 *out)
{
	return -EINVAL;
}

__attribute__((weak)) int backlight_disable(struct udevice *dev)
{
	return backlight_set_brightness(dev, 0);
}

/* Weak stubs for MIPI helpers referenced by rockchip_panel (unused on eDP). */
struct mipi_dsi_device;
struct drm_dsc_picture_parameter_set;

__attribute__((weak)) ssize_t mipi_dsi_compression_mode(struct mipi_dsi_device *dsi, bool enable)
{
	return -ENOSYS;
}

__attribute__((weak)) ssize_t mipi_dsi_picture_parameter_set(struct mipi_dsi_device *dsi,
	const struct drm_dsc_picture_parameter_set *pps)
{
	return -ENOSYS;
}

__attribute__((weak)) ssize_t mipi_dsi_generic_write(struct mipi_dsi_device *dsi, const void *payload, size_t size)
{
	return -ENOSYS;
}

__attribute__((weak)) ssize_t mipi_dsi_dcs_write_buffer(struct mipi_dsi_device *dsi, const void *data, size_t len)
{
	return -ENOSYS;
}

#include <vsprintf.h>

__attribute__((weak)) int atoi(const char *s)
{
	return (int)simple_strtol(s, NULL, 10);
}

/* Pulled in once DRM_ROCKCHIP_VIDEO_FRAMEBUFFER keeps more display paths alive. */
struct drm_display_mode;

__attribute__((weak)) int edid_get_drm_mode(u8 *buf, int buf_size,
					    struct drm_display_mode *mode,
					    int *panel_bits_per_colourp)
{
	return -EINVAL;
}

__attribute__((weak)) int video_bridge_get_timing(struct udevice *dev)
{
	return -ENOSYS;
}
