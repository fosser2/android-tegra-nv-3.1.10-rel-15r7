/*
 * arch/arm/mach-tegra/common.c
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *	Colin Cross <ccross@android.com>
 *
 * Copyright (C) 2010-2011 NVIDIA Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/platform_device.h>
#include <linux/console.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/highmem.h>
#include <linux/memblock.h>

#include <asm/cacheflush.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/system.h>

#include <mach/iomap.h>
#include <mach/dma.h>
#include <mach/powergate.h>
#include <mach/system.h>
#include <mach/pinmux.h>

#include "apbio.h"
#include "board.h"
#include "clock.h"
#include "fuse.h"
#include "power.h"
#include "reset.h"

#define MC_SECURITY_CFG2	0x7c

#define AHB_ARBITRATION_PRIORITY_CTRL		0x4
#define   AHB_PRIORITY_WEIGHT(x)	(((x) & 0x7) << 29)
#define   PRIORITY_SELECT_USB	BIT(6)
#define   PRIORITY_SELECT_USB2	BIT(18)
#define   PRIORITY_SELECT_USB3	BIT(17)

#define AHB_GIZMO_AHB_MEM		0xc
#define   ENB_FAST_REARBITRATE	BIT(2)
#define   DONT_SPLIT_AHB_WR     BIT(7)

#define AHB_GIZMO_USB		0x1c
#define AHB_GIZMO_USB2		0x78
#define AHB_GIZMO_USB3		0x7c
#define   IMMEDIATE	BIT(18)

#define AHB_MEM_PREFETCH_CFG3	0xe0
#define AHB_MEM_PREFETCH_CFG4	0xe4
#define AHB_MEM_PREFETCH_CFG1	0xec
#define AHB_MEM_PREFETCH_CFG2	0xf0
#define   PREFETCH_ENB	BIT(31)
#define   MST_ID(x)	(((x) & 0x1f) << 26)
#define   AHBDMA_MST_ID	MST_ID(5)
#define   USB_MST_ID	MST_ID(6)
#define   USB2_MST_ID	MST_ID(18)
#define   USB3_MST_ID	MST_ID(17)
#define   ADDR_BNDRY(x)	(((x) & 0xf) << 21)
#define   INACTIVITY_TIMEOUT(x)	(((x) & 0xffff) << 0)

unsigned long tegra_bootloader_fb_start;
unsigned long tegra_bootloader_fb_size;
unsigned long tegra_fb_start;
unsigned long tegra_fb_size;
unsigned long tegra_fb2_start;
unsigned long tegra_fb2_size;
unsigned long tegra_carveout_start;
unsigned long tegra_carveout_size;
unsigned long tegra_lp0_vec_start;
unsigned long tegra_lp0_vec_size;
bool tegra_lp0_vec_relocate;
unsigned long tegra_grhost_aperture = ~0ul;
static   bool is_tegra_debug_uart_hsport;
static struct board_info pmu_board_info;

static int pmu_core_edp = 1200;	/* default 1.2V EDP limit */
static int board_panel_type;
static int modem_id;
void (*tegra_reset)(char mode, const char *cmd);

/* WARNING: There is implicit client of pllp_out3 like i2c, uart, dsi
 * and so this clock (pllp_out3) should never be disabled.
 */
