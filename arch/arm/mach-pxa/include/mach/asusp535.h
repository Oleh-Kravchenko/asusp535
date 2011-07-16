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

#ifndef __ASUSP535_H
#define __ASUSP535_H

/* LCD */
#define GPIO_ASUSP535_LCD_POWER1	44
#define GPIO_ASUSP535_LCD_POWER2	81

/* PCA9535 */
#define EGPIO_BASE					128
#define EGPIO_BANK(x)				(EGPIO_BASE + (x))

/* GPIO keys */
#define GPIO_P535_POWER_KEY			0
#define GPIO_ASUSP535_KEY_HP		16
#define GPIO_P535_HOLD_SWITCH		90
#define GPIO_P535_BATTERY_DOOR		99

/* GENERAL */
#define GPIO_P535_TOUCHSCREEN_IRQ	113

/* SD/MMC */
#define GPIO_ASUSP535_SD_DETECT_N	10
#define GPIO_ASUSP535_SD_POWER		85

/* USB?? */
#define GPIO_ASUSP535_USB_CABLE_DETECT	11
#define GPIO_P535_USB_CHARGE_DETECT	96
#define GPIO_P535_USB_AC_DETECT		97

#define GPIO_ASUSP535_USB_PULLUP	84

/* BLUETOOTH: BCM2045A */
#define GPIO_ASUSP535_BT_PWR1	54
#define GPIO_ASUSP535_BT_PWR2	EGPIO_BANK(7)

/* PCMCIA */
#define GPIO_ASUSP535_PCMCIA_POWER	EGPIO_BANK(6)
#define GPIO_ASUSP535_PCMCIA_RESET	80
#define GPIO_ASUSP535_PCMCIA_READY	53

/* SOUND */
#define GPIO_P535_SOUND_MIX		88
#define GPIO_ASUSP535_HP_INSERT	14

/* LEDS */
#define GPIO_ASUSP535_LED_TORCH		22
#define GPIO_ASUSP535_LED_VIBRA		EGPIO_BANK(2)
#define GPIO_ASUSP535_LED_KEYLED	EGPIO_BANK(5)
#define GPIO_ASUSP535_LED_WARNING	EGPIO_BANK(14)

/* GSM */
#define GPIO_ASUSP535_GSM_HWRST		EGPIO_BANK(11)
#define GPIO_ASUSP535_GSM_PWR		EGPIO_BANK(12)
#define GPIO_ASUSP535_GSM_RST		EGPIO_BANK(13)
#define GPIO_ASUSP535_GSM_READY1	19
#define GPIO_ASUSP535_GSM_READY2	91

/* GPS */
#define GPIO_ASUSP535_GPS_PWR1	EGPIO_BANK(1)
#define GPIO_ASUSP535_GPS_PWR2	EGPIO_BANK(15)

/* CHARGING */
#define GPIO_ASUSP535_CHARHING		EGPIO_BANK(9)


#define ASUSP535_DBG(format, args...) \
	printk(KERN_INFO "%s:%d %s: " format, __FILE__, __LINE__, __func__, ##args)

#endif /* __ASUSP535_H */
