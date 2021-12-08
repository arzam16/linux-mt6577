// SPDX-License-Identifier: GPL-2.0
/*
 * Mediatek Hardware ID driver
 * Based on drivers/soc/qcom/socinfo.c
 * 
 * Author: Boris Lysov <arzamas-16@mail.ee>
 */

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>

#include <linux/device.h>
#include <linux/sys_soc.h>

/*
 * Uh oh, I'm too lazy to create a devicetree doc
 * 
 * 	hardware-id@f8000000 {
 *		compatible = "mediatek,mt6577-hwid";
 *		reg = <0xf8000000 0x10>,
 *				<0xc1019100 0x50>;
 *	};
 * 
 * compatible: (required)
 * 	"mediatek,mt6572-hwid" for mt6572, mt6582, mt6592, mt8312
 * 	"mediatek,mt6573-hwid" for mt6573
 * 	"mediatek,mt6577-hwid" for mt6515, mt6575, mt6517, mt6577, mt8317, mt8377
 * 	"mediatek,mt6572-hwid", "mediatek,mt6589-hwid" for mt6589, mt8389
 * 
 * reg:
 * 	hardware id base (required)
 * 	cpu flags base (should be optional imo)
 * 
 * Hardware ID bases:
 * 	0x08000000 for mt6572, mt6582, mt6592, mt8312
 * 	0x70026000 for mt6573
 * 	0xf8000000 for mt6515, mt6575, mt6517, mt6577, mt8317, mt8377
 * 	0x08000000 for mt6589, mt8389
 */

struct mtk_hwid {
	u32 chip_id;
	u32 chip_subid;
	u32 hwver;
	u32 swver;
};

struct mtk_hwid_data {
	u32 chip_id_offset;
	u32 chip_subid_offset;
	u32 hwver_offset;
	u32 swver_offset;
	u32 key_offset;
};

struct mtk_hwid_context {
	struct device *dev;
	struct soc_device *soc_dev;
	struct soc_device_attribute attr;
	void __iomem *info_base;
	
	/*
	 * SoCs such as Cortex-A9 mt65xx's have additional registers with
	 * special flags for SoC capabilities. Next to these registers usually
	 * is a 64-bit cpu_key register.
	 */
	void __iomem *flags_base;
	
	struct mtk_hwid *hwid;
	const struct mtk_hwid_data *data;
};

static char * mtk_hwid_get_family(struct mtk_hwid_context *ctx)
{
	switch (ctx->hwid->chip_id) {
		case 0x6572:
		case 0x6582:
		case 0x6592:
			return "mt65x2/mt8312 Cortex-A7 SoC Family";
		case 0x6573:
			return "mt65xx ARMv6 SoC Family";
		case 0x6515:
		case 0x6517:
		case 0x6575:
		case 0x6577:
			return "mt65xx/mt83x7 Cortex-A9 SoC Family";
		case 0x6589:
			return "mt6589/mt8389 Cortex-A7 SoC Family";
		default:
			return "Application Processor";
	};
}

