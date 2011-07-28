/*
 * Support for Asus P535 PDA
 *
 * (C) 2008 Alexander Tarasikov <alex_dfr@mail.ru>
 * (C) 2011 Oleg Kravchenko <oleg@kaa.org.ua>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>

#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/pda_power.h>
#include <linux/wm97xx.h>
#include <linux/i2c/pca953x.h>
#include <linux/pwm_backlight.h>
#include <linux/usb/gpio_vbus.h>

#include <asm/gpio.h>
#include <asm/mach/map.h>
#include <asm/mach/arch.h>
#include <asm/mach-types.h>
#include <mach/pxa27x-udc.h>
#include <mach/mmc.h>
#include <mach/udc.h>
#include <mach/pxafb.h>
#include <mach/pxa2xx-regs.h>
#include <mach/pxa27x_keypad.h>
#include <mach/mfp-pxa27x.h>
#include <mach/asusp535.h>
#include <mach/ohci.h>

#include <plat/i2c.h>

#include "generic.h"
#include "devices.h"

/****************************************************************************
 * GPIO CONFIGURATION
 ***************************************************************************/
static unsigned long asusp535_pin_config[] __initdata = {
	/* AC97 */
	GPIO28_AC97_BITCLK,
	GPIO29_AC97_SDATA_IN_0,
	GPIO30_AC97_SDATA_OUT,
	GPIO31_AC97_SYNC,
	GPIO98_AC97_SYSCLK,

	/* headphone */
	GPIO14_GPIO | WAKEUP_ON_EDGE_BOTH,
	GPIO16_GPIO | WAKEUP_ON_EDGE_RISE,

	/* FFUART (GSM) */
	GPIO34_FFUART_RXD,
	GPIO35_FFUART_CTS,
	GPIO36_FFUART_DCD,
	GPIO37_FFUART_DSR,
	GPIO38_FFUART_RI,
	GPIO39_FFUART_TXD,
	GPIO40_FFUART_DTR,
	GPIO41_FFUART_RTS,

	/* I2C */
	GPIO117_I2C_SCL,
	GPIO118_I2C_SDA,

	/* Keypad */
	GPIO100_KP_MKIN_0,
	GPIO101_KP_MKIN_1,
	GPIO102_KP_MKIN_2,
	GPIO103_KP_MKOUT_0,
	GPIO104_KP_MKOUT_1,
	GPIO105_KP_MKOUT_2,
	GPIO106_KP_MKOUT_3,
	GPIO107_KP_MKOUT_4,
	GPIO108_KP_MKOUT_5,

	/* MMC/SD */
	GPIO10_GPIO | WAKEUP_ON_EDGE_BOTH,		/* detect */
	MFP_CFG_OUT(GPIO85, AF0, DRIVE_LOW),	/* power */
	GPIO32_MMC_CLK,
	GPIO92_MMC_DAT_0,
	GPIO109_MMC_DAT_1,
	GPIO110_MMC_DAT_2,
	GPIO111_MMC_DAT_3,
	GPIO112_MMC_CMD,

	/* LCD */
	GPIO58_LCD_LDD_0,
	GPIO59_LCD_LDD_1,
	GPIO60_LCD_LDD_2,
	GPIO61_LCD_LDD_3,
	GPIO62_LCD_LDD_4,
	GPIO63_LCD_LDD_5,
	GPIO64_LCD_LDD_6,
	GPIO65_LCD_LDD_7,
	GPIO66_LCD_LDD_8,
	GPIO67_LCD_LDD_9,
	GPIO68_LCD_LDD_10,
	GPIO69_LCD_LDD_11,
	GPIO70_LCD_LDD_12,
	GPIO71_LCD_LDD_13,
	GPIO72_LCD_LDD_14,
	GPIO73_LCD_LDD_15,
	GPIO74_LCD_FCLK,
	GPIO75_LCD_LCLK,
	GPIO76_LCD_PCLK,
	GPIO77_LCD_BIAS,

	/* TOUCHSCREEN */
	GPIO113_GPIO | WAKEUP_ON_EDGE_RISE,

	/* LCD brightness */
	GPIO79_PWM2_OUT,

	/* LCD power */
	MFP_CFG_OUT(GPIO44, AF0, DRIVE_LOW), /* GPIO44 */
	MFP_CFG_OUT(GPIO81, AF0, DRIVE_LOW), /* GPIO81 */

	/* PCMCIA (WiFi) */
	GPIO15_nPCE_1,
	GPIO48_nPOE,
	GPIO49_nPWE,
	GPIO50_nPIOR,
	GPIO51_nPIOW,
	GPIO55_nPREG,
	GPIO56_nPWAIT,
	GPIO57_nIOIS16,
	GPIO53_GPIO | WAKEUP_ON_EDGE_FALL, /* PCMCIA ready */
	MFP_CFG_OUT(GPIO80, AF0, DRIVE_LOW), /* PCMCIA reset */

	/* QCI */
	GPIO12_CIF_DD_7,
	GPIO23_CIF_MCLK,
	GPIO24_CIF_FV,
	GPIO25_CIF_LV,
	GPIO26_CIF_PCLK,
	GPIO27_CIF_DD_0,
	GPIO93_CIF_DD_6,
	GPIO94_CIF_DD_5,
	GPIO95_CIF_DD_4,
	GPIO114_CIF_DD_1,
	GPIO115_CIF_DD_3,
	GPIO116_CIF_DD_2,

	/* GPS */
	GPIO42_BTUART_RXD,
	GPIO43_BTUART_TXD,

	/* STUART (IrDA, not used) */
	GPIO46_STUART_RXD,

	/* BLUETOOTH */
	MFP_CFG_OUT(GPIO54, AF0, DRIVE_LOW), /* Bluetooth Power */

	/* USB */
	GPIO11_GPIO | WAKEUP_ON_EDGE_BOTH,
	MFP_CFG_OUT(GPIO84, AF0, DRIVE_LOW),
	MFP_CFG_OUT(GPIO89, AF2, DRIVE_LOW),
};

