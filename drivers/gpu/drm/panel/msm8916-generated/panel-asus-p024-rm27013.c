// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2022 FIXME
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2013, The Linux Foundation. All rights reserved. (FIXME)

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct rm27013_800x1280 {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct gpio_desc *reset_gpio;
	bool prepared;
};

static inline
struct rm27013_800x1280 *to_rm27013_800x1280(struct drm_panel *panel)
{
	return container_of(panel, struct rm27013_800x1280, panel);
}

#define dsi_generic_write_seq(dsi, seq...) do {				\
		static const u8 d[] = { seq };				\
		int ret;						\
		ret = mipi_dsi_generic_write(dsi, d, ARRAY_SIZE(d));	\
		if (ret < 0)						\
			return ret;					\
	} while (0)

static void rm27013_800x1280_reset(struct rm27013_800x1280 *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(5000, 6000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(2000, 3000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(20);
}

static int rm27013_800x1280_on(struct rm27013_800x1280 *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	dsi_generic_write_seq(dsi, 0x50, 0x77);
	dsi_generic_write_seq(dsi, 0xe1, 0x66);
	dsi_generic_write_seq(dsi, 0xdc, 0x67);
	dsi_generic_write_seq(dsi, 0x50, 0x00);
	dsi_generic_write_seq(dsi, 0x35, 0x00);
	dsi_generic_write_seq(dsi, 0x58, 0x99);
	dsi_generic_write_seq(dsi, 0xcd, 0x4d);
	dsi_generic_write_seq(dsi, 0x58, 0x00);
	dsi_generic_write_seq(dsi, 0xf0, 0x5a, 0x5a);
	dsi_generic_write_seq(dsi, 0xc3, 0x40, 0x00, 0x28);
	usleep_range(6000, 7000);
	dsi_generic_write_seq(dsi, 0x11, 0x00);
	dsi_generic_write_seq(dsi, 0x29, 0x00);

	return 0;
}

static int rm27013_800x1280_off(struct rm27013_800x1280 *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	dsi_generic_write_seq(dsi, 0x28, 0x00);
	dsi_generic_write_seq(dsi, 0x10, 0x00);
	usleep_range(5000, 6000);
	dsi_generic_write_seq(dsi, 0xf0, 0x5a, 0x5a);
	dsi_generic_write_seq(dsi, 0xc3, 0x40, 0x00, 0x20);

	return 0;
}

static int rm27013_800x1280_prepare(struct drm_panel *panel)
{
	struct rm27013_800x1280 *ctx = to_rm27013_800x1280(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (ctx->prepared)
		return 0;

	rm27013_800x1280_reset(ctx);

	ret = rm27013_800x1280_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		return ret;
	}

	ctx->prepared = true;
	return 0;
}

static int rm27013_800x1280_unprepare(struct drm_panel *panel)
{
	struct rm27013_800x1280 *ctx = to_rm27013_800x1280(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = rm27013_800x1280_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);

	ctx->prepared = false;
	return 0;
}

static const struct drm_display_mode rm27013_800x1280_mode = {
	.clock = (800 + 100 + 24 + 80) * (1280 + 24 + 10 + 24) * 60 / 1000,
	.hdisplay = 800,
	.hsync_start = 800 + 100,
	.hsync_end = 800 + 100 + 24,
	.htotal = 800 + 100 + 24 + 80,
	.vdisplay = 1280,
	.vsync_start = 1280 + 24,
	.vsync_end = 1280 + 24 + 10,
	.vtotal = 1280 + 24 + 10 + 24,
	.width_mm = 107,
	.height_mm = 172,
};

static int rm27013_800x1280_get_modes(struct drm_panel *panel,
				      struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &rm27013_800x1280_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs rm27013_800x1280_panel_funcs = {
	.prepare = rm27013_800x1280_prepare,
	.unprepare = rm27013_800x1280_unprepare,
	.get_modes = rm27013_800x1280_get_modes,
};

static int rm27013_800x1280_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct rm27013_800x1280 *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "Failed to get reset-gpios\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_HSE |
			  MIPI_DSI_MODE_NO_EOT_PACKET |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS;

	drm_panel_init(&ctx->panel, dev, &rm27013_800x1280_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);

	ret = drm_panel_of_backlight(&ctx->panel);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to get backlight\n");

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach to DSI host: %d\n", ret);
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	return 0;
}

static int rm27013_800x1280_remove(struct mipi_dsi_device *dsi)
{
	struct rm27013_800x1280 *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id rm27013_800x1280_of_match[] = {
	{ .compatible = "asus,p024-rm27013" }, // FIXME
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rm27013_800x1280_of_match);

static struct mipi_dsi_driver rm27013_800x1280_driver = {
	.probe = rm27013_800x1280_probe,
	.remove = rm27013_800x1280_remove,
	.driver = {
		.name = "panel-rm27013-800x1280",
		.of_match_table = rm27013_800x1280_of_match,
	},
};
module_mipi_dsi_driver(rm27013_800x1280_driver);

MODULE_AUTHOR("linux-mdss-dsi-panel-driver-generator <fix@me>"); // FIXME
MODULE_DESCRIPTION("DRM driver for rm27013 800x1280 video mode dsi panel");
MODULE_LICENSE("GPL v2");
