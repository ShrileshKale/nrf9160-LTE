#include "cloud_connection.h"
#include "lte_connection.h"

LOG_MODULE_REGISTER(connection, LOG_LEVEL_DBG);

/* Flow control event objects for waiting for key events. */
static K_EVENT_DEFINE(cloud_connection_events);

/**
 * @brief Notify that connection to nRF Cloud has been established.
 */
static void notify_cloud_connected(void)
{
	k_event_post(&cloud_connection_events, CLOUD_READY);
}

/**
 * @brief Notify that a cloud association request has been made.
 */
static void notify_cloud_requested_association(void)
{
	k_event_post(&cloud_connection_events, CLOUD_ASSOCIATION_REQUEST);
}

/**
 * @brief Await a connection to nRF Cloud (ignoring LTE connection state and cloud readiness).
 *
 * @param timeout - The time to wait before timing out.
 * @return true if occurred.
 * @return false if timed out.
 */
static bool await_cloud_connected(k_timeout_t timeout)
{
	LOG_DBG("Awaiting Cloud Connection");
	return k_event_wait(&cloud_connection_events, CLOUD_CONNECTED, false, timeout) != 0;
}

/**
 * @brief Clear nRF Cloud connection events.
 */
static void clear_cloud_connection_events(void)
{
	k_event_set(&cloud_connection_events, 0);
}

/**
 * @brief Notify that a cloud association request has been made.
 */
static bool cloud_has_requested_association(void)
{
	return k_event_wait(&cloud_connection_events, CLOUD_ASSOCIATION_REQUEST,
						false, K_NO_WAIT) != 0;
}

/**
 * @brief Wait for nRF Cloud readiness.
 *
 * @param timeout - The time to wait before timing out.
 * @param timeout_on_disconnection - Should cloud disconnection events count as a timeout?
 * @return true if occurred.
 * @return false if timed out.
 */
static bool await_cloud_ready(k_timeout_t timeout, bool timeout_on_disconnection)
{
	LOG_INF("Awaiting Cloud Ready");
	int await_condition = CLOUD_READY;

	if (timeout_on_disconnection)
	{
		await_condition |= CLOUD_DISCONNECTED;
	}

	return k_event_wait(&cloud_connection_events, await_condition,
						false, timeout) == CLOUD_READY;
}

/**
 * @brief Notify that cloud connection is ready.
 */
static void notify_cloud_ready(void)
{
	k_event_post(&cloud_connection_events, CLOUD_READY);
}

bool cloud_is_connected(void)
{
	return k_event_wait(&cloud_connection_events, CLOUD_CONNECTED, false, K_NO_WAIT) != 0;
}

bool await_date_time_known(k_timeout_t timeout)
{
	return k_event_wait(&cloud_connection_events, DATE_TIME_KNOWN, false, timeout) != 0;
}

void disconnect_cloud(void)
{
	k_event_post(&cloud_connection_events, CLOUD_DISCONNECTED);
}

bool await_cloud_disconnection(k_timeout_t timeout)
{
	return k_event_wait(&cloud_connection_events, CLOUD_DISCONNECTED, false, timeout) != 0;
}

bool cloud_is_disconnecting(void)
{
	return k_event_wait(&cloud_connection_events, CLOUD_DISCONNECTED, false, K_NO_WAIT) != 0;
}

bool await_connection(k_timeout_t timeout)
{
	return await_cloud_ready(timeout, false);
}

static void reset_cloud(void)
{
	int err;

	/* Wait for a few seconds to help residual events settle. */
	LOG_INF("Disconnecting from nRF Cloud");
	k_sleep(K_SECONDS(20));

	/* Disconnect from nRF Cloud */
	err = nrf_cloud_disconnect();

	/* nrf_cloud_uninit returns -EACCES if we are not currently in a connected state. */
	if (err == -EACCES)
	{
		LOG_INF("Cannot disconnect from nRF Cloud because we are not currently connected");
	}
	else if (err)
	{
		LOG_ERR("Cannot disconnect from nRF Cloud, error: %d. Continuing anyways", err);
	}
	else
	{
		LOG_INF("Successfully disconnected from nRF Cloud");
	}

	/* Clear cloud connection event state (reset to initial state). */
	clear_cloud_connection_events();
}

/**
 * @brief Handler for events from nRF Cloud Lib.
 *
 * @param nrf_cloud_evt Passed in event.
 */