/****************************************************************************
 * I2C GPIO EXPANDER
 ***************************************************************************/
static struct pca953x_platform_data gpio_exp[] = {
	[0] = {
		.gpio_base	= EGPIO_BASE,
	},
};

struct i2c_board_info asusp535_i2c_board_info[] = {
	{
		.type		= "pca9535",
		.addr		= 0x20,
		.platform_data	= &gpio_exp[0],
	},
};

/****************************************************************************
 * LEDS
 ***************************************************************************/
static struct gpio_led asusp535_gpio_leds[] = {
	{
		.name			= "asusp535:keylight",
		.default_trigger	= "backlight",
		.gpio			= GPIO_ASUSP535_LED_KEYLED,
	},
		{
		.name			= "asusp535:torch",
		.default_trigger	= "none",
		.gpio			= GPIO_ASUSP535_LED_TORCH,
	},
	{
		.name			= "asusp535:vibra",
		.default_trigger	= "none",
		.gpio			= GPIO_ASUSP535_LED_VIBRA,
	},
	{
		.name			= "asusp535:warning",
		.default_trigger	= "none",
		.gpio			= GPIO_ASUSP535_LED_WARNING,
	},
};

static struct gpio_led_platform_data asusp535_gpio_leds_platform_data = {
	.leds		= asusp535_gpio_leds,
	.num_leds	= ARRAY_SIZE(asusp535_gpio_leds),
};

static struct platform_device asusp535_leds = {
	.name	= "leds-gpio",
	.id	= -1,
	.dev	= {
		.platform_data	= &asusp535_gpio_leds_platform_data,
	},
};

/****************************************************************************
 * MMC/SD
 ***************************************************************************/
static struct pxamci_platform_data asusp535_mci_info = {
	.detect_delay_ms	= 200,
	.ocr_mask			= MMC_VDD_32_33 | MMC_VDD_33_34,
	.gpio_card_detect	= GPIO_ASUSP535_SD_DETECT_N,
	.gpio_power			= GPIO_ASUSP535_SD_POWER,
	.gpio_card_ro		= -1,
};

