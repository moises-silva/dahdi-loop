/*
 * Loopback DAHDI Driver for DAHDI Telephony interface
 * Emulates real dahdi devices allowing testing on hosts
 * which dont have real dahdi card - usefule for testing.
 * 
 * Copyright (C) 2007, Druid Software Ltd.
 * Copyright (C) 2008, Moises Silva <moises.silva@gmail.com>
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
 */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <dahdi/kernel.h>
#include <linux/hrtimer.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>

#include "dahdi_loop.h"

#ifndef VERSION_CODE
#  define VERSION_CODE(vers,rel,seq) ( ((vers)<<16) | ((rel)<<8) | (seq) )
#endif


#if LINUX_VERSION_CODE < VERSION_CODE(2,4,5)
#  error "This kernel is too old: not supported by this file"
#endif

#define DAHDI_RATE 1000                     /* DAHDI ticks per second */
#define DAHDI_TIME (1000000 / DAHDI_RATE)  /* DAHDI tick time in us */
#define DAHDI_TIME_NS (DAHDI_TIME * 1000)  /* DAHDI tick time in ns */

/* Different bits of the debug variable: */
#define DEBUG_GENERAL (1 << 0)
#define DEBUG_TICKS   (1 << 1)

static int num_loops=1;
static int num_taps=1;
static struct dahdi_loop *dahdi_loop;
static int debug = 0;
static unsigned int alarm_sim_type = DAHDI_ALARM_RED | DAHDI_ALARM_LOS;

static struct hrtimer looptimer;

static enum hrtimer_restart dahdi_loop_hr_int(struct hrtimer *htmr)
{
    unsigned long overrun;
    int i;

    for (i = 0; i < ((num_loops*2)+(num_taps*2)); i++) 
    {
	    dahdi_transmit(&dahdi_loop->spans[i]->span);
    }
    for (i = 0; i < ((num_loops*2)+(num_taps*2)); i++) 
    {
	    dahdi_receive(&dahdi_loop->spans[i]->span);
    }

    /* Overrun should always return 1, since we are in the timer that 
     * expired.
     * We should worry if overrun is 2 or more; then we really missed 
     * a tick */
     overrun = hrtimer_forward(&looptimer, hrtimer_get_expires(htmr), 
		ktime_set(0, DAHDI_TIME_NS));
    if(overrun > 1) {
	if(printk_ratelimit())
		printk(KERN_NOTICE "dahdi_loop: HRTimer missed %lu ticks\n", 
				overrun - 1);
}

if(debug && DEBUG_TICKS) {
	static int count = 0;
	/* Printk every 5 seconds, good test to see if timer is 
	 * running properly */
	if (count++ % 5000 == 0)
		printk(KERN_DEBUG "dahdi_loop: 5000 ticks from hrtimer\n");
}

/* Always restart the timer */
return HRTIMER_RESTART;
}

static int dahdi_loop_spanconfig(struct file *file, struct dahdi_span *span, struct dahdi_lineconfig *lc) 
{
    span->lineconfig = lc->lineconfig;
    return 0;
}

static int dahdi_loop_chanconfig(struct file *file, struct dahdi_chan *chan,int sigtype) 
{
    return 0;
}

static int dahdi_loop_startup(struct file *file, struct dahdi_span *span) 
{
    int alreadyrunning;

    alreadyrunning = span->flags & DAHDI_FLAG_RUNNING;

    if (!alreadyrunning) 
    {
        span->flags |= DAHDI_FLAG_RUNNING;
    }
    return 0;
}

static int dahdi_loop_shutdown(struct dahdi_span *span) {
    int alreadyrunning;

    alreadyrunning = span->flags & DAHDI_FLAG_RUNNING;

    if (!alreadyrunning) 
    {
        return 0;
    }
    return 0;
}

static int dahdi_loop_open(struct dahdi_chan *chan) {
    try_module_get(THIS_MODULE);
    return 0;
}

static int dahdi_loop_close(struct dahdi_chan *chan) {
    module_put(THIS_MODULE);
    return 0;
}

