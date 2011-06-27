/*
 * GSM, GPS and Bluetooth power control for Asus P535
 *
 * Copyright (c) 2011 Oleg Kravchenko
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/rfkill.h>

#include <mach/asusp535.h>

static struct rfkill *gsm_rfk = NULL;
static struct rfkill *gps_rfk = NULL;
static struct rfkill *bt_rfk = NULL;

typedef int (*request_gpio_t)(void);
typedef void (*free_gpio_t)(void);

static int asusp535_rfk_dev_probe
(
	struct platform_device *dev,
	struct rfkill **rfk,
	const char* name,
	enum rfkill_type rfk_type,
	request_gpio_t request_gpio,
	free_gpio_t free_gpio,
	const struct rfkill_ops *ops
)
{
	int ret;

	ret = request_gpio();
	if(ret)
		goto err_request;

	*rfk = rfkill_alloc(name, &dev->dev, rfk_type, ops, NULL);
	if(!*rfk)
	{
		ret = -ENOMEM;
		goto err_rf_alloc;
	}

	ret = rfkill_register(*rfk);
	if(ret)
		goto err_rf_reg;

	return ret;

err_rf_reg:
	rfkill_destroy(*rfk);
	*rfk = NULL;

err_rf_alloc:
	free_gpio();

err_request:
	return ret;
}

static int set_block_gsm(void *data, bool blocked)
{
	printk(KERN_INFO "GSM going: %s\n", blocked ? "off" : "on");

	if(!blocked) {
		gpio_direction_output(GPIO_ASUSP535_GSM_HWRST, 1);
		msleep(600);
		gpio_direction_output(GPIO_ASUSP535_GSM_RST, 1);
		msleep(100);
		gpio_direction_output(GPIO_ASUSP535_GSM_PWR, 1);
	} else {
		gpio_direction_output(GPIO_ASUSP535_GSM_PWR, 0);
	}

	return 0;
}

static int request_gpio_gsm(void)
{
	int ret;

	ret = gpio_request(GPIO_ASUSP535_GSM_PWR, "GSM_PWR");
	if(ret)
		goto err_gpio;

	ret = gpio_direction_output(GPIO_ASUSP535_GSM_PWR, 0);
	if(ret)
		goto err_pwr;

	ret = gpio_request(GPIO_ASUSP535_GSM_RST, "GSM_RST");
	if(ret)
		goto err_pwr;

	ret = gpio_direction_output(GPIO_ASUSP535_GSM_RST, 0);
	if(ret)
		goto err_rst;

	ret = gpio_request(GPIO_ASUSP535_GSM_HWRST, "GSM_HWRST");
	if(ret)
		goto err_rst;

	ret = gpio_direction_output(GPIO_ASUSP535_GSM_HWRST, 0);
	if(ret)
		goto err_hwrst;

	return 0;

err_hwrst:
	gpio_free(GPIO_ASUSP535_GSM_HWRST);
	
err_rst:
	gpio_free(GPIO_ASUSP535_GSM_RST);

err_pwr:
	gpio_free(GPIO_ASUSP535_GSM_PWR);

err_gpio:
	return ret;
}

static void free_gpio_gsm(void)
{
	gpio_free(GPIO_ASUSP535_GSM_PWR);
	gpio_free(GPIO_ASUSP535_GSM_RST);
	gpio_free(GPIO_ASUSP535_GSM_HWRST);
}

static const struct rfkill_ops gsm_rfkill_ops = {
	.set_block = set_block_gsm,
};

static int set_block_gps(void *data, bool blocked)
{
	printk(KERN_INFO "GPS going: %s\n", blocked ? "off" : "on");

	if(!blocked) {
		gpio_direction_output(GPIO_ASUSP535_GPS_PWR1, 1);
		gpio_direction_output(GPIO_ASUSP535_GPS_PWR2, 1);
	} else {
		gpio_direction_output(GPIO_ASUSP535_GPS_PWR2, 0);
		gpio_direction_output(GPIO_ASUSP535_GPS_PWR1, 0);
	}

	return 0;
}

static int request_gpio_gps(void)
{
	int ret;

	ret = gpio_request(GPIO_ASUSP535_GPS_PWR2, "GPS_PWR2");
	if(ret)
		goto err_gpio;

	ret = gpio_direction_output(GPIO_ASUSP535_GPS_PWR2, 0);
	if(ret)
		goto err_pwr1;

	ret = gpio_request(GPIO_ASUSP535_GPS_PWR1, "GPS_PWR1");
	if(ret)
		goto err_pwr1;

	ret = gpio_direction_output(GPIO_ASUSP535_GPS_PWR1, 0);
	if(ret)
		goto err_pwr2;

	return 0;

err_pwr2:
	gpio_free(GPIO_ASUSP535_GPS_PWR1);

err_pwr1:
	gpio_free(GPIO_ASUSP535_GPS_PWR2);

err_gpio:
	return ret;
}

static void free_gpio_gps(void)
{
	gpio_free(GPIO_ASUSP535_GPS_PWR1);
	gpio_free(GPIO_ASUSP535_GPS_PWR2);
}

static const struct rfkill_ops gps_rfkill_ops = {
	.set_block = set_block_gps,
};

static int set_block_bt(void *data, bool blocked)
{
	printk(KERN_INFO "BT going: %s\n", blocked ? "off" : "on");

	if(!blocked) {
		gpio_direction_output(GPIO_ASUSP535_BT_PWR1, 1);
		msleep(5);
		gpio_direction_output(GPIO_ASUSP535_BT_PWR2, 1);
	} else {
		gpio_direction_output(GPIO_ASUSP535_BT_PWR1, 0);
		gpio_direction_output(GPIO_ASUSP535_BT_PWR2, 0);
	}

	return 0;
}

static int request_gpio_bt(void)
{
	int ret;

	ret = gpio_request(GPIO_ASUSP535_BT_PWR1, "BT_PWR1");
	if(ret)
		goto err_gpio;

	ret = gpio_direction_output(GPIO_ASUSP535_BT_PWR1, 0);
	if(ret)
		goto err_pwr1;

	ret = gpio_request(GPIO_ASUSP535_BT_PWR2, "BT_PWR2");
	if(ret)
		goto err_pwr1;

	ret = gpio_direction_output(GPIO_ASUSP535_BT_PWR2, 0);
	if(ret)
		goto err_pwr2;

	return 0;

err_pwr2:
	gpio_free(GPIO_ASUSP535_BT_PWR2);

err_pwr1:
	gpio_free(GPIO_ASUSP535_BT_PWR1);

err_gpio:
	return ret;
}

static void free_gpio_bt(void)
{
	gpio_free(GPIO_ASUSP535_BT_PWR1);
	gpio_free(GPIO_ASUSP535_BT_PWR2);
}

static const struct rfkill_ops bt_rfkill_ops = {
	.set_block = set_block_bt,
};

static int asusp535_rfk_probe(struct platform_device *dev)
{
	int ret;

	ret = asusp535_rfk_dev_probe(dev, &gsm_rfk, "asusp535_gsm",
		RFKILL_TYPE_UWB, request_gpio_gsm, free_gpio_gsm, &gsm_rfkill_ops);
	if(ret)
		goto err_gsm;

	ret = asusp535_rfk_dev_probe(dev, &gps_rfk, "asusp535_gps",
		RFKILL_TYPE_GPS, request_gpio_gps, free_gpio_gps, &gps_rfkill_ops);
	if(ret)
		goto err_gps;

	ret = asusp535_rfk_dev_probe(dev, &gps_rfk, "asusp535_bt",
		RFKILL_TYPE_BLUETOOTH, request_gpio_bt, free_gpio_bt, &bt_rfkill_ops);
	if(ret)
		goto err_bt;

err_bt:
	rfkill_unregister(gps_rfk);
	rfkill_destroy(gps_rfk);
	free_gpio_gps();
	gps_rfk = NULL;

err_gps:
	rfkill_unregister(gsm_rfk);
	rfkill_destroy(gsm_rfk);
	free_gpio_gsm();
	gsm_rfk = NULL;

err_gsm:	
	return ret;
}

static int __devexit asusp535_rfk_remove(struct platform_device *dev)
{
	if(gsm_rfk) {
		rfkill_unregister(gsm_rfk);
		rfkill_destroy(gsm_rfk);
		gsm_rfk = NULL;

		free_gpio_gsm();
	}

	if(gps_rfk) {
		rfkill_unregister(gps_rfk);
		rfkill_destroy(gps_rfk);
		gps_rfk = NULL;

		free_gpio_gps();
	}

	if(bt_rfk) {
		rfkill_unregister(bt_rfk);
		rfkill_destroy(bt_rfk);
		bt_rfk = NULL;

		free_gpio_bt();
	}

	return 0;
}

static struct platform_driver asusp535_rfk_driver = {
	.probe = asusp535_rfk_probe,
	.remove = __devexit_p(asusp535_rfk_remove),
	.driver = {
		.name = "asusp535-rfk",
		.owner = THIS_MODULE,
	},
};

static int __init asusp535_rfk_init(void)
{
	return platform_driver_register(&asusp535_rfk_driver);
}

static void __exit asusp535_rfk_exit(void)
{
	platform_driver_unregister(&asusp535_rfk_driver);
}

module_init(asusp535_rfk_init);
module_exit(asusp535_rfk_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Oleg Kravchenko <oleg@kaa.org.ua>");
MODULE_DESCRIPTION("GSM, GPS and Bluetooth power control driver for Asus P535");