static __initdata struct tegra_clk_init_table common_clk_init_table[] = {
	/* set up clocks that should always be on */
	/* name		parent		rate		enabled */
	{ "clk_m",	NULL,		0,		true },
#ifdef CONFIG_TEGRA_SILICON_PLATFORM
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	{ "pll_p",	NULL,		216000000,	true },
	{ "pll_p_out1",	"pll_p",	28800000,	true },
	{ "pll_p_out2",	"pll_p",	48000000,	true },
	{ "pll_p_out3",	"pll_p",	72000000,	true },
	{ "pll_m",	"clk_m",	600000000,	true },
	{ "pll_m_out1",	"pll_m",	120000000,	true },
	{ "sclk",	"pll_m_out1",	40000000,	true },
	{ "hclk",	"sclk",		40000000,	true },
	{ "pclk",	"hclk",		40000000,	true },
#else
	{ "pll_p",	NULL,		408000000,	true },
	{ "pll_p_out1",	"pll_p",	9600000,	true },
	{ "pll_p_out2",	"pll_p",	48000000,	true },
	{ "pll_p_out3",	"pll_p",	102000000,	true },
	{ "pll_m_out1",	"pll_m",	275000000,	false},
	{ "pll_p_out4",	"pll_p",	102000000,	true },
	{ "sclk",	"pll_p_out4",	102000000,	true },
	{ "hclk",	"sclk",		102000000,	true },
	{ "pclk",	"hclk",		51000000,	true },
#endif
#else
	{ "pll_p",	NULL,		216000000,	true },
	{ "pll_p_out1",	"pll_p",	28800000,	true },
	{ "pll_p_out2",	"pll_p",	48000000,	true },
	{ "pll_p_out3",	"pll_p",	72000000,	true },
	{ "pll_m_out1",	"pll_m",	275000000,	false},
	{ "pll_c",	NULL,		ULONG_MAX,	false },
	{ "pll_c_out1",	"pll_c",	208000000,	false },
	{ "pll_p_out4",	"pll_p",	108000000,	true },
	{ "sclk",	"pll_p_out4",	108000000,	true },
	{ "hclk",	"sclk",		108000000,	true },
	{ "pclk",	"hclk",		54000000,	true },
#endif
	{ "pll_x",	NULL,		0,		true },
	{ "cpu",	NULL,		0,		true },
	{ "emc",	NULL,		0,		true },
	{ "csite",	NULL,		0,		true },
	{ "timer", 	NULL,		0,		true },
	{ "kfuse",	NULL,		0,		true },
	{ "fuse",	NULL,		0,		true },
	{ "rtc",	NULL,		0,		true },

	/* set frequencies of some device clocks */
	{ "pll_u",	NULL,		480000000,	false },
	{ "sdmmc1",	"pll_p",	48000000,	false},
	{ "sdmmc3",	"pll_p",	48000000,	false},
	{ "sdmmc4",	"pll_p",	48000000,	false},
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	{ "cbus",	"pll_c",	416000000,	false },
	{ "pll_c_out1",	"pll_c",	208000000,	false },
#endif
	{ NULL,		NULL,		0,		0},
};

void __init tegra_init_cache(void)
{
#ifdef CONFIG_CACHE_L2X0
	void __iomem *p = IO_ADDRESS(TEGRA_ARM_PERIF_BASE) + 0x3000;
	u32 aux_ctrl;

#ifndef CONFIG_TRUSTED_FOUNDATIONS
#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
	writel(0x331, p + L2X0_TAG_LATENCY_CTRL);
	writel(0x441, p + L2X0_DATA_LATENCY_CTRL);
	writel(2, p + L2X0_PWR_CTRL);

#elif defined(CONFIG_ARCH_TEGRA_3x_SOC)
#ifdef CONFIG_TEGRA_SILICON_PLATFORM
	/* PL310 RAM latency is CPU dependent. NOTE: Changes here
	   must also be reflected in __cortex_a9_l2x0_restart */

	if (is_lp_cluster()) {
		writel(0x221, p + L2X0_TAG_LATENCY_CTRL);
		writel(0x221, p + L2X0_DATA_LATENCY_CTRL);
	} else {
		writel(0x777, p + L2X0_TAG_LATENCY_CTRL);
		writel(0x777, p + L2X0_DATA_LATENCY_CTRL);
	}
#else
	writel(0x770, p + L2X0_TAG_LATENCY_CTRL);
	writel(0x770, p + L2X0_DATA_LATENCY_CTRL);
#endif
#endif
#endif
	aux_ctrl = readl(p + L2X0_CACHE_TYPE);
	aux_ctrl = (aux_ctrl & 0x700) << (17-8);
	aux_ctrl |= 0x6C000001;
	l2x0_init(p, aux_ctrl, 0x8200c3fe);
#endif
}

static void __init tegra_init_power(void)
{
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
        tegra_powergate_partition_with_clk_off(TEGRA_POWERGATE_SATA);
#endif
	tegra_powergate_partition_with_clk_off(TEGRA_POWERGATE_PCIE);
}

static inline unsigned long gizmo_readl(unsigned long offset)
{
	return readl(IO_TO_VIRT(TEGRA_AHB_GIZMO_BASE + offset));
}

static inline void gizmo_writel(unsigned long value, unsigned long offset)
{
	writel(value, IO_TO_VIRT(TEGRA_AHB_GIZMO_BASE + offset));
}

