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
 * This file implements a linux kernel-module which causes the kernel
 * to panic
 **/

#include <linux/module.h>
#include <linux/init.h>

/**
 * Panics :o)
 **/
static int __init PANIC_init(void)
{
	panic("This kernelpanic is prodly presented by the PANIC-module");

	return 0;
}

/**
 * Does nothing
 **/
static void __exit PANIC_exit(void)
{
	// Never called, since the kernel panics at loading
	return;
}

module_init(PANIC_init);
module_exit(PANIC_exit);

MODULE_AUTHOR("Simon Schönfeld <simon.schoenfeld@web.de");
MODULE_DESCRIPTION("Causes the kernel to panic (for testing purposes)");
MODULE_LICENSE("GPL");

