/*
 * linux/drivers/pcmcia/pxa/pxa2xx_asusp535.c
 *
 * Copyright (C) 2008 Alexander Tarasikov <alex_dfr@mail.ru>
 * Copyright (C) 2010 Oleg Kravchenko <oleg@kaa.org.ua>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#include <asm/mach-types.h>
#include <mach/asusp535.h>

#include "soc_common.h"

static int asusp535_pcmcia_hw_init(struct soc_pcmcia_socket *skt)
{
	int ret;

	ret = gpio_request(GPIO_ASUSP535_PCMCIA_POWER, "PCMCIA Power");
	if (ret)
		goto err1;
	gpio_set_value_cansleep(GPIO_ASUSP535_PCMCIA_POWER, 0);

	ret = gpio_request(GPIO_ASUSP535_PCMCIA_RESET, "PCMCIA Reset");
	if (ret)
		goto err2;
	gpio_direction_output(GPIO_ASUSP535_PCMCIA_RESET, 1);

	ret = gpio_request(GPIO_ASUSP535_PCMCIA_READY, "PCMCIA Ready");
	if (ret)
		goto err3;
	gpio_direction_input(GPIO_ASUSP535_PCMCIA_READY);

	skt->socket.pci_irq = IRQ_GPIO(GPIO_ASUSP535_PCMCIA_READY);

	return 0;

err3:
	gpio_free(GPIO_ASUSP535_PCMCIA_RESET);
err2:
	gpio_free(GPIO_ASUSP535_PCMCIA_POWER);
err1:
	return ret;
}

static void asusp535_pcmcia_hw_shutdown(struct soc_pcmcia_socket *skt)
{
	gpio_free(GPIO_ASUSP535_PCMCIA_READY);

	gpio_direction_output(GPIO_ASUSP535_PCMCIA_RESET, 1);
	gpio_free(GPIO_ASUSP535_PCMCIA_RESET);

	gpio_set_value_cansleep(GPIO_ASUSP535_PCMCIA_POWER, 0);
	gpio_free(GPIO_ASUSP535_PCMCIA_POWER);
}

static void asusp535_pcmcia_socket_state(struct soc_pcmcia_socket *skt,
				       struct pcmcia_state *state)
{
	state->detect = 1; /* always inserted */
	state->ready  = !!gpio_get_value(GPIO_ASUSP535_PCMCIA_READY);
	state->bvd1   = 1;
	state->bvd2   = 1;
	state->wrprot = 0;
	state->vs_3v  = 1;
	state->vs_Xv  = 0;
}

static int asusp535_pcmcia_configure_socket
(
	struct soc_pcmcia_socket *skt,
	const socket_state_t *state
)
{
	gpio_set_value_cansleep(GPIO_ASUSP535_PCMCIA_POWER, 1);

	if(state->flags & SS_RESET)
	{
		gpio_set_value(GPIO_ASUSP535_PCMCIA_RESET, 1);
		udelay(30);
		gpio_set_value(GPIO_ASUSP535_PCMCIA_RESET, 0);
	}

	if (state->flags & SS_POWERON)
		printk(KERN_INFO "pcmcia_configure_socket: on\n");

	if ((!state->flags) & SS_POWERON)
		printk(KERN_INFO "pcmcia_configure_socket: off\n");

	return 0;
}

static void asusp535_pcmcia_socket_init(struct soc_pcmcia_socket *skt)
{
}

static void asusp535_pcmcia_socket_suspend(struct soc_pcmcia_socket *skt)
{
}

static struct pcmcia_low_level asusp535_pcmcia_ops __initdata = {
	.owner				= THIS_MODULE,
	.first				= 0,
	.nr					= 1,
	.hw_init			= asusp535_pcmcia_hw_init,
	.hw_shutdown		= asusp535_pcmcia_hw_shutdown,
	.socket_state		= asusp535_pcmcia_socket_state,
	.configure_socket	= asusp535_pcmcia_configure_socket,
	.socket_init		= asusp535_pcmcia_socket_init,
	.socket_suspend		= asusp535_pcmcia_socket_suspend,
};

static struct platform_device *asusp535_pcmcia_device;

static int __init asusp535_pcmcia_init(void)
{
	int ret;

	if (!machine_is_asusp535())
		return -ENODEV;

	asusp535_pcmcia_device = platform_device_alloc("pxa2xx-pcmcia", -1);

	if (!asusp535_pcmcia_device)
		return -ENOMEM;

	ret = platform_device_add_data
	(
		asusp535_pcmcia_device,
		&asusp535_pcmcia_ops,
		sizeof(asusp535_pcmcia_ops)
	);

	if(!ret)
	{
		printk(KERN_INFO "Registering Asus P535 PCMCIA interface.\n");
		ret = platform_device_add(asusp535_pcmcia_device);
	}

	if(ret)
		platform_device_put(asusp535_pcmcia_device);

	return ret;
}

static void __exit asusp535_pcmcia_exit(void)
{
	platform_device_unregister(asusp535_pcmcia_device);
}

module_init(asusp535_pcmcia_init);
module_exit(asusp535_pcmcia_exit);

MODULE_AUTHOR("Oleg Kravchenko <oleg@kaa.org.ua>");
MODULE_DESCRIPTION("Asus P535 PCMCIA Driver");
MODULE_ALIAS("platform:pxa2xx-pcmcia");
MODULE_LICENSE("GPL");