static void nrf_cloud_event_handler(const struct nrf_cloud_evt *evt)
{

	switch (evt->type)
	{
	case NRF_CLOUD_EVT_TRANSPORT_CONNECTED:
		LOG_INF("***NRF_CLOUD_EVT_TRANSPORT_CONNECTED***");
		notify_cloud_connected();
		break;

	case NRF_CLOUD_EVT_TRANSPORT_CONNECTING:
		LOG_INF("***NRF_CLOUD_EVT_TRANSPORT_CONNECTING***");
		break;

	case NRF_CLOUD_EVT_USER_ASSOCIATION_REQUEST:
		LOG_INF("***NRF_CLOUD_EVT_USER_ASSOCIATION_REQUEST***");
		LOG_INF("Add the device to nRF Cloud to complete user association");
		notify_cloud_requested_association();
		break;

	case NRF_CLOUD_EVT_USER_ASSOCIATED:
		LOG_INF("***NRF_CLOUD_EVT_USER_ASSOCIATED***");

		if (cloud_has_requested_association())
		{
			LOG_INF("Device successfully associated with cloud!");
			disconnect_cloud();
		}

		break;

	case NRF_CLOUD_EVT_READY:
		LOG_INF("***NRF_CLOUD_EVT_READY***");
		notify_cloud_ready();
		break;

	case NRF_CLOUD_EVT_TRANSPORT_DISCONNECTED:
		LOG_INF("NRF_CLOUD_EVT_TRANSPORT_DISCONNECTED");
		disconnect_cloud();
		break;

	case NRF_CLOUD_EVT_TRANSPORT_CONNECT_ERROR:
		break;

	case NRF_CLOUD_EVT_ERROR:
		LOG_ERR("NRF_CLOUD_EVT_ERROR: %d", evt->status);
		break;

	case NRF_CLOUD_EVT_SENSOR_DATA_ACK:
		LOG_INF("Sensor data received on NRF Cloud");
		break;

	case NRF_CLOUD_EVT_RX_DATA_SHADOW:
		LOG_DBG("NRF_CLOUD_EVT_RX_DATA_SHADOW");
		break;

	default:
		LOG_ERR("Unknown nRF Cloud event type: %d", evt->type);
		break;
	}
}

/**
 * @brief Establish a connection to nRF Cloud (presuming we are connected to LTE).
 *
 * @return int - 0 on success, otherwise negative error code.
 */
int connect_cloud(void)
{
	int err;

	LOG_INF("Connecting to nRF Cloud...");

	err = nrf_cloud_connect();
	if (err)
	{
		LOG_ERR("cloud_connect, error: %d", err);
	}
	/* Wait for cloud connection success. If succeessful, break out of the loop. */
	if (await_cloud_connected(K_SECONDS(CONFIG_CLOUD_CONNECTION_RETRY_TIMEOUT_SECONDS)))
		{
			LOG_INF("Cloud is ready");
		}
	/* Wait for cloud to become ready, resetting if we time out or are disconnected. */
	if (!await_cloud_ready(K_SECONDS(CLOUD_READY_TIMEOUT_SECONDS), true))
	{
		LOG_INF("nRF Cloud failed to become ready. Resetting connection.");
		reset_cloud();
		return -ETIMEDOUT;
	}

	LOG_INF("Connected to nRF Cloud");

	return err;
}

/**
 * @brief Set up the nRF Cloud library
 * Must be called before setup_lte, so that pending FOTA jobs can be checked before LTE init.
 * @return int - 0 on success, otherwise negative error code.
 */
int setup_cloud(void)
{
	/* Initialize nrf_cloud library. */
	struct nrf_cloud_init_param params = {
		.event_handler = nrf_cloud_event_handler,
		.application_version = "V1.0"};

	int err = nrf_cloud_init(&params);

	if (err)
	{
		LOG_ERR("nRF Cloud library could not be initialized, error: %d", err);
		return err;
	}

	return 0;
}

void cloud_connection_thread(void)
{
	int err;

	LOG_INF("Setting up modem...");
	err = modem_configure();
	if (err)
	{
		LOG_ERR("Failed to configure the modem");
		return;
	}

	LOG_INF("Setting up nRF Cloud library...");
	if (setup_cloud())
	{
		LOG_ERR("Fatal: nRF Cloud library setup failed");
		return;
	}
	LOG_INF("nRF Cloud library setup Sucessful");

	while (true)
	{

		LOG_INF("Waiting for connection to LTE network...");

		(void)await_lte_connection(K_FOREVER);
		LOG_INF("Connected to LTE network");

		if (!connect_cloud())
		{
			(void)await_cloud_disconnection(K_FOREVER);
			LOG_INF("Disconnected from nRF Cloud");
		}
		else
		{

			LOG_INF("Failed to connect to nRF Cloud");
		}
		/* Reset cloud connection state before trying again. */
		reset_cloud();

		/* Wait a bit before trying again. */
		k_sleep(K_SECONDS(CONFIG_CLOUD_CONNECTION_REESTABLISH_DELAY_SECONDS));
	}
}
