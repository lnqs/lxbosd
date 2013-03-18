/**
 * Copyright (C) 2008 by Simon Schönfeld
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 **/

/**
 * This file implements a linux kernel-module which shows a windows-like
 * bluescreen, if a kernel-panic appears
 **/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/vt_kern.h>
#include <linux/console.h>

/* taken from vt_buffer.h */
#define scr_writew(val, addr) (*(addr) = (val))

/* saves some copy&paste */
#define update_pos(vc) (vc)->vc_pos = (vc)->vc_origin + (vc)->vc_y * (vc)->vc_size_row + ((vc)->vc_x<<1);

/* The colorcodes we use */
#define FG_COLOR_BLACK 0x00
#define FG_COLOR_BLUE 0x01
#define FG_COLOR_GREEN 0x02
#define FG_COLOR_CYAN 0x03
#define FG_COLOR_RED 0x04
#define FG_COLOR_PURPLE 0x05
#define FG_COLOR_YELLOW 0x06
#define FG_COLOR_WHITE 0x07

#define BG_COLOR_BLACK 0x00
#define BG_COLOR_BLUE 0x10
#define BG_COLOR_GREEN 0x20
#define BG_COLOR_CYAN 0x30
#define BG_COLOR_RED 0x40
#define BG_COLOR_PURPLE 0x50
#define BG_COLOR_YELOOW 0x60
#define BG_COLOR_WHITE 0x70

/* Relative to the number of rows/cols, lower value means higher padding */
#define PADDING_SIDES 4
#define PADDING_TOP 10

/* This is used to determine if a linebreak should be made, if a
   whitespace is following. This sucks a bit, since it may break too
   early or too late, but this is a simple and fast apporach, so we
   use it for now */
#define ASSUMED_WORDLENGTH 10

/* The build_attr() in vt.c is static, we have to copy it here */
static u8 build_attr(struct vc_data *vc, u8 _color, u8 _intensity,
		u8 _blink, u8 _underline, u8 _reverse)
{
#ifndef VT_BUF_VRAM_ONLY
/*
 * ++roman: I completely changed the attribute format for monochrome
 * mode (!can_do_color). The formerly used MDA (monochrome display
 * adapter) format didn't allow the combination of certain effects.
 * Now the attribute is just a bit vector:
 *  Bit 0..1: intensity (0..2)
 *  Bit 2   : underline
 *  Bit 3   : reverse
 *  Bit 7   : blink
 */
	{
	u8 a = vc->vc_color;
	if (!vc->vc_can_do_color)
		return _intensity |
		       (_underline ? 4 : 0) |
		       (_reverse ? 8 : 0) |
		       (_blink ? 0x80 : 0);
	if (_underline)
		a = (a & 0xf0) | vc->vc_ulcolor;
	else if (_intensity == 0)
		a = (a & 0xf0) | vc->vc_ulcolor;
	if (_reverse)
		a = ((a) & 0x88) | ((((a) >> 4) | ((a) << 4)) & 0x77);
	if (_blink)
		a ^= 0x80;
	if (_intensity == 2)
		a ^= 0x08;
	if (vc->vc_hi_font_mask == 0x100)
		a <<= 1;
	return a;
	}
#else
	return 0;
#endif
}

/* The update_attr() in vt.c is static, we have to copy it here */
static void update_attr(struct vc_data *vc)
{
	vc->vc_attr = build_attr(vc, vc->vc_color, vc->vc_intensity,
	              vc->vc_blink, vc->vc_underline,
	              vc->vc_reverse ^ vc->vc_decscnm);
	vc->vc_video_erase_char = (build_attr(vc, vc->vc_color, 1, vc->vc_blink, 0, vc->vc_decscnm) << 8) | ' ';
}

