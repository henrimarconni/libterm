#ifndef LT__POSIX_INTERNAL_H
#define LT__POSIX_INTERNAL_H

int lt__posix_get_tty_fd(void);

/* Monotonic milliseconds (CLOCK_MONOTONIC), for input-side deadlines. */
long long lt__posix_now_ms(void);

#endif // LT__POSIX_INTERNAL_H