/****************************************************************************
 * UDC
 ***************************************************************************/

static struct pxa2xx_udc_mach_info asusp535_udc_info = {
	.gpio_pullup		= GPIO_ASUSP535_USB_PULLUP,
	.gpio_vbus			= -1,
};

static struct gpio_vbus_mach_info gpio_vbus_data = {
	.gpio_pullup		= -1,
	.gpio_vbus			= GPIO_ASUSP535_USB_CABLE_DETECT,
};

static struct platform_device asusp535_gpio_vbus = {
	.name = "gpio-vbus",
	.id = -1,
	.dev = {
		.platform_data = &gpio_vbus_data,
	}
};

/****************************************************************************
 * MATRIX KEYBOARD
 ***************************************************************************/
static unsigned int asusp535_mkbd[] = {
	KEY(0, 0, KEY_R),			/* MENU_RIGHT */
	KEY(0, 1, KEY_O),			/* MENU_LEFT */
	KEY(0, 2, KEY_T),			/* CALL */
	KEY(0, 3, KEY_O),			/* REFRESH */
	KEY(0, 4, KEY_4),
	KEY(0, 5, KEY_5),

	KEY(1, 0, KEY_6),			/* OK */
	KEY(1, 1, KEY_7),			/* HANGUP */
	KEY(1, 2, KEY_PAGEDOWN),	/* VOLUME- */
	KEY(1, 3, KEY_PAGEUP),		/* VOLUME+ */
	KEY(1, 4, KEY_A),			/* RECORD */
	KEY(1, 5, KEY_B),			/* FOCUS */

	KEY(2, 0, KEY_UP),
	KEY(2, 1, KEY_ENTER),
	KEY(2, 2, KEY_LEFT),
	KEY(2, 3, KEY_RIGHT),
	KEY(2, 4, KEY_DOWN),
	KEY(2, 5, KEY_H),			/* CAPTURE */
};

static struct pxa27x_keypad_platform_data asusp535_keypad_info = {
	.matrix_key_rows		= 3,
	.matrix_key_cols		= 6,
	.matrix_key_map			= asusp535_mkbd,
	.matrix_key_map_size	= ARRAY_SIZE(asusp535_mkbd),
	.debounce_interval		= 30,
};

/****************************************************************************
 * GPIO KEYS
 ***************************************************************************/
static struct gpio_keys_button asusp535_gpio_buttons[] = {
	{ KEY_P, GPIO_P535_POWER_KEY,		1, "Power Button" },
	{ KEY_H, GPIO_P535_HOLD_SWITCH,		1, "Hold switch" },
	{ KEY_B, GPIO_P535_BATTERY_DOOR,	1, "Battery door" },
	{
		.type				= EV_SW,
		.code				= SW_HEADPHONE_INSERT,
		.gpio				= 14,
		.desc				= "HeadPhone insert",
		.active_low			= 1,
		.debounce_interval	= 300,
	},
	{
		.type				= EV_SW,
		.code				= KEY_HP,
		.gpio				= 16,
		.desc				= "KEY_HP",
		.active_low			= 1,
		.debounce_interval	= 300,
	},
};

static struct gpio_keys_platform_data asusp535_buttons_data = {
	.buttons	= asusp535_gpio_buttons,
	.nbuttons	= ARRAY_SIZE(asusp535_gpio_buttons),
};

static struct platform_device asusp535_buttons = {
	.name	= "gpio-keys",
	.id		= -1,
	.dev	= {
		.platform_data = &asusp535_buttons_data,
	},
};

/****************************************************************************
 * FRAMEBUFFER
 ***************************************************************************/
