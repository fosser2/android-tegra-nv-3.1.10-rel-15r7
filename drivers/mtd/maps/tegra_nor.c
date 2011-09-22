/*
 * drivers/mtd/maps/tegra_nor.c
 *
 * MTD mapping driver for the internal SNOR controller in Tegra SoCs
 *
 * Copyright (C) 2009 - 2011 NVIDIA Corporation
 *
 * Author:
 *	Raghavendra VK <rvk@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/mutex.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/dma-mapping.h>
#include <linux/proc_fs.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/platform_data/tegra_nor.h>
#include <asm/cacheflush.h>

#define __BITMASK0(len)			((1 << (len)) - 1)
#define __BITMASK(start, len)		(__BITMASK0(len) << (start))
#define REG_BIT(bit)			BIT(bit)
#define REG_FIELD(val, start, len)	(((val) & __BITMASK0(len)) << (start))
#define REG_FIELD_MASK(start, len)	(~(__BITMASK((start), (len))))
#define REG_GET_FIELD(val, start, len)	(((val) >> (start)) & __BITMASK0(len))

/* tegra gmi registers... */
#define TEGRA_SNOR_CONFIG_REG			0x00
#define TEGRA_SNOR_NOR_ADDR_PTR_REG		0x08
#define TEGRA_SNOR_AHB_ADDR_PTR_REG		0x0C
#define TEGRA_SNOR_TIMING0_REG			0x10
#define TEGRA_SNOR_TIMING1_REG			0x14
#define TEGRA_SNOR_DMA_CFG_REG			0x20

/* config register */
#define TEGRA_SNOR_CONFIG_GO			REG_BIT(31)
#define TEGRA_SNOR_CONFIG_WORDWIDE		REG_BIT(30)
#define TEGRA_SNOR_CONFIG_DEVICE_TYPE		REG_BIT(29)
#define TEGRA_SNOR_CONFIG_MUX_MODE		REG_BIT(28)
#define TEGRA_SNOR_CONFIG_BURST_LEN(val)	REG_FIELD((val), 26, 2)
#define TEGRA_SNOR_CONFIG_RDY_ACTIVE		REG_BIT(24)
#define TEGRA_SNOR_CONFIG_RDY_POLARITY		REG_BIT(23)
#define TEGRA_SNOR_CONFIG_ADV_POLARITY		REG_BIT(22)
#define TEGRA_SNOR_CONFIG_OE_WE_POLARITY	REG_BIT(21)
#define TEGRA_SNOR_CONFIG_CS_POLARITY		REG_BIT(20)
#define TEGRA_SNOR_CONFIG_NOR_DPD		REG_BIT(19)
#define TEGRA_SNOR_CONFIG_WP			REG_BIT(15)
#define TEGRA_SNOR_CONFIG_PAGE_SZ(val)		REG_FIELD((val), 8, 2)
#define TEGRA_SNOR_CONFIG_MST_ENB		REG_BIT(7)
#define TEGRA_SNOR_CONFIG_SNOR_CS(val)		REG_FIELD((val), 4, 2)
#define TEGRA_SNOR_CONFIG_CE_LAST		REG_FIELD(3)
#define TEGRA_SNOR_CONFIG_CE_FIRST		REG_FIELD(2)
#define TEGRA_SNOR_CONFIG_DEVICE_MODE(val)	REG_FIELD((val), 0, 2)

/* dma config register */
#define TEGRA_SNOR_DMA_CFG_GO			REG_BIT(31)
#define TEGRA_SNOR_DMA_CFG_BSY			REG_BIT(30)
#define TEGRA_SNOR_DMA_CFG_DIR			REG_BIT(29)
#define TEGRA_SNOR_DMA_CFG_INT_ENB		REG_BIT(28)
#define TEGRA_SNOR_DMA_CFG_INT_STA		REG_BIT(27)
#define TEGRA_SNOR_DMA_CFG_BRST_SZ(val)		REG_FIELD((val), 24, 3)
#define TEGRA_SNOR_DMA_CFG_WRD_CNT(val)		REG_FIELD((val), 2, 14)

