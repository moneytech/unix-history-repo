/*	ct.c	6.1	83/07/29	*/

#include "ct.h"
#if NCT > 0
/*
 * GP DR11C driver used for C/A/T
 *
 * BUGS:
 *	This driver hasn't been tested in 4.1bsd
 */
#include "../machine/pte.h"

#include "../h/param.h"
#include "../h/systm.h"
#include "../h/tty.h"
#include "../h/map.h"
#include "../h/buf.h"
#include "../h/conf.h"
#include "../h/dir.h"
#include "../h/user.h"

#include "../vaxuba/ubareg.h"
#include "../vaxuba/ubavar.h"

#define	PCAT	(PZERO+9)
#define	CATHIWAT	100
#define	CATLOWAT	30

struct ct_softc {
	int	sc_openf;
	struct	clist sc_oq;
} ct_softc[NCT];

struct ctdevice {
	short	ctcsr;
	short	ctbuf;
};

int	ctprobe(), ctattach(), ctintr();
struct	uba_device *ctdinfo[NCT];
u_short	ctstd[] = { 0 };
struct	uba_driver ctdriver = 
    { ctprobe, 0, ctattach, 0, ctstd, "ct", ctdinfo };

#define	CTUNIT(dev)	(minor(dev))

ctprobe(reg)
	caddr_t reg;
{
	register int br, cvec;		/* value-result */
	register struct ctdevice *ctaddr = (struct ctdevice *)reg;

#ifdef lint
	br = 0; cvec = br; br = cvec;
	ctintr(0);
#endif
	ctaddr->ctcsr = IENABLE;
	DELAY(10000);
	ctaddr->ctcsr = 0;
	return (sizeof (struct ctdevice));
}

/*ARGSUSED*/
ctattach(ui)
	register struct uba_device *ui;
{

}

ctopen(dev)
	dev_t dev;
{
	register struct ct_softc *sc;
	register struct uba_device *ui;
	register struct ctdevice *ctaddr;

	if (CTUNIT(dev) >= NCT || (ui = ctdinfo[CTUNIT(dev)]) == 0 ||
	    ui->ui_alive == 0 || (sc = &ct_softc[CTUNIT(dev)])->sc_openf)
		return (ENXIO);
	sc->sc_openf = 1;
	ctaddr->ctcsr |= IENABLE;
	return (0);
}

ctclose(dev)
	dev_t dev;
{

	ct_softc[CTUNIT(dev)].sc_openf = 0;
	ctintr(dev);
}

ctwrite(dev, uio)
	dev_t dev;
	struct uio *uio;
{
	register struct ct_softc *sc = &ct_softc[CTUNIT(dev)];
	register int c;

	while ((c=cupass(uio)) >= 0) {
		(void) spl5();
		while (sc->sc_oq.c_cc > CATHIWAT)
			sleep((caddr_t)&sc->sc_oq, PCAT);
		while (putc(c, &sc->sc_oq) < 0)
			sleep((caddr_t)&lbolt, PCAT);
		ctintr(dev);
		(void) spl0();
	}
}

ctintr(dev)
	dev_t dev;
{
	register int c;
	register struct ct_softc *sc = &ct_softc[CTUNIT(dev)];
	register struct ctdevice *ctaddr =
	    (struct ctdevice *)ctdinfo[CTUNIT(dev)]->ui_addr;

	if (ctaddr->ctcsr&DONE) {
		if ((c = getc(&sc->sc_oq)) >= 0) {
			ctaddr->ctbuf = c;
			if (sc->sc_oq.c_cc==0 || sc->sc_oq.c_cc==CATLOWAT)
				wakeup(&sc->sc_oq);
		} else {
			if (sc->sc_openf==0)
				ctaddr->ctcsr = 0;
		}
	}

}
#endif