static void __init tegra_init_ahb_gizmo_settings(void)
{
	unsigned long val;

	val = gizmo_readl(AHB_GIZMO_AHB_MEM);
	val |= ENB_FAST_REARBITRATE | IMMEDIATE | DONT_SPLIT_AHB_WR;
	gizmo_writel(val, AHB_GIZMO_AHB_MEM);

	val = gizmo_readl(AHB_GIZMO_USB);
	val |= IMMEDIATE;
	gizmo_writel(val, AHB_GIZMO_USB);

	val = gizmo_readl(AHB_GIZMO_USB2);
	val |= IMMEDIATE;
	gizmo_writel(val, AHB_GIZMO_USB2);

	val = gizmo_readl(AHB_GIZMO_USB3);
	val |= IMMEDIATE;
	gizmo_writel(val, AHB_GIZMO_USB3);

	val = gizmo_readl(AHB_ARBITRATION_PRIORITY_CTRL);
	val |= PRIORITY_SELECT_USB | PRIORITY_SELECT_USB2 | PRIORITY_SELECT_USB3
				| AHB_PRIORITY_WEIGHT(7);
	gizmo_writel(val, AHB_ARBITRATION_PRIORITY_CTRL);

	val = gizmo_readl(AHB_MEM_PREFETCH_CFG1);
	val &= ~MST_ID(~0);
	val |= PREFETCH_ENB | AHBDMA_MST_ID | ADDR_BNDRY(0xc) | INACTIVITY_TIMEOUT(0x1000);
	gizmo_writel(val, AHB_MEM_PREFETCH_CFG1);

	val = gizmo_readl(AHB_MEM_PREFETCH_CFG2);
	val &= ~MST_ID(~0);
	val |= PREFETCH_ENB | USB_MST_ID | ADDR_BNDRY(0xc) | INACTIVITY_TIMEOUT(0x1000);
	gizmo_writel(val, AHB_MEM_PREFETCH_CFG2);

	val = gizmo_readl(AHB_MEM_PREFETCH_CFG3);
	val &= ~MST_ID(~0);
	val |= PREFETCH_ENB | USB3_MST_ID | ADDR_BNDRY(0xc) | INACTIVITY_TIMEOUT(0x1000);
	gizmo_writel(val, AHB_MEM_PREFETCH_CFG3);

	val = gizmo_readl(AHB_MEM_PREFETCH_CFG4);
	val &= ~MST_ID(~0);
	val |= PREFETCH_ENB | USB2_MST_ID | ADDR_BNDRY(0xc) | INACTIVITY_TIMEOUT(0x1000);
	gizmo_writel(val, AHB_MEM_PREFETCH_CFG4);
}

static bool console_flushed;

static void tegra_pm_flush_console(void)
{
	if (console_flushed)
		return;
	console_flushed = true;

	pr_emerg("Restarting %s\n", linux_banner);

	if (!try_acquire_console_sem()) {
		release_console_sem();
		return;
	}

	mdelay(50);

	local_irq_disable();
	if (try_acquire_console_sem())
		pr_emerg("%s: Console was locked! Busting\n", __func__);
	else
		pr_emerg("%s: Console was locked!\n", __func__);

	release_console_sem();
}

static void tegra_pm_restart(char mode, const char *cmd)
{
	tegra_pm_flush_console();
	arm_machine_restart(mode, cmd);
}

#ifdef CONFIG_TRUSTED_FOUNDATIONS
void callGenericSMC(u32 param0, u32 param1, u32 param2);
#endif