/* timing 0 register */
#define TEGRA_SNOR_TIMING0_PG_RDY(val)		REG_FIELD((val), 28, 4)
#define TEGRA_SNOR_TIMING0_PG_SEQ(val)		REG_FIELD((val), 20, 4)
#define TEGRA_SNOR_TIMING0_MUX(val)		REG_FIELD((val), 12, 4)
#define TEGRA_SNOR_TIMING0_HOLD(val)            REG_FIELD((val), 8, 4)
#define TEGRA_SNOR_TIMING0_ADV(val)		REG_FIELD((val), 4, 4)
#define TEGRA_SNOR_TIMING0_CE(val)		REG_FIELD((val), 0, 4)

/* timing 1 register */
#define TEGRA_SNOR_TIMING1_WE(val)		REG_FIELD((val), 16, 8)
#define TEGRA_SNOR_TIMING1_OE(val)		REG_FIELD((val), 8, 8)
#define TEGRA_SNOR_TIMING1_WAIT(val)		REG_FIELD((val), 0, 8)

/* SNOR DMA supports 2^14 AHB (32-bit words)
 * Maximum data in one transfer = 2^16 bytes
 */
#define TEGRA_SNOR_DMA_LIMIT           (0x10000)
#define TEGRA_SNOR_DMA_LIMIT_WORDS     (TEGRA_SNOR_DMA_LIMIT >> 2)

/* Even if BW is 1 MB/s, maximum time to
 * transfer SNOR_DMA_LIMIT bytes is 66 ms
 */
#define SNOR_DMA_TIMEOUT_MS       (67)

struct tegra_nor_info {
	struct tegra_nor_platform_data *plat;
	struct device *dev;
	struct clk *clk;
	struct mtd_partition *parts;
	struct mtd_info *mtd;
	struct map_info map;
	struct completion dma_complete;
	void __iomem *base;
	u32 init_config;
	u32 timing0_default, timing1_default;
	u32 timing0_read, timing1_read;
};

static inline unsigned long snor_tegra_readl(struct tegra_nor_info *tnor,
					     unsigned long reg)
{
	return readl(tnor->base + reg);
}

static inline void snor_tegra_writel(struct tegra_nor_info *tnor,
				     unsigned long val, unsigned long reg)
{
	writel(val, tnor->base + reg);
}

#define DRV_NAME "tegra-nor"

#ifdef CONFIG_MTD_PARTITIONS
static const char *part_probes[] = { "cmdlinepart", NULL };
#endif

static int wait_for_dma_completion(struct tegra_nor_info *info)
{
	unsigned long dma_timeout;
	int ret;

	dma_timeout = msecs_to_jiffies(SNOR_DMA_TIMEOUT_MS);
	ret = wait_for_completion_timeout(&info->dma_complete, dma_timeout);
	return ret ? 0 : -ETIMEDOUT;
}

