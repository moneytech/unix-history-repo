/*	kern_clock.c	3.2	%H%	*/

#include "../h/param.h"
#include "../h/systm.h"
#include "../h/callo.h"
#include "../h/seg.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/proc.h"
#include "../h/reg.h"
#include "../h/psl.h"
#include "../h/vm.h"
#include "../h/buf.h"
#include "../h/text.h"

#define	SCHMAG	9/10


/*
 * clock is called straight from
 * the real time clock interrupt.
 *
 * Functions:
 *	implement callouts
 *	maintain user/system times
 *	maintain date
 *	profile
 *	lightning bolt wakeup (every second)
 *	alarm clock signals
 *	jab the scheduler
 */
#ifdef KPROF
short	kcount[20000];
#endif

clock(pc, ps)
caddr_t pc;
{
	register struct callo *p1, *p2;
	register struct proc *pp;
	register int s;
	int a;

	/*
	 * reprime clock
	 */
	clkreld();

	/*
	 * callouts
	 * else update first non-zero time
	 */

	if(callout[0].c_func == NULL)
		goto out;
	p2 = &callout[0];
	while(p2->c_time<=0 && p2->c_func!=NULL)
		p2++;
	p2->c_time--;

	/*
	 * if ps is high, just return
	 */
	if (BASEPRI(ps))
		goto out;

	/*
	 * callout
	 */

	if(callout[0].c_time <= 0) {
		p1 = &callout[0];
		while(p1->c_func != 0 && p1->c_time <= 0) {
			(*p1->c_func)(p1->c_arg);
			p1++;
		}
		p2 = &callout[0];
		while(p2->c_func = p1->c_func) {
			p2->c_time = p1->c_time;
			p2->c_arg = p1->c_arg;
			p1++;
			p2++;
		}
	}

	/*
	 * lightning bolt time-out
	 * and time of day
	 */
out:
	if (!noproc) {
		s = u.u_procp->p_rssize;
		u.u_vm.vm_idsrss += s;
		if (u.u_procp->p_textp) {
			register int xrss = u.u_procp->p_textp->x_rssize;

			s += xrss;
			u.u_vm.vm_ixrss += xrss;
		}
		if (s > u.u_vm.vm_maxrss)
			u.u_vm.vm_maxrss = s;
	}
	a = dk_busy&07;
	if (USERMODE(ps)) {
		u.u_vm.vm_utime++;
		if(u.u_procp->p_nice > NZERO)
			a += 8;
	} else {
		a += 16;
		if (noproc)
			a += 8;
		else
			u.u_vm.vm_stime++;
	}
	dk_time[a]++;
	if (!noproc) {
		pp = u.u_procp;
		if(++pp->p_cpu == 0)
			pp->p_cpu--;
		if(pp->p_cpu % 16 == 0) {
			VOID setpri(pp);
			if (pp->p_pri >= PUSER)
				pp->p_pri = pp->p_usrpri;
		}
	}
	++lbolt;
	if (lbolt % (HZ/4) == 0) {
		vmpago();
		runrun++;
	}
	if (lbolt >= HZ) {
		if (BASEPRI(ps))
			return;
		lbolt -= HZ;
		++time;
		VOID spl1();
		runrun++;
		wakeup((caddr_t)&lbolt);
		for(pp = &proc[0]; pp < &proc[NPROC]; pp++)
		if (pp->p_stat && pp->p_stat<SZOMB) {
			if(pp->p_time != 127)
				pp->p_time++;
			if(pp->p_clktim)
				if(--pp->p_clktim == 0)
					if (pp->p_flag & STIMO) {
						s = spl6();
						if (pp->p_stat == SSLEEP)
							setrun(pp);
						pp->p_flag &= ~STIMO;
						splx(s);
					} else
						psignal(pp, SIGCLK);
			if(pp->p_stat==SSLEEP||pp->p_stat==SSTOP)
				if (pp->p_slptime != 127)
					pp->p_slptime++;
			if(pp->p_flag&SLOAD) {
				ave(pp->p_aveflt, pp->p_faults, 5);
				pp->p_faults = 0;
			}
			a = (pp->p_cpu & 0377)*SCHMAG + pp->p_nice - NZERO;
			if(a < 0)
				a = 0;
			if(a > 255)
				a = 255;
			pp->p_cpu = a;
			VOID setpri(pp);
			s = spl6();
			if(pp->p_pri >= PUSER) {
				if ((pp != u.u_procp || noproc) &&
				    pp->p_stat == SRUN &&
				    (pp->p_flag & SLOAD) &&
				    pp->p_pri != pp->p_usrpri) {
					remrq(pp);
					pp->p_pri = pp->p_usrpri;
					setrq(pp);
				} else
					pp->p_pri = pp->p_usrpri;
			}
			splx(s);
		}
		vmmeter();
		if(runin!=0) {
			runin = 0;
			wakeup((caddr_t)&runin);
		}
		/*
		 * If there are pages that have been cleaned, 
		 * jolt the pageout daemon to process them.
		 * We do this here so that these pages will be
		 * freed if there is an abundance of memory and the
		 * daemon would not be awakened otherwise.
		 */
		if (bclnlist != NULL)
			wakeup((caddr_t)&proc[2]);
#ifdef ERNIE
		if (USERMODE(ps)) {
			pp = u.u_procp;
			if (pp->p_uid)
				if (pp->p_nice == NZERO && u.u_vm.vm_utime > 600 * HZ)
					pp->p_nice = NZERO+4;
			VOID setpri(pp);
			pp->p_pri = pp->p_usrpri;
		}
#endif
	}
	if (USERMODE(ps)) {
		/*
		 * We do this last since it
		 * may block on a page fault in user space.
		 */
		if (u.u_prof.pr_scale)
			addupc(pc, &u.u_prof, 1);
	}
#ifdef KPROF
	else if (!noproc) {
		register int indx = ((int)pc & 0x7fffffff) / 8;

		if (indx >= 0 && indx < 20000)
			kcount[indx]++;
	}
#endif
}

/*
 * timeout is called to arrange that
 * fun(arg) is called in tim/HZ seconds.
 * An entry is sorted into the callout
 * structure. The time in each structure
 * entry is the number of HZ's more
 * than the previous entry.
 * In this way, decrementing the
 * first entry has the effect of
 * updating all entries.
 *
 * The panic is there because there is nothing
 * intelligent to be done if an entry won't fit.
 */
timeout(fun, arg, tim)
int (*fun)();
caddr_t arg;
{
	register struct callo *p1, *p2;
	register int t;
	int s;

	t = tim;
	p1 = &callout[0];
	s = spl7();
	while(p1->c_func != 0 && p1->c_time <= t) {
		t -= p1->c_time;
		p1++;
	}
	if (p1 >= &callout[NCALL-1])
		panic("Timeout table overflow");
	p1->c_time -= t;
	p2 = p1;
	while(p2->c_func != 0)
		p2++;
	while(p2 >= p1) {
		(p2+1)->c_time = p2->c_time;
		(p2+1)->c_func = p2->c_func;
		(p2+1)->c_arg = p2->c_arg;
		p2--;
	}
	p1->c_time = t;
	p1->c_func = fun;
	p1->c_arg = arg;
	splx(s);
}