void tegra_cpu_reset_handler_enable(void)
{
	extern void __tegra_cpu_reset_handler(void);
	extern void __tegra_cpu_reset_handler_start(void);
	extern void __tegra_cpu_reset_handler_end(void);
#ifndef CONFIG_TRUSTED_FOUNDATIONS
	unsigned long reg;
	void __iomem *evp_cpu_reset =
		IO_ADDRESS(TEGRA_EXCEPTION_VECTORS_BASE + 0x100);
	void __iomem *sb_ctrl = IO_ADDRESS(TEGRA_SB_BASE);
#endif
	void __iomem *iram_base = IO_ADDRESS(TEGRA_IRAM_BASE);

	unsigned long cpu_reset_handler_size =
		__tegra_cpu_reset_handler_end - __tegra_cpu_reset_handler_start;
	unsigned long cpu_reset_handler_offset =
		__tegra_cpu_reset_handler - __tegra_cpu_reset_handler_start;

	BUG_ON(cpu_reset_handler_size > TEGRA_RESET_HANDLER_SIZE);

	memcpy(iram_base, (void *)__tegra_cpu_reset_handler_start, cpu_reset_handler_size);
#ifdef CONFIG_TRUSTED_FOUNDATIONS
	callGenericSMC(0xFFFFF200,
		TEGRA_RESET_HANDLER_BASE + cpu_reset_handler_offset, 0);
#else
	/* NOTE: This must be the one and only write to the CPU reset
		 vector in the entire system. */
	writel(TEGRA_RESET_HANDLER_BASE + cpu_reset_handler_offset,
		evp_cpu_reset);
	wmb();

	/* Prevent further modifications to the physical reset vector.
	   NOTE: Has no effect on chips prior to Tegra3. */
	reg = readl(sb_ctrl);
	reg |= 2;
	writel(reg, sb_ctrl);
	wmb();
#endif
}

void tegra_cpu_reset_handler_flush(bool l1cache)
{
	unsigned long first = (unsigned long)cpu_online_mask;
	unsigned long last  = (unsigned long)cpu_present_mask;

	if (first > last)
		swap(first,last);
	last += sizeof(struct cpumask);

	/* Push all of this data out to the L3 memory system. */
	if (l1cache) {
		__cpuc_coherent_kern_range(first, last);
		__cpuc_coherent_kern_range(
			(unsigned long)&__tegra_cpu_reset_handler_data[0],
			(unsigned long)&__tegra_cpu_reset_handler_data[TEGRA_RESET_DATA_SIZE]);
	}

	outer_clean_range(__pa(first), __pa(last));
	outer_clean_range(__pa(&__tegra_cpu_reset_handler_data[0]),
			  __pa(&__tegra_cpu_reset_handler_data[TEGRA_RESET_DATA_SIZE]));
}

#ifdef CONFIG_PM
static unsigned long tegra_cpu_reset_handler_bckup[TEGRA_RESET_DATA_SIZE];

void tegra_cpu_reset_handler_save(void)
{
	unsigned int i;
	for (i = 0; i < TEGRA_RESET_DATA_SIZE; i++)
		tegra_cpu_reset_handler_bckup[i] =
					tegra_cpu_reset_handler_ptr[i];
}

void tegra_cpu_reset_handler_restore(void)
{
	unsigned int i;
	for (i = 0; i < TEGRA_RESET_DATA_SIZE; i++)
		tegra_cpu_reset_handler_ptr[i] =
					tegra_cpu_reset_handler_bckup[i];
}
#endif

void __init tegra_cpu_reset_handler_init(void)
{
#ifdef CONFIG_SMP
	/* Mark the initial boot CPU as initialized. */
	__tegra_cpu_reset_handler_data[TEGRA_RESET_MASK_INIT] |= 1;

	__tegra_cpu_reset_handler_data[TEGRA_RESET_MASK_PRESENT_PTR] =
		virt_to_phys((void*)cpu_present_mask);
	__tegra_cpu_reset_handler_data[TEGRA_RESET_STARTUP_SECONDARY] =
		virt_to_phys((void*)tegra_secondary_startup);
#ifdef CONFIG_HOTPLUG_CPU
	__tegra_cpu_reset_handler_data[TEGRA_RESET_MASK_ONLINE_PTR] =
		virt_to_phys((void*)cpu_online_mask);
	__tegra_cpu_reset_handler_data[TEGRA_RESET_STARTUP_HOTPLUG] =
		virt_to_phys((void*)tegra_hotplug_startup);
#endif
#endif
#ifdef CONFIG_PM
	__tegra_cpu_reset_handler_data[TEGRA_RESET_STARTUP_LP1] =
		TEGRA_IRAM_CODE_AREA;
#endif
	__tegra_cpu_reset_handler_data[TEGRA_RESET_STARTUP_LP2] =
		virt_to_phys((void*)tegra_lp2_startup);

	tegra_cpu_reset_handler_flush(true);
	tegra_cpu_reset_handler_enable();
}

