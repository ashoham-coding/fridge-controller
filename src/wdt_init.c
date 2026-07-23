/*
 * wdt_init.c — Leonardo (ATmega32U4) watchdog / Caterina bootloader fix.
 *
 * Problem: after a WDT reset the Caterina bootloader sees WDRF in MCUSR and
 * jumps immediately to the application, bypassing the 8-second upload window.
 * The user must double-tap the reset button to force the bootloader to wait.
 *
 * Fix (two-part):
 *
 * 1. .init3 — runs before ANY C/C++ initialisation, before setup().
 *    Clears MCUSR and disables the WDT so the reset loop is broken and the
 *    app can start cleanly after a WDT-caused restart.
 *
 * 2. WDT ISR — fires one interrupt cycle before the WDT hardware reset.
 *    Writes Caterina's magic key (0x7777) at RAMEND so the bootloader enters
 *    programming mode instead of jumping to the app.
 *    Requires WDT to be armed in interrupt+reset mode — see wdt_enable_safe()
 *    below, which replaces the standard wdt_enable() call in setup().
 */

#include <avr/wdt.h>
#include <avr/io.h>
#include <avr/interrupt.h>

/* Magic value checked by the Caterina bootloader at startup.
   If present at RAMEND, it enters programming mode regardless of reset cause. */
#define MAGIC_KEY     0x7777
#define MAGIC_KEY_PTR ((volatile uint16_t *)RAMEND)

/* ---- Part 1: break reset loop ------------------------------------------ */
void wdt_init(void) __attribute__((naked, used, section(".init3")));
void wdt_init(void)
{
    MCUSR = 0;
    wdt_disable();
}

/* ---- Part 2: write magic key before WDT reset fires -------------------- */
ISR(WDT_vect)
{
    *MAGIC_KEY_PTR = MAGIC_KEY;
}

/* ---- Helper: arm WDT in interrupt+reset mode (8 s) --------------------- */
/* Call this instead of wdt_enable(WDTO_8S) in setup().                     */
/* WDP3|WDP0 = 8-second prescaler; WDIE enables the pre-reset interrupt;    */
/* WDE enables the subsequent hardware reset.                                */
void wdt_enable_safe(void)
{
    cli();
    MCUSR = 0;
    /* Timed sequence required by hardware to change WDTCSR */
    WDTCSR |= (1 << WDCE) | (1 << WDE);
    WDTCSR  = (1 << WDIE) | (1 << WDE) | (1 << WDP3) | (1 << WDP0);
    sei();
}