static int dahdi_loop_ioctl(struct dahdi_chan *chan, unsigned int cmd, unsigned long data) 
{
    switch(cmd) 
    {
      default:
        return -ENOTTY;
    }
    return 0;
}

static int dahdi_loop_rbs(struct dahdi_chan *chan, int bits)
{
    struct dahdi_loop *loop = chan->pvt;
    struct dahdi_chan *loopedchan = NULL;
    struct dahdi_chan *tappingchan = NULL;
    int i = chan->chanpos - 1;
    int s = chan->span->offset;
    int registered = chan->flags & DAHDI_FLAG_REGISTERED;
    if (!registered) {
        /* if the channel is not registered, we are just starting up with the idle bits
	       and the span to which we're connected may not be registered, so we should
	       not call dahdi_rbsbits or we will hang/crash the kernel. */
        printk(KERN_INFO "dahdi_loop: Ignoring transmission of bits 0x%X on chan %d (chan not registered)\n", bits, chan->channo);
        return 0;
    }
    printk(KERN_INFO "dahdi_loop: Transmitting bits 0x%X on chan %d\n", bits, i);
    loopedchan = &(loop->spans[((s%2)==0)?(s+1):s-1]->chans[i]);
    tappingchan = &(loop->spans[s+(num_loops*2)]->chans[i]);
    registered = loopedchan->flags & DAHDI_FLAG_REGISTERED;
    /* sanity check: verify if the looped and tapping devices are registered */
    if (registered) {
		printk(KERN_INFO "dahdi_loop: looping bits 0x%X on chan %d\n", bits, loopedchan->channo);
        dahdi_rbsbits(loopedchan, bits);
	}
    registered = tappingchan->flags & DAHDI_FLAG_REGISTERED;
    if (registered) {
		printk(KERN_INFO "dahdi_loop: tapping bits 0x%X on chan %d\n", bits, tappingchan->channo);
        dahdi_rbsbits(tappingchan, bits);
	}
    return 0;
}

static int dahdi_loop_taprbs(struct dahdi_chan *chan, int bits)
{
    /* nothing to do for cas tx since we're just tapping */
    return 0;
}

static int dahdi_loop_maint(struct dahdi_span *span, int cmd)
{
	switch (cmd) {
	case DAHDI_MAINT_ALARM_SIM:
		if (span->alarms) {
			span->alarms = 0;
		} else {
			span->alarms = alarm_sim_type;
		}
		dahdi_alarm_notify(span);
		break;
	}
	return 0;
}

static const struct dahdi_span_ops loop_span_ops = {
	.owner = THIS_MODULE,
	.spanconfig = dahdi_loop_spanconfig,
	.chanconfig = dahdi_loop_chanconfig,
	.startup = dahdi_loop_startup,
	.shutdown = dahdi_loop_shutdown,
	.open = dahdi_loop_open,
	.close = dahdi_loop_close,
	.ioctl = dahdi_loop_ioctl,
	.rbsbits = dahdi_loop_rbs,
	.maint = dahdi_loop_maint,
};

static const struct dahdi_span_ops tap_span_ops = {
	.owner = THIS_MODULE,
	.spanconfig = dahdi_loop_spanconfig,
	.chanconfig = dahdi_loop_chanconfig,
	.startup = dahdi_loop_startup,
	.shutdown = dahdi_loop_shutdown,
	.open = dahdi_loop_open,
	.close = dahdi_loop_close,
	.ioctl = dahdi_loop_ioctl,
	.rbsbits = dahdi_loop_taprbs,
};