void __init tegra_common_init(void)
{
	arm_pm_restart = tegra_pm_restart;
#ifndef CONFIG_SMP
	/* For SMP system, initializing the reset dispatcher here is too
	   late. For non-SMP systems, the function that initializes the
	   reset dispatcher is not called, so do it here for non-SMP. */
	tegra_cpu_reset_handler_init();

	/* The same goes for initialization of CPU dynamic power gating.
	   For SMP systems it's done in SMP initialization. For non-SMP
	   systems it's done here. */
	tegra_cpu_dynamic_power_init();
#endif
	tegra_init_fuse();
	tegra_init_clock();
	tegra_clk_init_from_table(common_clk_init_table);
	tegra_init_power();
	tegra_init_cache();
	tegra_dma_init();
	tegra_init_apb_dma();
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	tegra_mc_init(); /* !!!FIXME!!! Change Tegra3 behavior to match Tegra2 */
#endif
	tegra_init_ahb_gizmo_settings();
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	tegra_tsensor_init();
#endif
	tegra_pinmux_init();
}

static int __init tegra_bootloader_fb_arg(char *options)
{
	char *p = options;

	tegra_bootloader_fb_size = memparse(p, &p);
	if (*p == '@')
		tegra_bootloader_fb_start = memparse(p+1, &p);

	pr_info("Found tegra_fbmem: %08lx@%08lx\n",
		tegra_bootloader_fb_size, tegra_bootloader_fb_start);

	return 0;
}
early_param("tegra_fbmem", tegra_bootloader_fb_arg);

static int __init tegra_lp0_vec_arg(char *options)
{
	char *p = options;
	unsigned long start;
	unsigned long size;

	size = memparse(p, &p);
	if (*p == '@') {
		start = memparse(p+1, &p);

		if (size && start) {
			tegra_lp0_vec_size  = size;
			tegra_lp0_vec_start = start;
		}
	}

	return 0;
}
early_param("lp0_vec", tegra_lp0_vec_arg);

enum panel_type get_panel_type(void)
{
	return board_panel_type;
}
static int __init tegra_board_panel_type(char *options)
{
	if (!strcmp(options, "lvds"))
		board_panel_type = panel_type_lvds;
	else if (!strcmp(options, "dsi"))
		board_panel_type = panel_type_dsi;
	else
		return 0;
	return 1;
}
__setup("panel=", tegra_board_panel_type);

int get_core_edp(void)
{
	return pmu_core_edp;
}
static int __init tegra_pmu_core_edp(char *options)
{
	char *p = options;
	int core_edp = memparse(p, &p);
	if (core_edp != 0)
		pmu_core_edp = core_edp;
	return 1;
}
__setup("core_edp_mv=", tegra_pmu_core_edp);

static int __init tegra_debug_uartport(char *info)
{
	if (!strcmp(info, "hsport"))
		is_tegra_debug_uart_hsport = true;
	else if (!strcmp(info, "lsport"))
		is_tegra_debug_uart_hsport = false;

	return 1;
}

bool is_tegra_debug_uartport_hs(void)
{
	return is_tegra_debug_uart_hsport;
}

__setup("debug_uartport=", tegra_debug_uartport);

void tegra_get_board_info(struct board_info *bi)
{
	bi->board_id = (system_serial_high >> 16) & 0xFFFF;
	bi->sku = (system_serial_high) & 0xFFFF;
	bi->fab = (system_serial_low >> 24) & 0xFF;
	bi->major_revision = (system_serial_low >> 16) & 0xFF;
	bi->minor_revision = (system_serial_low >> 8) & 0xFF;
}

static int __init tegra_pmu_board_info(char *info)
{
	char *p = info;
	pmu_board_info.board_id = memparse(p, &p);
	pmu_board_info.sku = memparse(p+1, &p);
	pmu_board_info.fab = memparse(p+1, &p);
	pmu_board_info.major_revision = memparse(p+1, &p);
	pmu_board_info.minor_revision = memparse(p+1, &p);
	return 1;
}

void tegra_get_pmu_board_info(struct board_info *bi)
{
	memcpy(bi, &pmu_board_info, sizeof(struct board_info));
}

__setup("pmuboard=", tegra_pmu_board_info);

static int __init tegra_modem_id(char *id)
{
	char *p = id;

	modem_id = memparse(p, &p);
	return 1;
}

int tegra_get_modem_id(void)
{
	return modem_id;
}

