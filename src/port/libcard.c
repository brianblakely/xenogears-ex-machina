/*
 * libcard: starting the BIOS memory card driver and checking a card (PsyQ
 * InitCARD, StartCARD, StopCARD, _bu_init, _card_info). The driver itself is
 * the host's card service; a check ends with the BIOS card events the game
 * opens (class 0xF4000001, also delivered to the software class 0xF0000011).
 * InitCARD's _patch_card and _patch_card2 patch the BIOS's own card code and
 * have nothing to act on here.
 */
#include "common.h"
#include "psyq/libapi.h"
#include "xem/card.h"
#include "xem/port.h"

void ChangeClearPAD(long val);
void DeliverEvent(unsigned long class, unsigned long spec);

long InitCARD(long val) {
    ChangeClearPAD(0);
    EnterCriticalSection();
    xem_host_card_init(val);
    ExitCriticalSection();
    return 0;
}

long StartCARD(void) {
    EnterCriticalSection();
    xem_host_card_start();
    ChangeClearPAD(0);
    ExitCriticalSection();
    return 1;
}

long StopCARD(void) {
    xem_host_card_stop();
    return 1;
}

void _bu_init(void) {
    xem_host_card_bu_init();
}

/* Start a check of `channel`: 1 when started (its event follows), 0 when the
 * driver cannot take it. */
long _card_info(long channel) {
    return xem_host_card_info(channel);
}

void xem_card_interrupt(unsigned int spec) {
    DeliverEvent(XEM_CARD_SW_CLASS, spec);
    DeliverEvent(XEM_CARD_HW_CLASS, spec);
}
