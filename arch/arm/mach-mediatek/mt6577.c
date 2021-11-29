// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Device Tree support for Mediatek mt6577 SoC Family
 *
 * Author: Boris Lysov <arzamas-16@mail.ee>
 */
#include <asm/delay.h>
#include <asm/smp_scu.h>
#include <asm/mach/arch.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include "mt6577.h"

static const char * const mt6577_board_dt_compat[] = {
	"mediatek,mt6577",
	"mediatek,mt8317",
	"mediatek,mt8377",
	NULL,
};

static void __iomem *mtk_smp_base;
static void __iomem *scu_base;
static void __iomem *mcusys_base;

static void __init mt6577_smp_prepare_cpus(unsigned int max_cpus)
{
	struct device_node *node;
	
	mtk_smp_base = ioremap(mtk_mt6577_boot.smp_base, MTK_SMP_REG_SIZE);
	if (!mtk_smp_base) {
		pr_err("%s: Can't remap %lx\n", __func__,
			mtk_mt6577_boot.smp_base);
		return;
	}
	
	spin_lock_init(&cpu1_pwr_ctr_lock);
	
	// Map MCUSYS to reset CPU1 state now and power it on later
    node = of_find_compatible_node(NULL, NULL, "mediatek,mt6577-mcusys");
	if (!node) {
		pr_err("%s: missing mcusys in device tree\n", __func__);
		return;
	}
	
	mcusys_base = of_iomap(node, 0);
	of_node_put(node);
	if (!mcusys_base) {
		pr_err("%s: Can't remap mcusys registers\n", __func__);
		return;
	}
	
	mt6577_reset_cpu1();
	
	// Map SCU to enable it now and power on the CPU1 later
	node = of_find_compatible_node(NULL, NULL, "arm,cortex-a9-scu");
	if (!node) {
		pr_err("%s: missing scu in device tree\n", __func__);
		return;
	}
	
	scu_base = of_iomap(node, 0);
	of_node_put(node);
	if (!scu_base) {
		pr_err("%s: Can't remap scu registers\n", __func__);
	}

	scu_enable(scu_base);

	/*
	 * write the address of slave startup address into the system-wide
	 * jump register
	 */
	writel_relaxed(__pa_symbol(secondary_startup_arm),
			mtk_smp_base + mtk_mt6577_boot.jump_reg);
}

static int mt6577_boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	if (!mtk_smp_base)
		return -EINVAL;

	if (!mtk_mt6577_boot.core_keys[cpu-1])
		return -EINVAL;

	writel_relaxed(mtk_mt6577_boot.core_keys[cpu-1],
		mtk_smp_base + mtk_mt6577_boot.core_regs[cpu-1]);

	mt6577_power_on_cpu1();

	arch_send_wakeup_ipi_mask(cpumask_of(cpu));
    
    iounmap(mcusys_base);
    iounmap(scu_base);

	return 0;
}

/*
 * Thanks to the ancient bootloader, CPU1 may not be reset clearly after
 * powering on the device which might affect bringing it up on boot.
 * Reset it here by rewriting the SW_CPU_RST register (RST_CTL0[1:0]).
 */
static void mt6577_reset_cpu1(void)
{
	u32 val;
	
    val = readl(mcusys_base + RST_CTL0);
    writel(val | 0x2, mcusys_base + RST_CTL0);
    udelay(10);
    writel(val & ~0x2, mcusys_base + RST_CTL0);
    udelay(10);
}

static void mt6577_power_on_cpu1(void)
{
    unsigned long flags;
    u32 val;
    
    spin_lock_irqsave(&cpu1_pwr_ctr_lock, flags);
    
    mb();
    
    // Set RST_CTL0[1] = 1
    val = readl(mcusys_base + RST_CTL0) | 0x00000002;
    writel(val, mcusys_base + RST_CTL0);
    
    // Set PWR_CTL1[15] = 1 to enable internal power supply
    val = readl(mcusys_base + PWR_CTL1) | 0x00008000;
    writel(val, mcusys_base + PWR_CTL1);
    mb();
    udelay(32); // delay 31.25us (1T of 32K) for power-on settle time
    
    // Set PWR_CTL1[4] = 1 to disable SRAM sleep mode
    val = readl(mcusys_base + PWR_CTL1) & 0xfffffff0;
    writel(val, mcusys_base + PWR_CTL1);
    mb();
    mdelay(2); // delay 2ms
    
    // Set PWR_CTL1[13] = 0 to enable internal clock
    val = readl(mcusys_base + PWR_CTL1) & 0xffffdfff;
    writel(val, mcusys_base + PWR_CTL1);
    
    // Set PWR_CTL1[12] = 0 to enable internal RGU
    val = readl(mcusys_base + PWR_CTL1) & 0xffffefff;
    writel(val, mcusys_base + PWR_CTL1);
    
    // Set PWR_CTL1[14] = 0 to disable cell isolation
    val = readl(mcusys_base + PWR_CTL1) & 0xffffbfff;
    writel(val, mcusys_base + PWR_CTL1);
    
    // Set PWR_CTL1[11] = 1 to enable NEON1 power supply
    val = readl(mcusys_base + PWR_CTL1) | 0x00000800;
    writel(val, mcusys_base + PWR_CTL1);
    mb();
    udelay(32); // delay 31.25us (1T of 32K)
    // ... and set PWR_CTL1[9, 8, 10] = 0 to configure power supply
    val = readl(mcusys_base + PWR_CTL1) & 0xfffffdff;
    writel(val, mcusys_base + PWR_CTL1);
    val = readl(mcusys_base + PWR_CTL1) & 0xfffffeff;
    writel(val, mcusys_base + PWR_CTL1);
    val = readl(mcusys_base + PWR_CTL1) & 0xfffffbff;
    writel(val, mcusys_base + PWR_CTL1);
    
    // Set RST_CTL0[1] = 0 to send a reset signal
    val = readl(mcusys_base + RST_CTL0) & 0xfffffffd;
    writel(val, mcusys_base + RST_CTL0);
    mb();
    udelay(10); // delay 10us to compensate for the previous reset workaround
    
    // Set CPU1 power status register in SCU to normal mode
    val = readl(scu_base + SCU_CPU_PWR_STATUS) & 0xfffffcff;
    writel(val, scu_base + SCU_CPU_PWR_STATUS);
    // Finally set PWR_CTL1[25:24] = 0 to make CPU1 come up online
    val = readl(mcusys_base + PWR_CTL1) & 0xfcffffff;
    writel(val, mcusys_base + PWR_CTL1);
    mb();
    
    spin_unlock_irqrestore(&cpu1_pwr_ctr_lock, flags);
}

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