__setup("modem_id=", tegra_modem_id);

/*
 * Tegra has a protected aperture that prevents access by most non-CPU
 * memory masters to addresses above the aperture value.  Enabling it
 * secures the CPU's memory from the GPU, except through the GART.
 */
void __init tegra_protected_aperture_init(unsigned long aperture)
{
#ifndef CONFIG_NVMAP_ALLOW_SYSMEM
	void __iomem *mc_base = IO_ADDRESS(TEGRA_MC_BASE);
	pr_info("Enabling Tegra protected aperture at 0x%08lx\n", aperture);
	writel(aperture, mc_base + MC_SECURITY_CFG2);
#else
	pr_err("Tegra protected aperture disabled because nvmap is using "
		"system memory\n");
#endif
}

/*
 * Due to conflicting restrictions on the placement of the framebuffer,
 * the bootloader is likely to leave the framebuffer pointed at a location
 * in memory that is outside the grhost aperture.  This function will move
 * the framebuffer contents from a physical address that is anywher (lowmem,
 * highmem, or outside the memory map) to a physical address that is outside
 * the memory map.
 */
void tegra_move_framebuffer(unsigned long to, unsigned long from,
	unsigned long size)
{
	struct page *page;
	void __iomem *to_io;
	void *from_virt;
	unsigned long i;

	BUG_ON(PAGE_ALIGN((unsigned long)to) != (unsigned long)to);
	BUG_ON(PAGE_ALIGN(from) != from);
	BUG_ON(PAGE_ALIGN(size) != size);

	to_io = ioremap(to, size);
	if (!to_io) {
		pr_err("%s: Failed to map target framebuffer\n", __func__);
		return;
	}

	if (pfn_valid(page_to_pfn(phys_to_page(from)))) {
		for (i = 0 ; i < size; i += PAGE_SIZE) {
			page = phys_to_page(from + i);
			from_virt = kmap(page);
			memcpy_toio(to_io + i, from_virt, PAGE_SIZE);
			kunmap(page);
		}
	} else {
		void __iomem *from_io = ioremap(from, size);
		if (!from_io) {
			pr_err("%s: Failed to map source framebuffer\n",
				__func__);
			goto out;
		}

		for (i = 0; i < size; i += 4)
			writel(readl(from_io + i), to_io + i);

		iounmap(from_io);
	}
out:
	iounmap(to_io);
}

#ifdef CONFIG_TEGRA_IOVMM_SMMU
/* Support for Tegra3 A01 chip mask that needs to have SMMU IOVA reside in
 * the upper half of 4GB IOVA space. A02 and after use the bottom 1GB and
 * do not need to reserve memory.
 */
#define SUPPORT_TEGRA_3_IOVMM_SMMU_A01
#endif

