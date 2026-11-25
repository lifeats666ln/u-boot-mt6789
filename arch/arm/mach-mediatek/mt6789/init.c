// SPDX-License-Identifier: GPL-2.0

#include <env.h>
#include <fdtdec.h>
#include <init.h>
#include <lmb.h>
#include <log.h>
#include <asm/io.h>
#include <asm/armv8/mmu.h>
#include <asm/system.h>
#include <asm/global_data.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/sizes.h>

#define INFRACFG_AO_BASE	0x10001000
#define MODULE_SW_CG_2_CLR	0xa8
#define SSUSB_CG		BIT(1)

DECLARE_GLOBAL_DATA_PTR;

int print_cpuinfo(void)
{
	printk("CPU:   MediaTek Helio G99 (MT6789)\n");
	return 0;
}

int dram_init(void)
{
	int ret;

	ret = fdtdec_setup_mem_size_base();
	if (ret)
		return ret;

	// Fix ram_size to 2GB, above that things start to break
	gd->ram_size = get_ram_size((void *)gd->ram_base, SZ_2G);

	return 0;
}

phys_addr_t board_get_usable_ram_top(phys_size_t total_size)
{
	/* Keep relocation below wifi/ccci/SCP/atf-log no-map at 0xbf000000-0xc0000000 */
	return min_t(phys_addr_t, gd->ram_top, 0xbf000000);
}

void reset_cpu(void)
{
	psci_system_reset();
}

static int mt6789_set_load_addr(const char *name, phys_size_t size)
{
	phys_addr_t addr;
	int ret;

	ret = lmb_alloc_mem(LMB_MEM_ALLOC_ANY, SZ_2M, &addr, size, LMB_NONE);
	if (ret) {
		log_warning("Cannot reserve memory for %s\n", name);
		return ret;
	}

	return env_set_hex(name, addr);
}

int board_late_init(void)
{
	/* clk-mt6789 is not built yet: open the SSUSB gate for fastboot */
	writel(SSUSB_CG, INFRACFG_AO_BASE + MODULE_SW_CG_2_CLR);

	/* lmb already holds U-Boot and the DT reserved-memory regions */
	mt6789_set_load_addr("kernel_addr_r", SZ_128M);
	mt6789_set_load_addr("ramdisk_addr_r", SZ_128M);

#ifdef CONFIG_FASTBOOT_BUF_SIZE
	/* bootm parses Android boot images from ${loadaddr} */
	if (!mt6789_set_load_addr("fastboot_addr_r", CONFIG_FASTBOOT_BUF_SIZE))
		env_set("loadaddr", env_get("fastboot_addr_r"));
#endif

	return 0;
}

static struct mm_region vicky_mem_map[] = {
	{
		/* Peripheral region */
		.virt = 0x00000000UL,
		.phys = 0x00000000UL,
		.size = 0x40000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
		PTE_BLOCK_NON_SHARE |
		PTE_BLOCK_PXN | PTE_BLOCK_UXN,
	},
	{
		/* DRAM region 0 */
		.virt = 0x040000000UL,
		.phys = 0x040000000UL,
		.size = 0x026f00000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) | PTE_BLOCK_OUTER_SHARE
	},
	{
		/* TEE Reserved 0 */
		.virt = 0x70000000UL,
		.phys = 0x70000000UL,
		.size = 0x04600000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
		PTE_BLOCK_NON_SHARE |
		PTE_BLOCK_PXN | PTE_BLOCK_UXN,
	},
	{
		/* DRAM region 1 */
		.virt = 0x70000000UL,
		.phys = 0x70000000UL,
		.size = 0x08000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) | PTE_BLOCK_OUTER_SHARE
	},
	{
		/* TEE Reserved 1 */
		.virt = 0x7de00000UL,
		.phys = 0x7de00000UL,
		.size = 0x00800000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
		PTE_BLOCK_NON_SHARE |
		PTE_BLOCK_PXN | PTE_BLOCK_UXN,
	},
	{
		/* DRAM region 2 */
		.virt = 0x7ffff000UL,
		.phys = 0x7ffff000UL,
		.size = 0x7e0d1000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) | PTE_BLOCK_OUTER_SHARE
	},
	{
		/* Framebuffer Reserved */
		.virt = 0xfe0d0000UL,
		.phys = 0xfe0d0000UL,
		.size = 0x01df0000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL_NC) | PTE_BLOCK_INNER_SHARE |PTE_BLOCK_PXN | PTE_BLOCK_UXN
	},
	{
		/* DRAM region 3 */
		.virt = 0xfecd0000UL,
		.phys = 0xfecd0000UL,
		.size = 0x4132f000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) | PTE_BLOCK_OUTER_SHARE
	},
	{
		/* DRAM Controller 0 Reserved */
		.virt = 0x03ffff000UL,
		.phys = 0x03ffff000UL,
		.size = 0x000001000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
		PTE_BLOCK_NON_SHARE |
		PTE_BLOCK_PXN | PTE_BLOCK_UXN,
	},
	{
		/* DRAM region 4 */
		.virt = 0x140000000UL,
		.phys = 0x140000000UL,
		.size = 0x0c0000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) | PTE_BLOCK_OUTER_SHARE
	},
	{
		0,
	}
};

struct mm_region *mem_map = vicky_mem_map;
