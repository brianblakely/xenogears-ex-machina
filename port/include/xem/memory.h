#ifndef XEM_MEMORY_H
#define XEM_MEMORY_H

/*
 * Game memory through PS1 addresses, for the port's reimplementations of the
 * handwritten routines (docs/runtime.md). They keep every game address as a
 * 32-bit integer, as the original registers did: a packet pointer the
 * original reduced to its 24-bit DMA address (main RAM mirrored at 0) stays
 * reduced where the game can see it, and only an access translates it.
 *
 * The game module holds RAM at its KSEG0 addresses and the scratchpad at
 * 0x1F800000; the native tests (tests/port_native.py) map the same two ranges
 * at the same host addresses, so the same C runs in both.
 */

/* The game-memory address of a PS1 address: KUSEG, KSEG0 and KSEG1 RAM and
 * its 8 MB of mirrors map to KSEG0 RAM, the scratchpad to 0x1F800000. */
static inline unsigned int xem_address(unsigned int address) {
    unsigned int physical = address & 0x1FFFFFFF;

    if (physical < 0x00800000) {
        return 0x80000000 | (physical & 0x001FFFFF);
    }
    return physical;
}

/* The object of type `type` at a PS1 address. */
#define XEM_AT(type, address) (*(type *)(__UINTPTR_TYPE__)xem_address(address))
#define XEM_U8(address) XEM_AT(unsigned char, address)
#define XEM_S8(address) XEM_AT(signed char, address)
#define XEM_U16(address) XEM_AT(unsigned short, address)
#define XEM_S16(address) XEM_AT(signed short, address)
#define XEM_U32(address) XEM_AT(unsigned int, address)

/* The PS1 address of a game object the C names (the port's game globals have
 * their original addresses, below 4 GB natively too). */
#define XEM_ADDRESS_OF(object) ((unsigned int)(__UINTPTR_TYPE__)&(object))

#endif
