// SPDX-License-Identifier: GPL-2.0-only
/*
 * arch/arm/mach-mediatek/mt6577.h
 *
 * Author: Boris Lysov <arzamas-16@mail.ee>
 */

#include <asm/smp.h>
#include <linux/spinlock.h>
#include "platsmp.h"

// SMP registers
#define RST_CTL0			0x10
#define PWR_CTL1			0x24
#define PWR_MON				0xA8
#define SCU_CPU_PWR_STATUS	0x08

// infrasys registers
#define INFRACFG_ACP		0xED0

static const struct mtk_smp_boot_info mtk_mt6577_boot = {
	0xc011a000, 0x0,
	{ 0x534c4156 },
	{ 0x8 },
};

static spinlock_t cpu1_pwr_ctr_lock;
static void __init mt6577_smp_prepare_cpus(unsigned int max_cpus);
static int mt6577_boot_secondary(unsigned int cpu, struct task_struct *idle);
static int mt6577_cpu_kill(unsigned int cpu);

static void mt6577_reset_cpu1(void);
static void mt6577_power_on_cpu1(void);
static void mt6577_power_off_cpu1(void);

#ifdef CONFIG_SMP
static const struct smp_operations mt6577_smp_ops __initconst = {
	.smp_prepare_cpus	= mt6577_smp_prepare_cpus,
	.smp_boot_secondary	= mt6577_boot_secondary,
	.cpu_kill			= mt6577_cpu_kill,
};
CPU_METHOD_OF_DECLARE(mt6577_smp_ops, "mediatek,mt6577-smp", &mt6577_smp_ops);
#endif
