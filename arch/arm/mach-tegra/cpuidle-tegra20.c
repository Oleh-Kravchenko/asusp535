/*
 * CPU idle driver for Tegra CPUs
 *
 * Copyright (c) 2010-2012, NVIDIA Corporation.
 * Copyright (c) 2011 Google, Inc.
 * Author: Colin Cross <ccross@android.com>
 *         Gary King <gking@nvidia.com>
 *
 * Rework for 3.3 by Peter De Schrijver <pdeschrijver@nvidia.com>
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
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/clockchips.h>

#include <asm/cpuidle.h>
#include <asm/proc-fns.h>
#include <asm/suspend.h>
#include <asm/smp_plat.h>

#include "pm.h"
#include "sleep.h"

#ifdef CONFIG_PM_SLEEP
static int tegra20_idle_lp2(struct cpuidle_device *dev,
			    struct cpuidle_driver *drv,
			    int index);
#endif

static struct cpuidle_state tegra_idle_states[] = {
	[0] = ARM_CPUIDLE_WFI_STATE_PWR(600),
#ifdef CONFIG_PM_SLEEP
	[1] = {
		.enter			= tegra20_idle_lp2,
		.exit_latency		= 5000,
		.target_residency	= 10000,
		.power_usage		= 0,
		.flags			= CPUIDLE_FLAG_TIME_VALID,
		.name			= "powered-down",
		.desc			= "CPU power gated",
	},
#endif
};

static struct cpuidle_driver tegra_idle_driver = {
	.name = "tegra_idle",
	.owner = THIS_MODULE,
	.en_core_tk_irqen = 1,
};

static DEFINE_PER_CPU(struct cpuidle_device, tegra_idle_device);

#ifdef CONFIG_PM_SLEEP
#ifdef CONFIG_SMP
static bool tegra20_idle_enter_lp2_cpu_1(struct cpuidle_device *dev,
					 struct cpuidle_driver *drv,
					 int index)
{
	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ENTER, &dev->cpu);

	cpu_suspend(0, tegra20_sleep_cpu_secondary_finish);

	tegra20_cpu_clear_resettable();

	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_EXIT, &dev->cpu);

	return true;
}
#else
static inline bool tegra20_idle_enter_lp2_cpu_1(struct cpuidle_device *dev,
						struct cpuidle_driver *drv,
						int index)
{
	return true;
}
#endif

static int tegra20_idle_lp2(struct cpuidle_device *dev,
			    struct cpuidle_driver *drv,
			    int index)
{
	u32 cpu = is_smp() ? cpu_logical_map(dev->cpu) : dev->cpu;
	bool entered_lp2 = false;

	local_fiq_disable();

	tegra_set_cpu_in_lp2(cpu);
	cpu_pm_enter();

	if (cpu == 0)
		cpu_do_idle();
	else
		entered_lp2 = tegra20_idle_enter_lp2_cpu_1(dev, drv, index);

	cpu_pm_exit();
	tegra_clear_cpu_in_lp2(cpu);

	local_fiq_enable();

	smp_rmb();

	return entered_lp2 ? index : 0;
}
#endif

int __init tegra20_cpuidle_init(void)
{
	int ret;
	unsigned int cpu;
	struct cpuidle_device *dev;
	struct cpuidle_driver *drv = &tegra_idle_driver;

	drv->state_count = ARRAY_SIZE(tegra_idle_states);
	memcpy(drv->states, tegra_idle_states,
			drv->state_count * sizeof(drv->states[0]));

	ret = cpuidle_register_driver(&tegra_idle_driver);
	if (ret) {
		pr_err("CPUidle driver registration failed\n");
		return ret;
	}

	for_each_possible_cpu(cpu) {
		dev = &per_cpu(tegra_idle_device, cpu);
		dev->cpu = cpu;

		dev->state_count = drv->state_count;
		ret = cpuidle_register_device(dev);
		if (ret) {
			pr_err("CPU%u: CPUidle device registration failed\n",
				cpu);
			return ret;
		}
	}
	return 0;
}