static int dahdi_loop_initialize(struct dahdi_loop *loop) 
{
    int i=0,s=0, x=0;

    if ( (num_loops+num_taps) * 2 > DAHDI_LOOP_MAX_SPANS) {
        printk(KERN_INFO "dahdi_loop: dahdi_loop - too many loops/taps defined - total spans can not exceed %d!\n", DAHDI_LOOP_MAX_SPANS);
        return -1;
    }

    for (s=0; s < (num_loops*2); s++) {
		struct dahdi_loop_span *lspan = loop->spans[s];
		struct dahdi_span *span = &lspan->span;

        memset(span, 0, sizeof(*span));
		lspan->ddev = dahdi_create_device();
        sprintf(span->name, "dahdi_loop/%d",s+1);
        snprintf(span->desc, sizeof(span->desc) - 1, "Loop device span %d looped with span %d.", s+1, (((s%2)==0)?(s+1):s-1) + 1);
		lspan->ddev->manufacturer = "ZFormant Technologies";
		lspan->ddev->devicetype = "T1/E1 Loop";
		lspan->ddev->location = "n/a";
		dev_set_name(&lspan->ddev->dev, "dahdi_loop:%d", s+1);

        for (x=0; x < (sizeof(loop->spans[s]->chans)/sizeof(loop->spans[s]->chans[0])); x++) {
            lspan->chans_p[x] = &lspan->chans[x];
        }

		loop->spans[s]->span.ops = &loop_span_ops;
		loop->spans[s]->span.flags |= DAHDI_FLAG_RBS;

        loop->spans[s]->span.chans = loop->spans[s]->chans_p;
        loop->spans[s]->span.channels = 31;
        loop->spans[s]->span.deflaw = DAHDI_LAW_ALAW;
        loop->spans[s]->span.linecompat = DAHDI_CONFIG_AMI | DAHDI_CONFIG_HDB3 | DAHDI_CONFIG_CCS | DAHDI_CONFIG_CRC4;
        //init_waitqueue_head(&loop->spans[s]->span.maintq);
        //loop->spans[s]->span.pvt = loop;
        loop->spans[s]->span.offset = s;

        for (i=0; i < loop->spans[s]->span.channels; i++) {
            memset(&(loop->spans[s]->chans[i]),0x0,sizeof(struct dahdi_chan));
            sprintf(loop->spans[s]->chans[i].name,"dahdi_loop/%d/%d",s + 1,i + 1);
            loop->spans[s]->chans[i].pvt = loop;
            loop->spans[s]->chans[i].sigcap =  DAHDI_SIG_CLEAR | DAHDI_SIG_CAS;
            loop->spans[s]->chans[i].chanpos = i + 1;
            loop->spans[s]->chans[i].readchunk = loop->spans[((s%2)==0)?(s+1):s-1]->chans[i].swritechunk;
            loop->spans[s]->chans[i].writechunk = loop->spans[s]->chans[i].swritechunk;
        }

		list_add_tail(&span->device_node, &lspan->ddev->spans);
        if (dahdi_register_device(lspan->ddev, NULL)) {
            printk(KERN_INFO "dahdi_loop: unable to register dahdi span %d!\n",s+1);
            return -1;
        }
        printk(KERN_INFO "dahdi_loop: Registered loop device for span %d!\n",s+1);
    }

    for (s=(num_loops*2); s < ((num_loops*2)+(num_taps*2)); s++) 
    {
		struct dahdi_loop_span *lspan = loop->spans[s];
		struct dahdi_span *span = &lspan->span;
        memset(span, 0, sizeof(*span));
		lspan->ddev = dahdi_create_device();
        sprintf(span->name, "dahdi_tap/%d", s+1);
        sprintf(span->desc, "Tap device span %d - tap on tx of span %d.", s+1, s-(num_loops*2)+1);
		lspan->ddev->manufacturer = "ZFormant Technologies";
		lspan->ddev->devicetype = "T1/E1 Loop";
		lspan->ddev->location = "n/a";
		dev_set_name(&lspan->ddev->dev, "dahdi_tap:%d", s+1);

        for (x=0; x < (sizeof(loop->spans[s]->chans)/sizeof(loop->spans[s]->chans[0])); x++) {
            lspan->chans_p[x] = &lspan->chans[x];
        }
		span->ops = &tap_span_ops;
		loop->spans[s]->span.flags |= DAHDI_FLAG_RBS;

        loop->spans[s]->span.chans = loop->spans[s]->chans_p;
        loop->spans[s]->span.channels = 31;
        loop->spans[s]->span.deflaw = DAHDI_LAW_ALAW;
        loop->spans[s]->span.linecompat = DAHDI_CONFIG_AMI | DAHDI_CONFIG_HDB3 | DAHDI_CONFIG_CCS;
        //init_waitqueue_head(&loop->spans[s]->span.maintq);
        //loop->spans[s]->span.pvt = loop;
        loop->spans[s]->span.offset = s;

        for (i=0; i < loop->spans[s]->span.channels; i++) {
            memset(&(loop->spans[s]->chans[i]),0x0,sizeof(struct dahdi_chan));
            sprintf(loop->spans[s]->chans[i].name,"zttap/%d/%d",s + 1,i + 1);
            loop->spans[s]->chans[i].pvt = loop;
            loop->spans[s]->chans[i].sigcap =  DAHDI_SIG_CLEAR | DAHDI_SIG_CAS;
            loop->spans[s]->chans[i].chanpos = i + 1;
            /* Point the readchunk at the TX of what we are tapping */
            loop->spans[s]->chans[i].readchunk = loop->spans[s-(num_loops*2)]->chans[i].swritechunk;
            loop->spans[s]->chans[i].writechunk = loop->spans[s]->chans[i].swritechunk;
        }

        if (dahdi_register_device(lspan->ddev, NULL)) {
            printk(KERN_INFO "dahdi_loop: unable to register dahdi tap span %d!\n",s+1);
            return -1;
        }
        printk(KERN_INFO "dahdi_loop: Registered tap device for span %d!\n",s+1);
    }
    return 0;
}


