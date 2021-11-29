// SPDX-License-Identifier: GPL-2.0-only
/*
 * arch/arm/mach-mediatek/platsmp.h
 *
 * Author: Shunli Wang <shunli.wang@mediatek.com>
 *         Yingjoe Chen <yingjoe.chen@mediatek.com>
 *         Boris Lysov <arzamas-16@mail.ee>
 */

#define MTK_MAX_CPU		8
#define MTK_SMP_REG_SIZE	0x1000

struct mtk_smp_boot_info {
	unsigned long smp_base;
	unsigned int jump_reg;
	unsigned int core_keys[MTK_MAX_CPU - 1];
	unsigned int core_regs[MTK_MAX_CPU - 1];
};
