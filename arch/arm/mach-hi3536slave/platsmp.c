/******************************************************************************
 *    COPYRIGHT (C) 2013 Hisilicon
 *    All rights reserved.
 * ***
 *    Create by Czyong 2013-12-19
 *
******************************************************************************/
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/smp.h>
#include <linux/io.h>
#include <asm/cacheflush.h>
#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <asm/unified.h>
#include <asm/smp_scu.h>
#include <mach/early-debug.h>

#include "platsmp.h"
#include "hotplug.h"

static DEFINE_SPINLOCK(boot_lock);

/*****************************************************************************/

static void __cpuinit write_pen_release(int val)
{
	pen_release = val;
	smp_wmb();
	__cpuc_flush_dcache_area((void *)&pen_release, sizeof(pen_release));
	outer_clean_range(__pa(&pen_release), __pa(&pen_release + 1));
}
/*****************************************************************************/

static int __cpuinit hi3536_boot_secondary(unsigned int cpu,
					struct task_struct *idle)
{
	unsigned long timeout;
	set_scu_boot_addr(0x00000000,
		(unsigned int)virt_to_phys(hi3536_secondary_startup));

	/*
	 * set synchronisation state between this boot processor
	 * and the secondary one
	 */
	spin_lock(&boot_lock);

	hi3536_scu_power_up(cpu);

	/*
	 * The secondary processor is waiting to be released from
	 * the holding pen - release it, then wait for it to flag
	 * that it has been released by resetting pen_release.
	 *
	 * Note that "pen_release" is the hardware CPU ID, whereas
	 * "cpu" is Linux's internal ID.
	 */
	write_pen_release(cpu);

	/*
	 * Send the secondary CPU a soft interrupt, thereby causing
	 * the boot monitor to read the system wide flags register,
	 * and branch to the address found there.
	 */
	arch_send_wakeup_ipi_mask(cpumask_of(cpu));

	/*
	 * Send the secondary CPU a soft interrupt, thereby causing
	 * the boot monitor to read the system wide flags register,
	 * and branch to the address found there.
	 */
	timeout = jiffies + (5 * HZ);
	while (time_before(jiffies, timeout)) {
		smp_rmb();
		if (pen_release == -1)
			break;

		udelay(10);
	}

	/*
	 * now the secondary core is starting up let it run its
	 * calibrations, then wait for it to finish
	 */
	spin_unlock(&boot_lock);
	return pen_release != -1 ? -ENOSYS : 0;
}
/*****************************************************************************/

static void __cpuinit hi3536_secondary_init(unsigned int cpu)
{
	/*
	 * 1. enable L1 prefetch                       [2]
	 * 2. enable L2 prefetch hint                  [1]a
	 * 3. enable write full line of zeros mode.    [3]a
	 * 4. enable allocation in one cache way only. [8]
	 *   a: This feature must be enabled only when the slaves
	 *      connected on the Cortex-A17 AXI master port support it.
	 */
	asm volatile (
	"	mrc	p15, 0, r0, c1, c0, 1\n"
	"	orr	r0, r0, #0x0104\n"
	"	orr	r0, r0, #0x02\n"
	"	mcr	p15, 0, r0, c1, c0, 1\n"
	  :
	  :
	  : "r0", "cc");

	/*
	 * let the primary processor know we're out of the
	 * pen, then head off into the C entry point
	 */
	write_pen_release(-1);

	/*
	 * Synchronise with the boot thread.
	 */
	spin_lock(&boot_lock);
	spin_unlock(&boot_lock);
}
/*****************************************************************************/

static void __init hi3536_smp_init_cpus(void)
{
	unsigned int i, ncores;

	asm volatile (
	"	mrc	p15, 1, %0, c9, c0, 4\n"
	: "=r"(ncores)
	);
	ncores = (ncores & 0x3) + 1;
	/* sanity check */
	if (ncores > NR_CPUS) {
		pr_warn(
			"Realview: no. of cores (%d) greater than configured "
			"maximum of %d - clipping\n",
			ncores, NR_CPUS);
		ncores = NR_CPUS;
	}

	for (i = 0; i < ncores; i++)
		set_cpu_possible(i, true);
}
/*****************************************************************************/

static void __init hi3536_smp_prepare_cpus(unsigned int max_cpus)
{
/* Scu should be open here,but scu is open default,
 * so wo do nothing here,and just return.
 */
	return;
}
/*****************************************************************************/

struct smp_operations hi3536_smp_ops __initdata = {
	.smp_init_cpus = hi3536_smp_init_cpus,
	.smp_prepare_cpus = hi3536_smp_prepare_cpus,
	.smp_secondary_init = hi3536_secondary_init,
	.smp_boot_secondary = hi3536_boot_secondary,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_die = hi3536_cpu_die,
#endif
};