int init_module(void)
{
    int i;
    dahdi_loop = kmalloc(sizeof(struct dahdi_loop), GFP_KERNEL);
    if (dahdi_loop == NULL) {
        printk("dahdi_loop: Unable to allocate memory\n");
        return -ENOMEM;
    }
    memset(dahdi_loop, 0x0, sizeof(struct dahdi_loop));

    for (i=0; i < ((num_loops*2) + (num_taps*2)); i++)
    {
        dahdi_loop->spans[i] = kmalloc(sizeof(struct dahdi_loop_span), GFP_KERNEL);
        if (dahdi_loop->spans[i] == NULL)
        {
            for (i--; i > 0; i--)
            {
                kfree(dahdi_loop->spans[i]);
            }
            kfree(dahdi_loop);
            return -ENOMEM;
        }
    }

    if (dahdi_loop_initialize(dahdi_loop)) {
        printk("dahdi_loop: Unable to intialize dahdi driver\n");
        kfree(dahdi_loop);
        return -ENODEV;
    }

    printk(KERN_DEBUG "dahdi_loop: Trying to load High Resolution Timer\n");
    hrtimer_init(&looptimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    printk(KERN_DEBUG "dahdi_loop: Initialized High Resolution Timer\n");

    /* Set timer callback function */
    looptimer.function = dahdi_loop_hr_int;

    printk(KERN_DEBUG "dahdi_loop: Starting High Resolution Timer\n");
    hrtimer_start(&looptimer, ktime_set(0, DAHDI_TIME_NS), HRTIMER_MODE_REL);
    printk(KERN_INFO "dahdi_loop: High Resolution Timer started, good to go\n");
    printk("dahdi_loop: Finished loading\n");
    return 0;
}


void cleanup_module(void)
{
    int s;
    hrtimer_cancel(&looptimer);
    for (s=0; s < ((num_loops*2)+(num_taps*2)); s++) {
        dahdi_unregister_device(dahdi_loop->spans[s]->ddev);
		dahdi_free_device(dahdi_loop->spans[s]->ddev);
    }
    kfree(dahdi_loop);
    printk("dahdi_loop: cleanup() finished\n");
}



module_param(debug, int, 0600);
module_param(num_loops, int, 0600);
module_param(num_taps, int, 0600);
module_param(alarm_sim_type, uint, 0600);
MODULE_PARM_DESC(alarm_sim_type, "Alarm flags to set when simulating an alarm (default: DAHDI_ALARM_RED|DAHDI_ALARM_LOS=0x9). Use 0x4 for DAHDI_ALARM_YELLOW.");

MODULE_DESCRIPTION("Loopback DAHDI Driver");
MODULE_AUTHOR("Druid Software Ltd (liamk)");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

