/* libultra's conversions between 64-bit integers and floating point (llcvt.c). */
#include "../rom_tu.h"

/* A double to a long long. */
long long __d_to_ll(double d) {
    return d;
}

/* A float to a long long. */
long long __f_to_ll(float f) {
    return f;
}

/* A double to an unsigned long long. */
unsigned long long __d_to_ull(double d) {
    return d;
}

/* A float to an unsigned long long. */
unsigned long long __f_to_ull(float f) {
    return f;
}

/* A long long to a double. */
double __ll_to_d(long long s) {
    return s;
}

/* A long long to a float. */
float __ll_to_f(long long s) {
    return s;
}

/* An unsigned long long to a double. */
double __ull_to_d(unsigned long long u) {
    return u;
}

/* An unsigned long long to a float. */
float __ull_to_f(unsigned long long u) {
    return u;
}

