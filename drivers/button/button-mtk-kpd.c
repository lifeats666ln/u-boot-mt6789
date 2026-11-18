/*
 * Copyright (C) 2026 MillenniumOSS
 * SPDX-License-Identifier: GPL-2.0+
 */
#include <asm/io.h>
#include <button.h>
#include <dm.h>
#include <dm/lists.h>
#include <linux/bitops.h>
#include <linux/input.h>

#define KP_MEM1		0x4

struct mtk_pwrap_data {
	u32 acc;
	u32 rdata;
	u32 vld_clr;
	u32 sta;
	u32 topstatus;
};

struct mtk_button_priv {
	ulong reg;
	u32 key;
	u32 code;
	const struct mtk_pwrap_data *pwrap;
};

static u32 mtk_pwrap_read(ulong base, const struct mtk_pwrap_data *pwrap, u32 addr){
	u32 val;
	int i;

	for (i = 0; i < 100000; i++)
		if (((readl(base + pwrap->sta) >> 1) & 0x7) == 0)
			break;

	writel(addr, base + pwrap->acc);

	for (i = 0; i < 100000; i++)
		if (((readl(base + pwrap->sta) >> 1) & 0x7) == 0x6)
			break;

	if (((readl(base + pwrap->sta) >> 1) & 0x7) != 0x6)
		return ~0;

	val = readl(base + pwrap->rdata);
	writel(1, base + pwrap->vld_clr);

	return val;
}

static enum button_state_t mtk_button_get_state(struct udevice *dev){
	struct mtk_button_priv *priv = dev_get_priv(dev);
	u32 val;

	if (priv->pwrap) {
		val = mtk_pwrap_read(priv->reg, priv->pwrap,
				     priv->pwrap->topstatus);
		return (val & BIT(priv->key)) ? BUTTON_OFF : BUTTON_ON;
	}

	val = readl(priv->reg + KP_MEM1 + 4 * (priv->key / 16));
	return (val & BIT(priv->key % 16)) ? BUTTON_OFF : BUTTON_ON;
}

static int mtk_button_get_code(struct udevice *dev){
	struct mtk_button_priv *priv = dev_get_priv(dev);

	return priv->code;
}

static int mtk_button_probe(struct udevice *dev){
	struct button_uc_plat *uc_plat = dev_get_uclass_plat(dev);
	struct mtk_button_priv *priv = dev_get_priv(dev);
	int ret;

	if (!uc_plat->label)
		return 0;

	priv->reg = dev_read_addr(dev->parent);
	if (priv->reg == FDT_ADDR_T_NONE)
		return -EINVAL;

	ret = dev_read_u32(dev, "linux,code", &priv->code);
	if (ret)
		return ret;

	priv->pwrap = (void *)dev_get_driver_data(dev->parent);
	if (priv->pwrap)
		return dev_read_u32(dev, "mediatek,pmic-key", &priv->key);

	return dev_read_u32(dev, "mediatek,kp-key", &priv->key);
}

static int mtk_button_bind(struct udevice *parent){
	struct udevice *dev;
	ofnode node;
	int ret;

	dev_for_each_subnode(node, parent) {
		struct button_uc_plat *uc_plat;
		const char *label = ofnode_read_string(node, "label");

		if (!label)
			continue;

		ret = device_bind_driver_to_node(parent, "mtk_button",
						 ofnode_get_name(node),
						 node, &dev);
		if (ret)
			return ret;

		uc_plat = dev_get_uclass_plat(dev);
		uc_plat->label = label;
	}

	return 0;
}

static const struct button_ops mtk_button_ops = {
	.get_state	= mtk_button_get_state,
	.get_code	= mtk_button_get_code,
};

U_BOOT_DRIVER(mtk_button) = {
	.name		= "mtk_button",
	.id		= UCLASS_BUTTON,
	.ops		= &mtk_button_ops,
	.priv_auto	= sizeof(struct mtk_button_priv),
	.probe		= mtk_button_probe,
};

static const struct udevice_id mtk_kpd_ids[] = {
	{ .compatible = "mediatek,kp" },
	{ .compatible = "mediatek,mtk-kpd" },
	{ }
};

U_BOOT_DRIVER(mtk_kpd) = {
	.name		= "mtk_kpd",
	.id		= UCLASS_BUTTON,
	.of_match	= mtk_kpd_ids,
	.bind		= mtk_button_bind,
};

static const struct mtk_pwrap_data mt6789_pwrap_vals = {
	.acc		= 0x880,
	.rdata		= 0x894,
	.vld_clr	= 0x8a4,
	.sta		= 0x8a8,
	.topstatus	= 0x28,
};

static const struct udevice_id mtk_pwrap_keys_ids[] = {
	{ .compatible = "mediatek,mt6789-pwrap-keys", .data = (ulong)&mt6789_pwrap_vals },
	{ }
};

U_BOOT_DRIVER(mtk_pwrap_keys) = {
	.name		= "mtk_pwrap_keys",
	.id		= UCLASS_BUTTON,
	.of_match	= mtk_pwrap_keys_ids,
	.bind		= mtk_button_bind,
};