static void mtk_hwid_read(struct mtk_hwid_context *ctx)
{
	ctx->hwid->chip_id = readw(ctx->info_base + ctx->data->chip_id_offset);
	ctx->hwid->chip_subid = readw(ctx->info_base + ctx->data->chip_subid_offset);
	ctx->hwid->hwver = readw(ctx->info_base + ctx->data->hwver_offset);
	ctx->hwid->swver = readw(ctx->info_base + ctx->data->swver_offset);
	
	/*
	 * All mt65xx Cortex-A9 family SoCs (mt65x5, mt65x7, mt83x7) report
	 * a chip_id of mt6575 or mt6577 and we need to compare their subIDs
	 * and flags to tell them apart.
	 */
	if (ctx->hwid->chip_id == 0x6575 || ctx->hwid->chip_id == 0x6577) {
		u32 flags = readl(ctx->flags_base);
		// If flags[24] = 1 then the SoC is mt651x
		if (ctx->hwid->chip_subid == 0x8A00)
			ctx->hwid->chip_id = (flags & BIT(24))? 0x6515 : 0x6575;
		else if (ctx->hwid->chip_subid == 0x8B00)
			ctx->hwid->chip_id = (flags & BIT(24))? 0x6517 : 0x6577;
	} else {
		pr_warn("unknown soc chip_id=%x, chip_subid=%x, hwver=%x, swver=%x\n",
				ctx->hwid->chip_id,
				ctx->hwid->chip_subid,
				ctx->hwid->hwver,
				ctx->hwid->swver);
	}
	
	ctx->attr.machine = devm_kasprintf(ctx->dev, GFP_KERNEL, "Mediatek mt%x",
										ctx->hwid->chip_id);
	ctx->attr.family = mtk_hwid_get_family(ctx);
	ctx->attr.revision = devm_kasprintf(ctx->dev, GFP_KERNEL, "SubID: %x, HwVer: %x, SwVer: %x",
										ctx->hwid->chip_subid,
										ctx->hwid->hwver,
										ctx->hwid->swver);
	// TODO read the value into ctx->attr.soc_id
	// TODO calculate ctx->attr.serial_number based on the soc_id
}

static int mtk_hwid_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct mtk_hwid_context *ctx;

	if (IS_ERR(node))
		return -ENODEV;

	ctx = devm_kzalloc(&pdev->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->dev = &pdev->dev;
	ctx->data = of_device_get_match_data(&pdev->dev);
	
	ctx->info_base = of_iomap(node, 0);
	if (!ctx->info_base)
		return -EINVAL;
	
	ctx->flags_base = of_iomap(node, 1);
	if (!ctx->flags_base) // TODO maybe allow absense of flags_base for some SoCs?
		return -EINVAL;
	
	mtk_hwid_read(ctx);
	
	ctx->soc_dev = soc_device_register(&ctx->attr);
	if (IS_ERR(ctx->soc_dev))
		return PTR_ERR(ctx->soc_dev);
	
	platform_set_drvdata(pdev, ctx);

	return 0;
}

static const struct mtk_hwid_data hwid_mt6572 = {
	.chip_id_offset		= 0x0,
	.chip_subid_offset	= 0x4,
	.hwver_offset		= 0x8,
	.swver_offset		= 0xC,
	.key_offset			= -1,
	/*
	 * TODO check for -1  ^^ as some platforms might not have a cpu_key register
	 * close to the other ones listed here. In this case cpu_key and the serial
	 * number should be obtained/calculated in another way.
	 */
};

static const struct mtk_hwid_data hwid_mt6573 = {
	.chip_id_offset		= 0x8,
	.chip_subid_offset	= -1, // 0x8 should also work here
	.hwver_offset		= 0x0,
	.swver_offset		= 0x4,
	.key_offset			= -1,
};

static const struct mtk_hwid_data hwid_mt6577 = {
	.chip_id_offset		= 0x0,
	.chip_subid_offset	= 0x4,
	.hwver_offset		= 0x8,
	.swver_offset		= 0xC,
	.key_offset			= 0x40,
};

static const struct of_device_id mtk_hwid_dt_match[] = {
	{ .compatible = "mediatek,mt6572-hwid", .data = &hwid_mt6572, },
	{ .compatible = "mediatek,mt6573-hwid", .data = &hwid_mt6573, },
	{ .compatible = "mediatek,mt6577-hwid", .data = &hwid_mt6577, },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, mtk_hwid_dt_match);

static struct platform_driver mtk_hwid_driver = {
	.probe = mtk_hwid_probe,
	.driver = {
		.name = "mtk-hardware-id",
		.of_match_table = mtk_hwid_dt_match,
	},
};

module_platform_driver(mtk_hwid_driver);

MODULE_DESCRIPTION("Mediatek Hardware ID Driver");
MODULE_AUTHOR("Boris Lysov <arzamas-16@mail.ee>");
MODULE_LICENSE("GPL");
