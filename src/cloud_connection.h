#include <net/nrf_cloud.h>
#include <zephyr/net/mqtt.h>
#include <cJSON.h>
#include <zephyr/logging/log.h>

#define CLOUD_READY_TIMEOUT_SECONDS 60
#define CONFIG_CLOUD_CONNECTION_RETRY_TIMEOUT_SECONDS 30
#define CONFIG_CLOUD_CONNECTION_REESTABLISH_DELAY_SECONDS 30

/* CLOUD_CONNECTED is fired when we first connect to the nRF Cloud.
 * CLOUD_READY is fired when the connection is fully associated and ready to send device messages.
 * CLOUD_ASSOCIATION_REQUEST is a special state only used when first associating a device with
 *				an nRF Cloud user account.
 * CLOUD_DISCONNECTED is fired when disconnection is detected or requested, and will trigger
 *				a total reset of the nRF cloud connection, and the event flag state.
 */

#define CLOUD_READY			(1 << 2)
#define CLOUD_ASSOCIATION_REQUEST     (1 << 3)
#define CLOUD_CONNECTED                (1 << 1)
#define CLOUD_DISCONNECTED		(1 << 4)
/* Time either is or is not known. This is only fired once, and is never cleared. */
#define DATE_TIME_KNOWN			(1 << 1)


bool cloud_is_connected(void);

void disconnect_cloud(void);

bool await_cloud_disconnection(k_timeout_t timeout);

bool cloud_is_disconnecting(void);

int send_device_message_cJSON(cJSON *msg_obj);

void cloud_connection_thread(void);

bool await_connection(k_timeout_t timeout);

bool await_date_time_known(k_timeout_t timeout);