static void tegra_flash_dma(struct map_info *map,
			    void *to, unsigned long from, ssize_t len)
{
	u32 snor_config, dma_config = 0;
	int dma_transfer_count = 0, word32_count = 0;
	u32 nor_address, ahb_address, current_transfer;
	struct tegra_nor_info *c =
	    container_of(map, struct tegra_nor_info, map);
	unsigned int bytes_remaining = len;

	snor_config = c->init_config;
	snor_tegra_writel(c, c->timing0_read, TEGRA_SNOR_TIMING0_REG);
	snor_tegra_writel(c, c->timing1_read, TEGRA_SNOR_TIMING1_REG);

	if (len > 32) {
		if (to >= high_memory) {
			struct page *p1;
			if (((size_t) to & PAGE_MASK) !=
			    ((size_t) (to + len - 1) & PAGE_MASK))
				goto out_copy;
			p1 = vmalloc_to_page(to);
			if (!p1)
				goto out_copy;
			to = page_address(p1) + ((size_t) to & ~PAGE_MASK);
		}
		ahb_address = dma_map_single(c->dev, to, len, DMA_FROM_DEVICE);
		if (dma_mapping_error(c->dev, ahb_address)) {
			dev_err(c->dev,
				"Couldn't DMA map a %d byte buffer\n", len);
			goto out_copy;
		}
		word32_count = len >> 2;
		bytes_remaining = len & 0x00000003;
		/*
		 * The parameters can be setup in any order since we write to
		 * controller register only after all parameters are set.
		 */
		/* SNOR CONFIGURATION SETUP */
		snor_config |= TEGRA_SNOR_CONFIG_DEVICE_MODE(1);
		/* 8 word page */
		snor_config |= TEGRA_SNOR_CONFIG_PAGE_SZ(2);
		snor_config |= TEGRA_SNOR_CONFIG_MST_ENB;
		/* SNOR DMA CONFIGURATION SETUP */
		/* NOR -> AHB */
		dma_config &= ~TEGRA_SNOR_DMA_CFG_DIR;
		/* One word burst */
		dma_config |= TEGRA_SNOR_DMA_CFG_BRST_SZ(4);

		for (nor_address = (unsigned int)(map->phys + from);
		     word32_count > 0;
		     word32_count -= current_transfer,
		     dma_transfer_count += current_transfer,
		     nor_address += (current_transfer * 4),
		     ahb_address += (current_transfer * 4)) {

			current_transfer =
			    (word32_count > TEGRA_SNOR_DMA_LIMIT_WORDS)
			    ? (TEGRA_SNOR_DMA_LIMIT_WORDS) : word32_count;
			/* Start NOR operation */
			snor_config |= TEGRA_SNOR_CONFIG_GO;
			dma_config |= TEGRA_SNOR_DMA_CFG_GO;
			/* Enable interrupt before every transaction since the
			 * interrupt handler disables it */
			dma_config |= TEGRA_SNOR_DMA_CFG_INT_ENB;
			/* Num of AHB (32-bit) words to transferred minus 1 */
			dma_config |=
			    TEGRA_SNOR_DMA_CFG_WRD_CNT(current_transfer - 1);
			snor_tegra_writel(c, ahb_address,
					  TEGRA_SNOR_AHB_ADDR_PTR_REG);
			snor_tegra_writel(c, nor_address,
					  TEGRA_SNOR_NOR_ADDR_PTR_REG);
			snor_tegra_writel(c, snor_config,
					  TEGRA_SNOR_CONFIG_REG);
			snor_tegra_writel(c, dma_config,
					  TEGRA_SNOR_DMA_CFG_REG);
			if (wait_for_dma_completion(c)) {
				dev_err(c->dev, "timout waiting for DMA\n");
				/* Transfer the remaining words by memcpy */
				bytes_remaining += (word32_count << 2);
				break;
			}
		}
		dma_unmap_single(c->dev, ahb_address, len, DMA_FROM_DEVICE);
	}
	/* Put the controller back into slave mode. */
	snor_config = snor_tegra_readl(c, TEGRA_SNOR_CONFIG_REG);
	snor_config &= ~TEGRA_SNOR_CONFIG_MST_ENB;
	snor_config |= TEGRA_SNOR_CONFIG_DEVICE_MODE(0);
	snor_tegra_writel(c, snor_config, TEGRA_SNOR_CONFIG_REG);
out_copy:
	memcpy_fromio(((char *)to + (dma_transfer_count << 2)),
		      ((char *)(map->virt + from) + (dma_transfer_count << 2)),
		      bytes_remaining);

	snor_tegra_writel(c, c->timing0_default, TEGRA_SNOR_TIMING0_REG);
	snor_tegra_writel(c, c->timing1_default, TEGRA_SNOR_TIMING1_REG);
}

static irqreturn_t tegra_nor_isr(int flag, void *dev_id)
{
	struct tegra_nor_info *info = (struct tegra_nor_info *)dev_id;
	u32 dma_config = snor_tegra_readl(info, TEGRA_SNOR_DMA_CFG_REG);
	if (dma_config & TEGRA_SNOR_DMA_CFG_INT_STA) {
		/* Disable interrupts. WAR for BUG:821560 */
		dma_config &= ~TEGRA_SNOR_DMA_CFG_INT_ENB;
		snor_tegra_writel(info, dma_config, TEGRA_SNOR_DMA_CFG_REG);
		complete(&info->dma_complete);
	} else {
		printk(KERN_ERR "%s: Spurious interrupt\n", __func__);
	}
	return IRQ_HANDLED;
}

