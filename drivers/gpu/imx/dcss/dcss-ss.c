/*
 * Copyright (C) 2017 NXP
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include <linux/device.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <video/videomode.h>

#include <video/imx-dcss.h>
#include "dcss-prv.h"

#define USE_CTXLD

#define DCSS_SS_SYS_CTRL			0x00
#define   RUN_EN				BIT(0)
#define DCSS_SS_DISPLAY				0x10
#define   LRC_X_POS				0
#define   LRC_X_MASK				GENMASK(12, 0)
#define   LRC_Y_POS				16
#define   LRC_Y_MASK				GENMASK(28, 16)
#define DCSS_SS_HSYNC				0x20
#define DCSS_SS_VSYNC				0x30
#define   SYNC_START_POS			0
#define   SYNC_START_MASK			GENMASK(12, 0)
#define   SYNC_END_POS				16
#define   SYNC_END_MASK				GENMASK(28, 16)
#define   SYNC_POL				BIT(31)
#define DCSS_SS_DE_ULC				0x40
#define   ULC_X_POS				0
#define   ULC_X_MASK				GENMASK(12, 0)
#define   ULC_Y_POS				16
#define   ULC_Y_MASK				GENMASK(28, 16)
#define   ULC_POL				BIT(31)
#define DCSS_SS_DE_LRC				0x50
#define DCSS_SS_MODE				0x60
#define   PIPE_MODE_POS				0
#define   PIPE_MODE_MASK			GENMASK(1, 0)
#define DCSS_SS_COEFF				0x70
#define   HORIZ_A_POS				0
#define   HORIZ_A_MASK				GENMASK(3, 0)
#define   HORIZ_B_POS				4
#define   HORIZ_B_MASK				GENMASK(7, 4)
#define   HORIZ_C_POS				8
#define   HORIZ_C_MASK				GENMASK(11, 8)
#define   HORIZ_H_NORM_POS			12
#define   HORIZ_H_NORM_MASK			GENMASK(14, 12)
#define   VERT_A_POS				16
#define   VERT_A_MASK				GENMASK(19, 16)
#define   VERT_B_POS				20
#define   VERT_B_MASK				GENMASK(23, 20)
#define   VERT_C_POS				24
#define   VERT_C_MASK				GENMASK(27, 24)
#define   VERT_H_NORM_POS			28
#define   VERT_H_NORM_MASK			GENMASK(30, 28)
#define DCSS_SS_CLIP_CB				0x80
#define DCSS_SS_CLIP_CR				0x90
#define   CLIP_MIN_POS				0
#define   CLIP_MIN_MASK				GENMASK(9, 0)
#define   CLIP_MAX_POS				0
#define   CLIP_MAX_MASK				GENMASK(23, 16)
#define DCSS_SS_INTER_MODE			0xA0
#define   INT_EN				BIT(0)
#define   VSYNC_SHIFT				BIT(1)

static struct dcss_debug_reg ss_debug_reg[] = {
	DCSS_DBG_REG(DCSS_SS_SYS_CTRL),
	DCSS_DBG_REG(DCSS_SS_DISPLAY),
	DCSS_DBG_REG(DCSS_SS_HSYNC),
	DCSS_DBG_REG(DCSS_SS_VSYNC),
	DCSS_DBG_REG(DCSS_SS_DE_ULC),
	DCSS_DBG_REG(DCSS_SS_DE_LRC),
	DCSS_DBG_REG(DCSS_SS_MODE),
	DCSS_DBG_REG(DCSS_SS_COEFF),
	DCSS_DBG_REG(DCSS_SS_CLIP_CB),
	DCSS_DBG_REG(DCSS_SS_CLIP_CR),
	DCSS_DBG_REG(DCSS_SS_INTER_MODE),
};

struct dcss_ss_priv {
	struct dcss_soc *dcss;
	void __iomem *base_reg;
	u32 base_ofs;

	u32 ctx_id;

	bool in_use;
};

static void dcss_ss_write(struct dcss_ss_priv *ss, u32 val, u32 ofs)
{
	if (!ss->in_use)
		dcss_writel(val, ss->base_reg + ofs);
#if defined(USE_CTXLD)
	dcss_ctxld_write(ss->dcss, ss->ctx_id, val,
			 ss->base_ofs + ofs);
#endif
}

#ifdef CONFIG_DEBUG_FS
void dcss_ss_dump_regs(struct seq_file *s, void *data)
{
	struct dcss_soc *dcss = data;
	int j;

	seq_puts(s, ">> Dumping SUBSAM:\n");
	for (j = 0; j < ARRAY_SIZE(ss_debug_reg); j++)
		seq_printf(s, "%-35s(0x%04x) -> 0x%08x\n",
			   ss_debug_reg[j].name,
			   ss_debug_reg[j].ofs,
			   dcss_readl(dcss->ss_priv->base_reg +
				      ss_debug_reg[j].ofs));
}
#endif

int dcss_ss_init(struct dcss_soc *dcss, unsigned long ss_base)
{
	struct dcss_ss_priv *ss;

	ss = devm_kzalloc(dcss->dev, sizeof(*ss), GFP_KERNEL);
	if (!ss)
		return -ENOMEM;

	dcss->ss_priv = ss;
	ss->dcss = dcss;

	ss->base_reg = devm_ioremap(dcss->dev, ss_base, SZ_4K);
	if (!ss->base_reg) {
		dev_err(dcss->dev, "ss: unable to remap ss base\n");
		return -ENOMEM;
	}

	ss->base_ofs = ss_base;

#if defined(USE_CTXLD)
	ss->ctx_id = CTX_SB_HP;
#endif

	/* TODO: should these be hardcoded? */
	dcss_ss_write(dcss->ss_priv, 0x41614161, DCSS_SS_COEFF);
	dcss_ss_write(dcss->ss_priv, 0x03ff0000, DCSS_SS_CLIP_CB);
	dcss_ss_write(dcss->ss_priv, 0x03ff0000, DCSS_SS_CLIP_CR);

	return 0;
}

