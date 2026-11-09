/* libultra's 64-bit integer helpers (ll.c), which IDO's code called for long long arithmetic on the N64; the
 * PC's compiler does its own, so these are kept for the callers that name them. */
#include "../rom_tu.h"

/* a >> b, unsigned. */
unsigned long long __ull_rshift(unsigned long long a0, unsigned long long a1) {
    return a0 >> a1;
}

/* a % b, unsigned. */
unsigned long long __ull_rem(unsigned long long a0, unsigned long long a1) {
    return a0 % a1;
}

/* a / b, unsigned. */
unsigned long long __ull_div(unsigned long long a0, unsigned long long a1) {
    return a0 / a1;
}

/* a << b. */
unsigned long long __ll_lshift(unsigned long long a0, unsigned long long a1) {
    return a0 << a1;
}

/* a % b (the sign of a). */
long long __ll_rem(unsigned long long a0, long long a1) {
    return a0 % a1;
}

/* a / b, signed. */
long long __ll_div(long long a0, long long a1) {
    return a0 / a1;
}

/* a * b. */
unsigned long long __ll_mul(unsigned long long a0, unsigned long long a1) {
    return a0 * a1;
}

/* *div = a / b, *rem = a % b, for a 16-bit divisor. */
void __ull_divremi(unsigned long long* div, unsigned long long* rem, unsigned long long a2, unsigned short a3) {
    *div = a2 / a3;
    *rem = a2 % a3;
}

/* a modulo b, the result with b's sign. */
long long __ll_mod(long long a0, long long a1) {
    long long tmp = a0 % a1;

    if ((tmp < 0 && a1 > 0) || (tmp > 0 && a1 < 0)) {
        tmp += a1;
    }
    return tmp;
}

/* a >> b, arithmetic. */
long long __ll_rshift(long long a0, long long a1) {
    return a0 >> a1;
}
