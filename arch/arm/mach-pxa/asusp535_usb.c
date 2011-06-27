/*
 * Support for Asus P535 PDA
 *
 * (C) 2009 Oleg Kravchenko <oleg@kaa.org.ua>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <asm/mach/arch.h>
#include <asm/mach-types.h>

#include <mach/hardware.h>
#include <mach/pxa-regs.h>
#include <mach/ohci.h>

static int asusp535_ohci_init(struct device *dev)
{
	UHCHCON = 0x97;//(UHCHCON_HCFS_USBOPERATIONAL | /*UHCHCON_PLE |*/ UHCHCON_CLE | UHCHCON_CBSR41);//0x97 (USBOPERATIONAL | CLE(may be not set?) | PLE | CBSR=0x3)
	UHCINTE = 0x80000053;//(UHCINT_MIE | UHCINT_RHSC | UHCINT_UE | UHCINT_WDH | UHCINT_SO);//0x80000053;// (MIE | RHSC | UE | WDH | SO)
	UHCINTD = 0x80000053;//(UHCINT_MIE | UHCINT_RHSC | UHCINT_UE | UHCINT_WDH | UHCINT_SO);//0x80000053;// (MIE | RHSC | UE | WDH | SO)
	UHCFMI = 0x27782edf;
	//UHCFMR = 0x2d6b;// (no need to set ?)
	//UHCFMN = 0xd1bc;// (no need to set ?)
	//intel says typical val is 0x3e67. wince sets to 0x2a2f
	UHCPERS = 0x3e67;//0x2a2f
	UHCLS = 0x628;// (lsthreshold=0x628)
	UHCRHDA = 0x04000b02;// (numberdownstreamports=3 | psm=1 | overcurrentprotection=1 | noovercurrentprotection=1 | powerontopowergoodtime(bit26)=1)
	UHCRHDB = 0x0;
	UHCRHS = 0x0;
	UHCRHPS1 = 0x100;// (port power on)
	UHCRHPS2 = 0x100;// (port power on)
	UHCRHPS3 = 0x100;// (port power on)
	UHCSTAT = 0x0;
	UHCHR = (UHCHR_SSEP3 | UHCHR_PCPL | UHCHR_CGR); //0x884 (Sleep Standby Enable for Port3 | power control polarity low | clock generation reset inactive)
	UHCHIE = 0x0;
	UHCHIT = 0x0;

	return 0;
}

static void asusp535_ohci_exit(struct device *dev)
{
	UHCRHPS1 = 0x0;// (port power off)
	UHCRHPS2 = 0x0;// (port power off)
	UHCRHPS3 = 0x0;// (port power off)
}

static struct pxaohci_platform_data asusp535_ohci_platform_data = {
	.init		= asusp535_ohci_init,
	.exit		= asusp535_ohci_exit,
	.port_mode	= /*PMM_NPS_MODE,//*/PMM_PERPORT_MODE,
};

static int __init asusp535_usb_init(void)
{
	if(!machine_is_asusp535())
		return -ENODEV;

	printk("asusp535: Enabling USB-Host controller\n");

	pxa_set_ohci_info(&asusp535_ohci_platform_data);

	return 0;
}

static void __exit asusp535_usb_exit(void)
{
	printk("asusp535: Disabling USB-Host controller\n");
}

module_init(asusp535_usb_init);
module_exit(asusp535_usb_exit);

MODULE_AUTHOR("Oleg Kravchenko <oleg@kaa.org.ua>");
MODULE_DESCRIPTION("Asus P535 USB Host status driver");
MODULE_LICENSE("GPL");
