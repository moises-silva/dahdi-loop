/*
 * Loopback DAHDI Driver for DAHDI Telephony interface
 *
 * DAHDI driver simulating looped back spans as well as
 * taps on these spans.
 *
 * Copyright (C) 2007, Druid Software Ltd.
 *
 * Based on dahdi_dummy
 *
 * All rights reserved.
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
*/

#include <linux/version.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,19)
#define USB2420
#endif

#define DAHDI_LOOP_MAX_SPANS 16
#define DAHDI_LOOP_MAX_CHANS 31

struct dahdi_loop_span {
	struct dahdi_device *ddev;
	struct dahdi_span span;
	struct dahdi_chan chans[DAHDI_LOOP_MAX_CHANS];
	struct dahdi_chan *chans_p[DAHDI_LOOP_MAX_CHANS];
};

struct dahdi_loop {
    struct dahdi_loop_span *spans[DAHDI_LOOP_MAX_SPANS];
};


