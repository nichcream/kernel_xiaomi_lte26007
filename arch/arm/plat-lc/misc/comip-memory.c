
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <asm/setup.h>
#include <asm/sections.h>
#include <asm/pgtable.h>
#include <plat/memory.h>
#include <plat/comip-memory.h>
#include <plat/comip-setup.h>

phys_addr_t g_comip_memory_base;
phys_addr_t g_comip_memory_size;
phys_addr_t g_comip_kernel_memory_base;
phys_addr_t g_comip_kernel_memory_size;
phys_addr_t g_comip_memory2_base;
phys_addr_t g_comip_memory2_size;
phys_addr_t g_comip_kernel_memory2_base;
phys_addr_t g_comip_kernel_memory2_size;
phys_addr_t g_comip_on2_memory_base;
phys_addr_t g_comip_on2_memory_size;
unsigned char g_comip_on2_memory_fix;
phys_addr_t g_comip_fb_memory_base;
phys_addr_t g_comip_fb_memory_size;
unsigned char g_comip_fb_memory_num;
unsigned char g_comip_fb_memory_fix;

static void comip_add_mem(void)
{
	phys_addr_t reserved_size = 0;

	if (g_comip_fb_memory_fix)
		reserved_size += (g_comip_fb_memory_size * g_comip_fb_memory_num);
	if (g_comip_on2_memory_fix)
		reserved_size += g_comip_on2_memory_size;

	g_comip_kernel_memory_size = g_comip_memory_size - \
		(phys_addr_t)MODEM_MEMORY_SIZE - reserved_size;
	g_comip_kernel_memory2_size = g_comip_memory2_size;

	//g_comip_kernel_memory_size = g_comip_kernel_memory_size & PGDIR_MASK;
	//g_comip_kernel_memory2_size = g_comip_kernel_memory2_size & PGDIR_MASK;

	arm_add_memory(g_comip_kernel_memory_base, g_comip_kernel_memory_size);

	if (g_comip_kernel_memory2_size)
		arm_add_memory(g_comip_kernel_memory2_base, g_comip_kernel_memory2_size);

	#define MLM(b, s) b, ((b) + (s)), (s) >> 20
	printk("Physical memory layout:\n");

	if (g_comip_memory_size)
		printk("    DRAM Bank1  : 0x%09llx - 0x%09llx  (%4lld MB)\n",
			MLM((u64)g_comip_memory_base, (u64)g_comip_memory_size));

	if (g_comip_memory2_size)
		printk("    DRAM Bank2  : 0x%09llx - 0x%09llx  (%4lld MB)\n",
			MLM((u64)g_comip_memory2_base, (u64)g_comip_memory2_size));

	if (g_comip_kernel_memory_size)
		printk("    Kernel Bank1: 0x%09llx - 0x%09llx  (%4lld MB)\n",
			MLM((u64)g_comip_kernel_memory_base, (u64)g_comip_kernel_memory_size));

	if (g_comip_kernel_memory2_size)
		printk("    Kernel Bank2: 0x%09llx - 0x%09llx  (%4lld MB)\n",
			MLM((u64)g_comip_kernel_memory2_base, (u64)g_comip_kernel_memory2_size));

	if (g_comip_on2_memory_fix && g_comip_on2_memory_size)
		printk("    ON2         : 0x%09llx - 0x%09llx  (%4lld MB)\n",
			MLM((u64)g_comip_on2_memory_base, (u64)g_comip_on2_memory_size));

	if (g_comip_fb_memory_fix && (g_comip_fb_memory_size * g_comip_fb_memory_num))
		printk("    Framebuffer : 0x%09llx - 0x%09llx  (%4lld MB)\n",
			MLM((u64)g_comip_fb_memory_base, (u64)g_comip_fb_memory_size * (u64)g_comip_fb_memory_num));
}

static int __init comip_parse_tag_mem_ext(const struct tag *tag)
{
	struct tag_mem_ext *mem = (struct tag_mem_ext *)&tag->u;

	g_comip_memory_base = (phys_addr_t)mem->dram_base;
	g_comip_memory_size = (phys_addr_t)mem->dram_size;
	g_comip_kernel_memory_base = (phys_addr_t)MODEM_MEMORY_SIZE;
	g_comip_memory2_base = (phys_addr_t)mem->dram2_base;
	g_comip_memory2_size = (phys_addr_t)mem->dram2_size;
	g_comip_kernel_memory2_base = (phys_addr_t)g_comip_memory2_base;
	g_comip_on2_memory_base = (phys_addr_t)mem->on2_base;
	g_comip_on2_memory_size = (phys_addr_t)mem->on2_size;
	g_comip_on2_memory_fix = mem->on2_fix;
	g_comip_fb_memory_base = (phys_addr_t)mem->fb_base;
	g_comip_fb_memory_size = (phys_addr_t)mem->fb_size;
	g_comip_fb_memory_num = mem->fb_num;
	g_comip_fb_memory_fix = mem->fb_fix;

	if (!COMIP_MEMORY_SIZE
			|| (!ON2_MEMORY_SIZE && ON2_MEMORY_FIX)
			|| (!FB_MEMORY_SIZE && FB_MEMORY_FIX))
		goto fatal_error;

	comip_add_mem();

	return 0;

fatal_error:
	panic("Invalid boot parameters!");
	return 1;
}
__tagtable(ATAG_MEM_EXT, comip_parse_tag_mem_ext);

