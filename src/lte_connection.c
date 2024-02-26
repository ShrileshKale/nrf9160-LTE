#include "lte_connection.h"
#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>

LOG_MODULE_REGISTER(Smart_farm_lte, LOG_LEVEL_DBG);

static K_EVENT_DEFINE(lte_connection_events);

/**
 * @brief Notify that LTE connection has been established.
 */
static void notify_lte_connected(void)
{
	k_event_post(&lte_connection_events, LTE_CONNECTED);
}

/**
 * @brief Reset the LTE connection event flag.
 */
static void clear_lte_connected(void)
{
	k_event_set(&lte_connection_events, 0);
}

extern bool await_lte_connection(k_timeout_t timeout)
{
	LOG_DBG("Awaiting LTE Connection");
	return k_event_wait_all(&lte_connection_events, LTE_CONNECTED, false, timeout) != 0;
}

/**
 * @brief Handler for LTE events coming from modem.
 *
 * @param evt Events from modem.
 */
extern void lte_handler(const struct lte_lc_evt *const evt)
{

	switch (evt->type)
	{
	case LTE_LC_EVT_NW_REG_STATUS:
		if ((evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_HOME) &&
			(evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_ROAMING))
		{
			clear_lte_connected();
		}
		else
		{
			notify_lte_connected();
		}

		LOG_INF("Network registration status: %s",
				evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ? "Connected - home network" : "Connected - roaming");
		break;

	case LTE_LC_EVT_RRC_UPDATE:
		LOG_INF("RRC mode: %s", evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED ? "Connected" : "Idle");
		break;

	default:
		break;
	}
}

/**
 * @brief Set up and configure LTE and start trying to connect.
 * @return int - 0 on success, otherwise a negative error code.
 */

extern int modem_configure(void)
{
	int err;

	LOG_INF("Initializing modem library");

	err = nrf_modem_lib_init();

	if (err < 0)
	{
		LOG_ERR("Modem library initialization failed, error: %d", err);
		return err;
	}
	else if (err == NRF_MODEM_DFU_RESULT_OK)
	{
		LOG_DBG("Modem library initialized after "
				"successful modem firmware update.");
	}
	else if (err > 0)
	{
		LOG_ERR("Modem library initialized after "
				"failed modem firmware update, error: %d",
				err);
	}
	else
	{
		LOG_DBG("Modem library initialized.");
	}

	err = lte_lc_modem_events_enable();

	if (err)
	{
		LOG_ERR("lte_lc_modem_events_enable failed, error: %d", err);
		return err;
	}

	LOG_INF("Connecting to LTE network");

	err = lte_lc_init_and_connect_async(lte_handler);
	if (err)
	{
		LOG_ERR("Modem could not be configured, error: %d", err);
		return err;
	}
	dk_set_led_on(DK_LED2);

	return 0;
}
