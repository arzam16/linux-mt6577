// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Device Tree support for Mediatek mt6577 SoC Family
 *
 * Author: Boris Lysov <arzamas-16@mail.ee>
 */
#include <asm/mach/arch.h>
#include <linux/of.h>
#include <linux/of_address.h>

#define INFRACFG_ACP 0xED0

static const char * const mt6577_board_dt_compat[] = {
	"mediatek,mt6577",
	"mediatek,mt8317",
	"mediatek,mt8377",
	NULL,
};

/* 
 * To allow transactions to Accelerator Coherency Port, we need
 * to set several ARCACHE and AWCACHE registers (PERI and MM).
 * It is possible to skip this routine but potentially it could backfire somehow.
 */
static void __init mt6577_init_machine(void)
{
	struct device_node *np;
	void __iomem *infracfg_base;
	
	np = of_find_compatible_node(NULL, NULL, "mediatek,mt6577-infracfg");
	if (!np) {
		pr_warn("no infracfg device node, ACP transactions won't be allowed\n");
		return;
	}
	infracfg_base = of_iomap(np, 0);
	writel(0x00003333, infracfg_base + INFRACFG_ACP);
	
	iounmap(infracfg_base);
	of_node_put(np);
}

DT_MACHINE_START(MEDIATEK_MT6577_DT, "Mediatek mt6577 SoC Family")
	.dt_compat		= mt6577_board_dt_compat,
	.init_machine	= mt6577_init_machine,
	.l2c_aux_val	= 0x70400000,
	.l2c_aux_mask	= 0x8fbfffff,
MACHINE_END