static void show(char* headline, char* message, char* footer)
{
	struct vc_data* vc;
	int i, j;
	int padding_sides, padding_top;

	/* Do we have a valid vc? */
	if(!vc_cons || !vc_cons[fg_console].d)
		return;

	vc = vc_cons[fg_console].d;
	padding_sides = vc->vc_rows / PADDING_SIDES;
	padding_top = vc->vc_cols / PADDING_TOP;

	acquire_console_sem();

	/* Clear the screen */
	vc->vc_color = (FG_COLOR_WHITE | BG_COLOR_BLUE);
	update_attr(vc);

	for(i = 0; i < vc->vc_cols; i++)
	{
		for(j = 0; j < vc->vc_rows; j++)
		{
			vc->vc_x = i;
			vc->vc_y = j;
			update_pos(vc);

			scr_writew((vc->vc_attr << 8) + ' ', (unsigned short*)vc->vc_pos);
		}
	}
	vc->vc_need_wrap = 0;

	/* Draw the headline */
	vc->vc_color = (FG_COLOR_BLUE | BG_COLOR_WHITE);
	update_attr(vc);
	vc->vc_x = (vc->vc_cols / 2) - (strlen(headline) / 2);
	vc->vc_y = padding_top;
	update_pos(vc);
	for(i = 0; i < strlen(headline); i++)
	{
		scr_writew((vc->vc_attr << 8) + headline[i],
				(unsigned short*)vc->vc_pos);
		vc->vc_x++;
		update_pos(vc);
	}

	/* Draw the text */
	vc->vc_color = (FG_COLOR_WHITE | BG_COLOR_BLUE);
	update_attr(vc);
	vc->vc_x = padding_sides;
	vc->vc_y += 2;
	update_pos(vc);
	for(i = 0; i < strlen(message); i++)
	{
		/* Do we need a linebreak? */
		/* For the ASSUMED_WORDLENGTH-stuff see the comment at #define */
		if(message[i] == '\n'
				|| (message[i] == ' ' && vc->vc_x + padding_sides
						+ ASSUMED_WORDLENGTH >= vc->vc_cols))
		{
			vc->vc_y++;
			vc->vc_x = padding_sides;
			update_pos(vc);

			continue;
		}

		if(vc->vc_x + padding_sides >= vc->vc_cols)
		{
			vc->vc_y++;
			vc->vc_x = padding_sides;
			update_pos(vc);
		}

		scr_writew((vc->vc_attr << 8) + message[i],
				(unsigned short*)vc->vc_pos);

		vc->vc_x++;
		update_pos(vc);
	}

	/* Draw the footer */
	vc->vc_x = (vc->vc_cols / 2) - (strlen(footer) / 2);
	vc->vc_y += 2;
	update_pos(vc);
	for(i = 0; i < strlen(footer); i++)
	{
		scr_writew((vc->vc_attr << 8) + footer[i],
				(unsigned short*)vc->vc_pos);
		vc->vc_x++;
		update_pos(vc);
	}

	/* redraw to show our changes */
	redraw_screen(vc, 0);

	release_console_sem();
}

/**
 * Will be called when a kernelpanic appears - calls all needed
 * functions to print our BSOD
 **/
static int lxbsod_handle_panic(struct notifier_block *nfb, unsigned long action, void *hcpu)
{
	char buf[2048];

	snprintf(buf, sizeof(buf),
			"A problem has been detected and Linux has been shut down "
			"to prevent damage to your computer.\n\n\n"
			"%s\n\n\n"
			"If this is the first time you've seen this error screen, "
			"restart your computer. If this screen appears again, follow "
			"these steps:\n\n"
			"Check to make sure any new kernels or kernelmodules are "
			"properly configured and installed.\n"
			"Write a bugreport as explained in REPORTING-BUGS in the "
			"main directoy of the kernel-tree, if this is a bug in the "
			"kernel, or to the developers of any patches or external "
			"modules, if you use any, and they caused the error.\n",
			(char*)hcpu);

	show(" LINUX ", buf, "Please restart your computer now.");

	return 0;
}

/* Needed to set the panic-callback */
static struct notifier_block lxbsod_panic_block = {
	.notifier_call = lxbsod_handle_panic
};

/**
 * Just prints short info about the loading and registers the callback
 **/
static int __init lxbsod_init(void)
{
	printk(KERN_INFO "lxBSOD loading\n");
	atomic_notifier_chain_register(&panic_notifier_list,	
			&lxbsod_panic_block);
	
	return 0;
}

/**
 * Just prints short info about the unloading and unregisters the callback
 **/
static void __exit lxbsod_exit(void)
{
	printk(KERN_INFO "lxBSOD unloading\n");
	atomic_notifier_chain_unregister(&panic_notifier_list,
			&lxbsod_panic_block);

	return;
}

module_init(lxbsod_init);
module_exit(lxbsod_exit);

MODULE_AUTHOR("Simon Schönfeld <simon.schoenfeld@web.de>");
MODULE_DESCRIPTION("Shows a windows-like bluescreen when a kernelpanic appears");
MODULE_LICENSE("GPL");