static int tegra_snor_controller_init(struct tegra_nor_info *info)
{
	struct tegra_nor_chip_parms *chip_parm = &info->plat->chip_parms;
	u32 width = info->plat->flash.width;
	u32 config = 0;
	unsigned long clk_rate_khz = clk_get_rate(info->clk) / 1000;

	config |= TEGRA_SNOR_CONFIG_DEVICE_MODE(0);
	config |= TEGRA_SNOR_CONFIG_SNOR_CS(0);
	config &= ~TEGRA_SNOR_CONFIG_DEVICE_TYPE;	/* Select NOR */
	config |= TEGRA_SNOR_CONFIG_WP;	/* Enable writes */
	switch (width) {
	case 2:
		config &= ~TEGRA_SNOR_CONFIG_WORDWIDE;	/* 16 bit */
		break;
	case 4:
		config |= TEGRA_SNOR_CONFIG_WORDWIDE;	/* 32 bit */
		break;
	default:
		return -EINVAL;
	}
	config |= TEGRA_SNOR_CONFIG_BURST_LEN(0);
	config &= ~TEGRA_SNOR_CONFIG_MUX_MODE;
	snor_tegra_writel(info, config, TEGRA_SNOR_CONFIG_REG);
	info->init_config = config;

#define TIME_TO_CNT(timing) \
	(((((timing) * (clk_rate_khz)) + 1000000 - 1) / 1000000) - 1)
	info->timing0_default =
	    (TEGRA_SNOR_TIMING0_PG_RDY
	     (TIME_TO_CNT(chip_parm->timing_default.pg_rdy)) |
	     TEGRA_SNOR_TIMING0_PG_SEQ(TIME_TO_CNT
				       (chip_parm->timing_default.pg_seq)) |
	     TEGRA_SNOR_TIMING0_MUX(TIME_TO_CNT(chip_parm->timing_default.mux))
	     |
	     TEGRA_SNOR_TIMING0_HOLD(TIME_TO_CNT
				     (chip_parm->timing_default.hold)) |
	     TEGRA_SNOR_TIMING0_ADV(TIME_TO_CNT(chip_parm->timing_default.adv))
	     |
	     TEGRA_SNOR_TIMING0_CE(TIME_TO_CNT(chip_parm->timing_default.ce)));

	snor_tegra_writel(info, info->timing0_default, TEGRA_SNOR_TIMING0_REG);

	info->timing1_default =
	    (TEGRA_SNOR_TIMING1_WE(TIME_TO_CNT(chip_parm->timing_default.we)) |
	     TEGRA_SNOR_TIMING1_OE(TIME_TO_CNT(chip_parm->timing_default.oe)) |
	     TEGRA_SNOR_TIMING1_WAIT(TIME_TO_CNT
				     (chip_parm->timing_default.wait)));

	snor_tegra_writel(info, info->timing1_default, TEGRA_SNOR_TIMING1_REG);

	info->timing0_read =
	    (TEGRA_SNOR_TIMING0_PG_RDY
	     (TIME_TO_CNT(chip_parm->timing_read.pg_rdy)) |
	     TEGRA_SNOR_TIMING0_PG_SEQ(TIME_TO_CNT
				       (chip_parm->timing_read.pg_seq)) |
	     TEGRA_SNOR_TIMING0_MUX(TIME_TO_CNT(chip_parm->timing_read.mux)) |
	     TEGRA_SNOR_TIMING0_HOLD(TIME_TO_CNT(chip_parm->timing_read.hold)) |
	     TEGRA_SNOR_TIMING0_ADV(TIME_TO_CNT(chip_parm->timing_read.adv)) |
	     TEGRA_SNOR_TIMING0_CE(TIME_TO_CNT(chip_parm->timing_read.ce)));

	info->timing1_read =
	    (TEGRA_SNOR_TIMING1_WE(TIME_TO_CNT(chip_parm->timing_read.we)) |
	     TEGRA_SNOR_TIMING1_OE(TIME_TO_CNT(chip_parm->timing_read.oe)) |
	     TEGRA_SNOR_TIMING1_WAIT(TIME_TO_CNT(chip_parm->timing_read.wait)));
#undef TIME_TO_CNT
	return 0;
}