void __init tegra_reserve(unsigned long carveout_size, unsigned long fb_size,
	unsigned long fb2_size)
{
#ifdef SUPPORT_TEGRA_3_IOVMM_SMMU_A01
	extern struct platform_device tegra_smmu_device;
	int smmu_reserved = 0;
	struct resource *smmu_window =
		    platform_get_resource_byname(&tegra_smmu_device,
						IORESOURCE_MEM, "smmu");
#endif

	if (carveout_size) {
		tegra_carveout_start = memblock_end_of_DRAM() - carveout_size;
		if (memblock_remove(tegra_carveout_start, carveout_size)) {
			pr_err("Failed to remove carveout %08lx@%08lx "
				"from memory map\n",
				carveout_size, tegra_carveout_start);
			tegra_carveout_start = 0;
			tegra_carveout_size = 0;
		} else
			tegra_carveout_size = carveout_size;
	}

	if (fb2_size) {
		tegra_fb2_start = memblock_end_of_DRAM() - fb2_size;
		if (memblock_remove(tegra_fb2_start, fb2_size)) {
			pr_err("Failed to remove second framebuffer "
				"%08lx@%08lx from memory map\n",
				fb2_size, tegra_fb2_start);
			tegra_fb2_start = 0;
			tegra_fb2_size = 0;
		} else
			tegra_fb2_size = fb2_size;
	}

	if (fb_size) {
		tegra_fb_start = memblock_end_of_DRAM() - fb_size;
		if (memblock_remove(tegra_fb_start, fb_size)) {
			pr_err("Failed to remove framebuffer %08lx@%08lx "
				"from memory map\n",
				fb_size, tegra_fb_start);
			tegra_fb_start = 0;
			tegra_fb_size = 0;
		} else
			tegra_fb_size = fb_size;
	}

	if (tegra_fb_size)
		tegra_grhost_aperture = tegra_fb_start;

	if (tegra_fb2_size && tegra_fb2_start < tegra_grhost_aperture)
		tegra_grhost_aperture = tegra_fb2_start;

	if (tegra_carveout_size && tegra_carveout_start < tegra_grhost_aperture)
		tegra_grhost_aperture = tegra_carveout_start;

#ifdef SUPPORT_TEGRA_3_IOVMM_SMMU_A01
	if (!smmu_window) {
		pr_err("No SMMU resources\n");
	} else {
		size_t smmu_window_size;

		if (tegra_get_revision() == TEGRA_REVISION_A01) {
			smmu_window->start = TEGRA_SMMU_BASE_A01;
			smmu_window->end   = TEGRA_SMMU_BASE_A01 +
						TEGRA_SMMU_SIZE_A01 - 1;
		}
		smmu_window_size = smmu_window->end + 1 - smmu_window->start;
		if (smmu_window->start >= 0x80000000) {
			if (memblock_reserve(smmu_window->start,
						smmu_window_size))
				pr_err(
			"Failed to reserve SMMU I/O VA window %08lx@%08lx\n",
				(unsigned long)smmu_window_size,
				(unsigned long)smmu_window->start);
			else
				smmu_reserved = 1;
		}
	}
#endif

	if (tegra_lp0_vec_size &&
	   (tegra_lp0_vec_start < memblock_end_of_DRAM())) {
		if (memblock_reserve(tegra_lp0_vec_start, tegra_lp0_vec_size)) {
			pr_err("Failed to reserve lp0_vec %08lx@%08lx\n",
				tegra_lp0_vec_size, tegra_lp0_vec_start);
			tegra_lp0_vec_start = 0;
			tegra_lp0_vec_size = 0;
		}
		tegra_lp0_vec_relocate = false;
	} else
		tegra_lp0_vec_relocate = true;

	/*
	 * We copy the bootloader's framebuffer to the framebuffer allocated
	 * above, and then free this one.
	 * */
	if (tegra_bootloader_fb_size) {
		tegra_bootloader_fb_size = PAGE_ALIGN(tegra_bootloader_fb_size);
		if (memblock_reserve(tegra_bootloader_fb_start,
				tegra_bootloader_fb_size)) {
			pr_err("Failed to reserve bootloader frame buffer "
				"%08lx@%08lx\n", tegra_bootloader_fb_size,
				tegra_bootloader_fb_start);
			tegra_bootloader_fb_start = 0;
			tegra_bootloader_fb_size = 0;
		}
	}

	pr_info("Tegra reserved memory:\n"
		"LP0:                    %08lx - %08lx\n"
		"Bootloader framebuffer: %08lx - %08lx\n"
		"Framebuffer:            %08lx - %08lx\n"
		"2nd Framebuffer:        %08lx - %08lx\n"
		"Carveout:               %08lx - %08lx\n",
		tegra_lp0_vec_start,
		tegra_lp0_vec_size ?
			tegra_lp0_vec_start + tegra_lp0_vec_size - 1 : 0,
		tegra_bootloader_fb_start,
		tegra_bootloader_fb_size ?
			tegra_bootloader_fb_start + tegra_bootloader_fb_size - 1 : 0,
		tegra_fb_start,
		tegra_fb_size ?
			tegra_fb_start + tegra_fb_size - 1 : 0,
		tegra_fb2_start,
		tegra_fb2_size ?
			tegra_fb2_start + tegra_fb2_size - 1 : 0,
		tegra_carveout_start,
		tegra_carveout_size ?
			tegra_carveout_start + tegra_carveout_size - 1 : 0);

#ifdef SUPPORT_TEGRA_3_IOVMM_SMMU_A01
	if (smmu_reserved)
		pr_info("SMMU:                   %08x - %08x\n",
			smmu_window->start, smmu_window->end);
#endif
}

void __init tegra_release_bootloader_fb(void)
{
	/* Since bootloader fb is reserved in common.c, it is freed here. */
	if (tegra_bootloader_fb_size)
		if (memblock_free(tegra_bootloader_fb_start,
						tegra_bootloader_fb_size))
			pr_err("Failed to free bootloader fb.\n");
}