static void asusp535_lcd_power(int on, struct fb_var_screeninfo *si)
{
	if (on) {
		gpio_set_value(GPIO_ASUSP535_LCD_POWER1, 1);
		gpio_set_value(GPIO_ASUSP535_LCD_POWER2, 1);
	} else {
		gpio_set_value(GPIO_ASUSP535_LCD_POWER1, 0);
		gpio_set_value(GPIO_ASUSP535_LCD_POWER2, 0);
	}
}

static struct pxafb_mode_info asusp535_ltm0305a776c = {
	.pixclock		= 156000,
	.xres			= 240,
	.yres			= 320,
	.bpp			= 16,
	.hsync_len		= 10,
	.left_margin	= 20,
	.right_margin	= 10,
	.vsync_len		= 2,
	.upper_margin	= 2,
	.lower_margin	= 2,
	.sync			= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
};

static struct pxafb_mach_info asusp535_pxafb_info = {
	.modes			= &asusp535_ltm0305a776c,
	.num_modes		= 1,
	.lcd_conn		= LCD_COLOR_TFT_16BPP | LCD_PCLK_EDGE_RISE,
	.lccr0			= LCCR0_Act | LCCR0_Sngl | LCCR0_Color,
	.lccr3			= LCCR3_OutEnH | LCCR3_PixRsEdg,
	.pxafb_lcd_power	= asusp535_lcd_power,
};

/****************************************************************************
 * TOUCHSCREEN
 ***************************************************************************/
static struct platform_device asusp535_touchscreen = {
	.name			= "wm97xx-touch",
};

/****************************************************************************
 * LCD BACKLIGHT
 ***************************************************************************/
static struct platform_pwm_backlight_data asusp535_backlight_data = {
	.pwm_id			= 2,
	.max_brightness	= 255,
	.dft_brightness	= 150,
	.pwm_period_ns	= 19692,
};

static struct platform_device asusp535_backlight = {
	.name	= "pwm-backlight",
	.dev	= {
		.parent = &pxa27x_device_pwm0.dev,
		.platform_data = &asusp535_backlight_data,
	},
};

/****************************************************************************
 * USB HOST
 ***************************************************************************/
static struct pxaohci_platform_data asusp535_ohci_info = {
	.port_mode		= PMM_NPS_MODE,
	.flags			= OC_MODE_PERPORT | ENABLE_PORT1,
	.power_on_delay	= 8,
};

static struct platform_device asusp535_pcm = {
	.name	= "pxa2xx-pcm",
	.id		= 0,
};

static struct platform_device asusp535_ac97 = {
	.name	= "pxa2xx-ac97",
	.id		= 0,
};

static struct platform_device asusp535_rfk = {
	.name	= "asusp535-rfk",
	.id		= -1,
};

static struct platform_device *asusp535_devices[] __initdata = {
	&asusp535_buttons,
	&asusp535_leds,
	&asusp535_backlight,
	&asusp535_touchscreen,
	&asusp535_pcm,
	&asusp535_ac97,
	&asusp535_rfk,
	&asusp535_gpio_vbus,
};

static void __init asusp535_init(void)
{
	pxa2xx_mfp_config(ARRAY_AND_SIZE(asusp535_pin_config));

	pxa_set_ffuart_info(NULL);
	pxa_set_btuart_info(NULL);
	pxa_set_stuart_info(NULL);

	pxa_set_i2c_info(NULL);

	i2c_register_board_info(0, ARRAY_AND_SIZE(asusp535_i2c_board_info));

	set_pxa_fb_info(&asusp535_pxafb_info);

	pxa_set_keypad_info(&asusp535_keypad_info);

	pxa_set_mci_info(&asusp535_mci_info);

	pxa_set_udc_info(&asusp535_udc_info);

	pxa_set_ohci_info(&asusp535_ohci_info);

	platform_add_devices(asusp535_devices, ARRAY_SIZE(asusp535_devices));
}

MACHINE_START(ASUSP535, "Asus P535")
	.boot_params	= 0xa0000100,
	.map_io			= pxa_map_io,
	.init_irq		= pxa27x_init_irq,
	.init_machine	= asusp535_init,
	.timer			= &pxa_timer,
MACHINE_END
