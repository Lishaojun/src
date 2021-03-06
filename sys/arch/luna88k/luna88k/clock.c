/* $OpenBSD: clock.c,v 1.11 2016/09/04 00:56:08 aoyama Exp $ */
/* $NetBSD: clock.c,v 1.2 2000/01/11 10:29:35 nisimura Exp $ */

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah Hdr: clock.c 1.18 91/01/21
 *
 *	@(#)clock.c	8.1 (Berkeley) 6/10/93
 */

/* from NetBSD/luna68k sys/arch/luna68k/luna68k/clock.c */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/evcount.h>
#include <sys/timetc.h>

#include <machine/board.h>
#include <machine/cpu.h>

#include <dev/clock_subr.h>
#include <luna88k/luna88k/clockvar.h>

struct device *clockdev;
const struct clockfns *clockfns;
struct evcount *clockevc;
int clockinitted, todrvalid;

void
clockattach(struct device *dev, const struct clockfns *fns,
	struct evcount *evc)
{
	/*
	 * Just bookkeeping.
	 */
	if (clockfns != NULL)
		panic("clockattach: multiple clocks");
	clockdev = dev;
	clockfns = fns;
	clockevc = evc;
}

/*
 * Machine-dependent clock routines.
 *
 * Startrtclock restarts the real-time clock, which provides
 * hardclock interrupts to kern_clock.c.
 *
 * Inittodr initializes the time of day hardware which provides
 * date functions.  Its primary function is to use some file
 * system information in case the hardare clock lost state.
 *
 * Resettodr restores the time of day hardware after a time change.
 */

u_int	clock_get_tc(struct timecounter *);

struct timecounter clock_tc = {
	.tc_get_timecount = clock_get_tc,
	.tc_counter_mask = 0xffffffff,
	.tc_frequency = 0, /* will be filled in */
	.tc_name = "clock",
	.tc_quality = 0
};

/*
 * Start the real-time and statistics clocks. Leave stathz 0 since there
 * are no other timers available.
 */
void
cpu_initclocks()
{

#ifdef DIAGNOSTIC
	if (clockfns == NULL)
		panic("cpu_initclocks: no clock attached");
#endif

	tick = 1000000 / hz;	/* number of microseconds between interrupts */
	clock_tc.tc_frequency = hz;
	tc_init(&clock_tc);
	clockinitted = 1;
}

/*
 * We assume newhz is either stathz or profhz, and that neither will
 * change after being set up above.  Could recalculate intervals here
 * but that would be a drag.
 */
void
setstatclockrate(int newhz)
{
	/* nothing we can do */
}

/*
 * Initialze the time of day register, based on the time base which is, e.g.
 * from a filesystem.  Base provides the time to within six months,
 * and the time of year clock (if any) provides the rest.
 */
void
inittodr(time_t base)
{
	struct clock_ymdhms dt;
	struct timespec ts;
	time_t deltat;
	int badbase;

	ts.tv_sec = ts.tv_nsec = 0;

	if (base < (2012 - 1970) * SECYR) {
		printf("WARNING: preposterous time in file system");
		/* read the system clock anyway */
		base = (2012 - 1970) * SECYR;
		badbase = 1;
	} else
		badbase = 0;

	(*clockfns->cf_get)(clockdev, base, &dt);
	todrvalid = 1;
	/* simple sanity checks */
	if (dt.dt_year < 1970 || dt.dt_mon < 1 || dt.dt_mon > 12
		|| dt.dt_day < 1 || dt.dt_day > 31
		|| dt.dt_hour > 23 || dt.dt_min > 59 || dt.dt_sec > 59) {
		/*
		 * Believe the time in the file system for lack of
		 * anything better, resetting the TODR.
		 */
		ts.tv_sec = base;
		tc_setclock(&ts);
		if (!badbase) {
			printf("WARNING: preposterous clock chip time");
			resettodr();
		}
		goto bad;
	}
	/* now have days since Jan 1, 1970; the rest is easy... */
	ts.tv_sec = clock_ymdhms_to_secs(&dt);
	tc_setclock(&ts);

	if (!badbase) {
		/*
		 * See if we gained/lost two or more days;
		 * if so, assume something is amiss.
		 */
		deltat = ts.tv_sec - base;
		if (deltat < 0)
			deltat = -deltat;
		if (deltat < 2 * SECDAY)
			return;
		printf("WARNING: clock %s %d days",
		    ts.tv_sec < base ? "lost" : "gained",
		       (int) (deltat / SECDAY));
	}
bad:
	printf(" -- CHECK AND RESET THE DATE!\n");
}

/*
 * Reset the TODR based on the time value; used when the TODR
 * has a preposterous value and also when the time is reset
 * by the stime system call.  Also called when the TODR goes past
 * TODRZERO + 100*(SECYEAR+2*SECDAY) (e.g. on Jan 2 just after midnight)
 * to wrap the TODR around.
 */
void
resettodr()
{
	struct clock_ymdhms dt;

	if (!todrvalid)
		return;
	clock_secs_to_ymdhms(time_second, &dt);
	(*clockfns->cf_set)(clockdev, &dt);
}

/*
 * *clock_reg[CPU]
 * Points to the clock register for each CPU.
 */
volatile u_int32_t *clock_reg[] = {
	(u_int32_t *)OBIO_CLOCK0,
	(u_int32_t *)OBIO_CLOCK1,
	(u_int32_t *)OBIO_CLOCK2,
	(u_int32_t *)OBIO_CLOCK3
};

/*
 * Clock interrupt routine
 */
int
clockintr(void *eframe)
{
#ifdef MULTIPROCESSOR
	struct cpu_info *ci = curcpu();
	u_int cpu = ci->ci_cpuid;
#else
	u_int cpu = cpu_number();
#endif

#ifdef MULTIPROCESSOR
	if (CPU_IS_PRIMARY(ci))
#endif
		clockevc->ec_count++;

	*clock_reg[cpu] = 0xffffffff;
	if (clockinitted)
		hardclock(eframe);
	return 1;
}

u_int
clock_get_tc(struct timecounter *tc)
{
	return (u_int)clockevc->ec_count;
}