#ifdef CONFIG_TEGRA_CONVSERVATIVE_GOV_ON_EARLYSUPSEND
static char cpufreq_gov_default[32];
static char *cpufreq_gov_conservative = "conservative";
static char *cpufreq_sysfs_place_holder="/sys/devices/system/cpu/cpu%i/cpufreq/scaling_governor";
static char *cpufreq_gov_conservative_param="/sys/devices/system/cpu/cpufreq/conservative/%s";

static void cpufreq_set_governor(char *governor)
{
	struct file *scaling_gov = NULL;
	mm_segment_t old_fs;
	char    buf[128];
	int i;
	loff_t offset = 0;

	if (governor == NULL)
		return;

	/* change to KERNEL_DS address limit */
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	for_each_online_cpu(i) {
		sprintf(buf, cpufreq_sysfs_place_holder, i);
		scaling_gov = filp_open(buf, O_RDWR, 0);
		if (IS_ERR_OR_NULL(scaling_gov)) {
			pr_err("%s. Can't open %s\n", __func__, buf);
		} else {
			if (scaling_gov->f_op != NULL &&
				scaling_gov->f_op->write != NULL)
				scaling_gov->f_op->write(scaling_gov,
						governor,
						strlen(governor),
						&offset);
			else
				pr_err("f_op might be null\n");

			filp_close(scaling_gov, NULL);
		}
	}
	set_fs(old_fs);
}

void cpufreq_save_default_governor(void)
{
	struct file *scaling_gov = NULL;
	mm_segment_t old_fs;
	char    buf[128];
	loff_t offset = 0;

	/* change to KERNEL_DS address limit */
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	buf[127] = 0;
	sprintf(buf, cpufreq_sysfs_place_holder,0);
	scaling_gov = filp_open(buf, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(scaling_gov)) {
		pr_err("%s. Can't open %s\n", __func__, buf);
	} else {
		if (scaling_gov->f_op != NULL &&
			scaling_gov->f_op->read != NULL)
			scaling_gov->f_op->read(scaling_gov,
					cpufreq_gov_default,
					32,
					&offset);
		else
			pr_err("f_op might be null\n");

		filp_close(scaling_gov, NULL);
	}
	set_fs(old_fs);
}

void cpufreq_restore_default_governor(void)
{
	cpufreq_set_governor(cpufreq_gov_default);
}

void cpufreq_set_conservative_governor_param(int up_th, int down_th)
{
	struct file *gov_param = NULL;
	static char buf[128],parm[8];
	loff_t offset = 0;
	mm_segment_t old_fs;

	if (up_th <= down_th) {
		printk(KERN_ERR "%s: up_th(%d) is lesser than down_th(%d)\n",
			__func__, up_th, down_th);
		return;
	}

	/* change to KERNEL_DS address limit */
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	sprintf(parm, "%d", up_th);
	sprintf(buf, cpufreq_gov_conservative_param ,"up_threshold");
	gov_param = filp_open(buf, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(gov_param)) {
		pr_err("%s. Can't open %s\n", __func__, buf);
	} else {
		if (gov_param->f_op != NULL &&
			gov_param->f_op->write != NULL)
			gov_param->f_op->write(gov_param,
					parm,
					strlen(parm),
					&offset);
		else
			pr_err("f_op might be null\n");

		filp_close(gov_param, NULL);
	}

	sprintf(parm, "%d", down_th);
	sprintf(buf, cpufreq_gov_conservative_param ,"down_threshold");
	gov_param = filp_open(buf, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(gov_param)) {
		pr_err("%s. Can't open %s\n", __func__, buf);
	} else {
		if (gov_param->f_op != NULL &&
			gov_param->f_op->write != NULL)
			gov_param->f_op->write(gov_param,
					parm,
					strlen(parm),
					&offset);
		else
			pr_err("f_op might be null\n");

		filp_close(gov_param, NULL);
	}
	set_fs(old_fs);
}

void cpufreq_set_conservative_governor(void)
{
	cpufreq_set_governor(cpufreq_gov_conservative);
}
#endif /* CONFIG_TEGRA_CONVSERVATIVE_GOV_ON_EARLYSUPSEND */