static int tegra_nor_probe(struct platform_device *pdev)
{
	int err = 0;
	struct tegra_nor_platform_data *plat = pdev->dev.platform_data;
	struct tegra_nor_info *info = NULL;
	struct resource *res;
	struct resource *iomem;
	int irq;

	if (!plat) {
		pr_err("%s: no platform device info\n", __func__);
		err = -EINVAL;
		goto fail;
	}

	/* Get NOR flash aperture & map the same */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no mem resource?\n");
		err = -ENODEV;
		goto fail;
	}

	iomem = request_mem_region(res->start, resource_size(res), pdev->name);
	if (!iomem) {
		dev_err(&pdev->dev, "NOR region already claimed\n");
		err = -EBUSY;
		goto fail;
	}

	info = kzalloc(sizeof(struct tegra_nor_info), GFP_KERNEL);
	if (!info) {
		err = -ENOMEM;
		goto out_release_region;
	}

	info->plat = plat;
	info->dev = &pdev->dev;
	info->map.bankwidth = plat->flash.width;
	info->map.name = dev_name(&pdev->dev);

	info->base = ioremap(iomem->start, resource_size(iomem));
	if (!info->base) {
		dev_err(&pdev->dev, "Can't ioremap NOR region\n");
		err = -ENOMEM;
		goto out_free_info;
	}

	info->map.phys = iomem->start;
	info->map.size = resource_size(iomem);
	info->map.virt = info->base;

	info->clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR(info->clk)) {
		err = PTR_ERR(info->clk);
		goto out_iounmap;
	}

	err = clk_enable(info->clk);
	if (err != 0)
		goto out_clk_put;

	simple_map_init(&info->map);
	info->map.copy_from = tegra_flash_dma;

	/* Intialise the SNOR controller before probe */
	err = tegra_snor_controller_init(info);
	if (err) {
		dev_err(&pdev->dev, "Error initializing controller\n");
		goto out_clk_disable;
	}

	init_completion(&info->dma_complete);

	irq = platform_get_irq(pdev, 0);
	if (!irq) {
		dev_err(&pdev->dev, "no irq resource?\n");
		err = -ENODEV;
		goto out_clk_disable;
	}

	/* Register SNOR DMA completion interrupt */
	err = request_irq(irq, tegra_nor_isr, IRQF_DISABLED, pdev->name, info);
	if (err) {
		dev_err(&pdev->dev, "Failed to request irq %i\n", irq);
		goto out_clk_disable;
	}

	info->mtd = do_map_probe(plat->flash.map_name, &info->map);
	if (!info->mtd) {
		err = -EIO;
		goto out_clk_disable;
	}
	info->mtd->owner = THIS_MODULE;
	info->parts = NULL;

	platform_set_drvdata(pdev, info);
#ifdef CONFIG_MTD_PARTITIONS
	err = parse_mtd_partitions(info->mtd, part_probes, &info->parts, 0);
	if (err > 0)
		err = add_mtd_partitions(info->mtd, info->parts, err);
	else if (err <= 0 && plat->flash.parts)
		err =
		    add_mtd_partitions(info->mtd, plat->flash.parts,
				       plat->flash.nr_parts);
	else
#endif
		add_mtd_device(info->mtd);

	return 0;

out_clk_disable:
	clk_disable(info->clk);
out_clk_put:
	clk_put(info->clk);
out_iounmap:
	iounmap(iomem);
out_free_info:
	kfree(info);
out_release_region:
	release_region(iomem->start, resource_size(iomem));
fail:
	pr_err("Tegra NOR probe failed\n");
	return err;
}

static int tegra_nor_remove(struct platform_device *pdev)
{
	struct tegra_nor_info *info = platform_get_drvdata(pdev);

	if (info->parts) {
		del_mtd_partitions(info->mtd);
		kfree(info->parts);
	} else
		del_mtd_device(info->mtd);
	map_destroy(info->mtd);
	release_mem_region(info->map.phys, info->map.size);
	iounmap((void __iomem *)info->map.virt);
	clk_disable(info->clk);
	clk_put(info->clk);
	kfree(info);

	return 0;
}

static struct platform_driver __refdata tegra_nor_driver = {
	.probe = tegra_nor_probe,
	.remove = __devexit_p(tegra_nor_remove),
	.driver = {
		   .name = DRV_NAME,
		   .owner = THIS_MODULE,
		   },
};

static int __init tegra_nor_init(void)
{
	return platform_driver_register(&tegra_nor_driver);
}

static void __exit tegra_nor_exit(void)
{
	platform_driver_unregister(&tegra_nor_driver);
}

module_init(tegra_nor_init);
module_exit(tegra_nor_exit);

MODULE_AUTHOR("Raghavendra VK <rvk@nvidia.com>");
MODULE_DESCRIPTION("NOR Flash mapping driver for NVIDIA Tegra based boards");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