void dcss_ss_exit(struct dcss_soc *dcss)
{
	dcss_writel(0, dcss->ss_priv->base_reg + DCSS_SS_SYS_CTRL);
}

void dcss_ss_sync_set(struct dcss_soc *dcss, struct videomode *vm,
		      bool phsync, bool pvsync)
{
	struct dcss_ss_priv *ss = dcss->ss_priv;
	u16 lrc_x, lrc_y;
	u16 hsync_start, hsync_end;
	u16 vsync_start, vsync_end;
	u16 de_ulc_x, de_ulc_y;
	u16 de_lrc_x, de_lrc_y;

	lrc_x = vm->hfront_porch + vm->hback_porch + vm->hsync_len +
		vm->hactive - 1;
	lrc_y = vm->vfront_porch + vm->vback_porch + vm->vsync_len +
		vm->vactive - 1;

	dcss_ss_write(ss, (lrc_y << LRC_Y_POS) | lrc_x, DCSS_SS_DISPLAY);

	hsync_start = vm->hfront_porch + vm->hback_porch + vm->hsync_len +
		      vm->hactive - 1;
	hsync_end = vm->hsync_len - 1;

	dcss_ss_write(ss, (phsync ? SYNC_POL : 0) |
		      (hsync_end << SYNC_END_POS) | hsync_start,
		      DCSS_SS_HSYNC);

	vsync_start = vm->vfront_porch - 1;
	vsync_end = vm->vfront_porch + vm->vsync_len - 1;

	dcss_ss_write(ss, (pvsync ? SYNC_POL : 0) |
		      (vsync_end << SYNC_END_POS) | vsync_start, DCSS_SS_VSYNC);

	de_ulc_x = vm->hsync_len + vm->hback_porch - 1;
	de_ulc_y = vm->vsync_len + vm->vfront_porch + vm->vback_porch;

	dcss_ss_write(ss, SYNC_POL | (de_ulc_y << ULC_Y_POS) | de_ulc_x,
		      DCSS_SS_DE_ULC);

	de_lrc_x = vm->hsync_len + vm->hback_porch + vm->hactive - 1;
	de_lrc_y = vm->vsync_len + vm->vfront_porch + vm->vback_porch +
		   vm->vactive - 1;

	dcss_ss_write(ss, (de_lrc_y << LRC_Y_POS) | de_lrc_x, DCSS_SS_DE_LRC);
}
EXPORT_SYMBOL(dcss_ss_sync_set);

void dcss_ss_enable(struct dcss_soc *dcss, bool en)
{
	struct dcss_ss_priv *ss = dcss->ss_priv;

	dcss_ss_write(ss, en ? RUN_EN : 0, DCSS_SS_SYS_CTRL);
}
EXPORT_SYMBOL(dcss_ss_enable);
