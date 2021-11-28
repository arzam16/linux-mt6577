// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Device Tree support for Mediatek mt6577 SoC Family
 *
 * Author: Boris Lysov <arzamas-16@mail.ee>
 */
#include <asm/mach/arch.h>

static const char * const mt6577_board_dt_compat[] = {
	"mediatek,mt6577",
	"mediatek,mt8317",
	"mediatek,mt8377",
	NULL,
};

DT_MACHINE_START(MEDIATEK_MT6577_DT, "Mediatek mt6577 SoC Family")
	.dt_compat		= mt6577_board_dt_compat,
	.l2c_aux_val	= 0x70400000,
	.l2c_aux_mask	= 0x8fbfffff,
MACHINE_END
