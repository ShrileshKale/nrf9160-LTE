#ifndef LTE_CONNECTION_H
#define LTE_CONNECTION_H

#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>

/* LTE either is connected or isn't */
#define LTE_CONNECTED (1 << 1)

extern void lte_handler(const struct lte_lc_evt *const evt);

extern int modem_configure(void);

extern bool await_lte_connection(k_timeout_t timeout);

#endif /* _LTE_CONNECTION_H */
