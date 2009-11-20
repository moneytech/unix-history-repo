/*-
 * Copyright (c) 2006 Wojciech A. Koszek <wkoszek@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * $Id$
 */
/*
 * Skeleton of this file was based on respective code for ARM
 * code written by Olivier Houchard.
 */

/*
 * XXXMIPS: This file is hacked from arm/... . XXXMIPS here means this file is
 * experimental and was written for MIPS32 port.
 */
#include "opt_uart.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/pci/pcivar.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>
#include <dev/uart/uart_cpu.h>

/*
 * XXXMIPS:
 */
#include <mips/octeon1/octeonreg.h>

#include "uart_if.h"

extern struct uart_class uart_oct16550_class;


static int uart_octeon_probe(device_t dev);
static void octeon_uart_identify(driver_t * drv, device_t parent);

extern struct uart_class octeon_uart_class;

static device_method_t uart_octeon_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, uart_octeon_probe),
	DEVMETHOD(device_attach, uart_bus_attach),
	DEVMETHOD(device_detach, uart_bus_detach),
	DEVMETHOD(device_identify, octeon_uart_identify),
	{0, 0}
};

static driver_t uart_octeon_driver = {
	uart_driver_name,
	uart_octeon_methods,
	sizeof(struct uart_softc),
};

extern 
SLIST_HEAD(uart_devinfo_list, uart_devinfo) uart_sysdevs;

static int
uart_octeon_probe(device_t dev)
{
	struct uart_softc *sc;
	int unit;

/*
 * Note that both tty0 & tty1 are viable consoles. We add child devices
 * such that ttyu0 ends up front of queue.
 */
	unit = device_get_unit(dev);
	sc = device_get_softc(dev);
	sc->sc_sysdev = NULL;
	sc->sc_sysdev = SLIST_FIRST(&uart_sysdevs);
	bcopy(&sc->sc_sysdev->bas, &sc->sc_bas, sizeof(sc->sc_bas));
	if (!unit) {
		sc->sc_sysdev->bas.bst = 0;
		sc->sc_sysdev->bas.bsh = OCTEON_UART0ADR;
	}
	sc->sc_class = &uart_oct16550_class;
	sc->sc_bas.bst = 0;
	sc->sc_bas.bsh = unit ? OCTEON_UART1ADR : OCTEON_UART0ADR;
	sc->sc_bas.regshft = 0x3;
	return (uart_bus_probe(dev, sc->sc_bas.regshft, 0, 0, unit));
}

static void
octeon_uart_identify(driver_t * drv, device_t parent)
{
	BUS_ADD_CHILD(parent, 0, "uart", 0);
}



DRIVER_MODULE(uart, obio, uart_octeon_driver, uart_devclass, 0, 0);
