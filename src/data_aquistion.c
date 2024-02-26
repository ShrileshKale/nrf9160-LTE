#include "data_aquistion.h"
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(Smart_farm_data, LOG_LEVEL_DBG);

/* Timer used to time the sensor sampling rate. */
static K_TIMER_DEFINE(sensor_sample_timer, NULL, NULL);

static const struct i2c_dt_spec dev_i2c = I2C_DT_SPEC_GET(I2C2_NODE);

/**
 * @brief Write config values to I2C config register.
 */
int write_to_temp_sensor()
{
    if (!device_is_ready(dev_i2c.bus))
    {
        LOG_INF("I2C bus %s is not ready!\n\r", dev_i2c.bus->name);
        return -1;
    }

    printk("I2C bus name is : %s\n", dev_i2c.bus->name);
    uint8_t config[2] = {STTS751_CONFIG_REG, 0x8C};
    int ret;
    ret = i2c_write_dt(&dev_i2c, config, sizeof(config));
    if (ret != 0)
    {
        printk("Inside write - Failed to write to I2C device address %x at Reg. %x \n", dev_i2c.addr, config[0]);
        return -1;
    }
}

/**
 * @brief Read from I2C config register.
 * on Success returns temperature value in Celcius
 */
float read_from_temp_register()
{
    uint8_t temp_reading[2] = {0};
    uint8_t sensor_regs[2] = {STTS751_TEMP_LOW_REG, STTS751_TEMP_HIGH_REG};
    int ret = i2c_write_read_dt(&dev_i2c, &sensor_regs[0], 1, &temp_reading[0], 1);
    if (ret != 0)
    {
        printk("Failed to write/read I2C device address %x at Reg. %x \r\n", dev_i2c.addr, sensor_regs[0]);
    }
    ret = i2c_write_read_dt(&dev_i2c, &sensor_regs[1], 1, &temp_reading[1], 1);
    if (ret != 0)
    {
        printk("Failed to write/read I2C device address %x at Reg. %x \r\n", dev_i2c.addr, sensor_regs[1]);
    }
    int temp = ((int)temp_reading[1] * 256 + ((int)temp_reading[0] & 0xF0)) / 16;
    if (temp > 2047)
    {
        temp -= 4096;
    }
    // Convert to celcius
    double cTemp = temp * 0.0625;

    return cTemp;
}
/**
 * @brief Create JSON object with time and date.
 * On success returns JSON object, else NULL
 */
static cJSON *create_timestamped_device_message(const char *const appid, const char *const msg_type)
{
    cJSON *msg_obj = NULL;
    int64_t timestamp;

    /* Acquire timestamp */
    if (date_time_now(&timestamp))
    {
        LOG_ERR("Failed to create timestamp for data message "
                "with appid %s",
                appid);
        return NULL;
    }

    /* Create container object */
    msg_obj = json_create_req_obj(appid, msg_type);
    if (msg_obj == NULL)
    {
        LOG_ERR("Failed to create container object for timestamped data message "
                "with appid %s and message type %s",
                appid, msg_type);
        return NULL;
    }

    /* Add timestamp to container object */
    if (!cJSON_AddNumberToObject(msg_obj, NRF_CLOUD_MSG_TIMESTAMP_KEY, (double)timestamp))
    {
        LOG_ERR("Failed to add timestamp to data message with appid %s and message type %s",
                appid, msg_type);
        cJSON_Delete(msg_obj);
        return NULL;
    }

    return msg_obj;
}

/**
 * @brief Sends sensor data to cloud
 */
static int send_sensor_data_to_cloud(const char *const sensor, double value)
{
    int ret = 0;
    char *msg = NULL;

    /* Create a timestamped message container object for the sensor sample. */
    cJSON *msg_obj = create_timestamped_device_message(
        sensor, NRF_CLOUD_JSON_MSG_TYPE_VAL_DATA);

    if (msg_obj == NULL)
    {
        ret = -EINVAL;
        goto cleanup;
    }

    if (cJSON_AddNumberToObject(msg_obj, sensor, value) == NULL)
    {
        ret = -ENOMEM;
        LOG_ERR("Failed to append value to %s sample container object ",
                sensor);
        goto cleanup;
    }

    if (!msg_obj)
    {
        LOG_ERR("Cannot send NULL device message object");
        return -EINVAL;
    }

    /* Convert message object to a string. */
    msg = cJSON_PrintUnformatted(msg_obj);
    if (msg == NULL)
    {
        LOG_ERR("Failed to convert cJSON device message object to string");
        return -ENOMEM;
    }

    struct nrf_cloud_tx_data mqtt_msg = {
        .data.ptr = msg,
        .data.len = strlen(msg),
        .qos = MQTT_QOS_1_AT_LEAST_ONCE,
        .topic_type = NRF_CLOUD_TOPIC_MESSAGE,
    };

    if (!nrf_cloud_send(&mqtt_msg))
    {
        LOG_INF("Device message sent successfully with error code %d", ret);
    }
    else
    {
        LOG_ERR("Transmission of device message failed, nrf_cloud_send "
                "gave error: %d. The message will be re-enqueued and tried again "
                "later.",
                ret);
    }

cleanup:
    if (msg_obj)
    {
        cJSON_Delete(msg_obj);
    }

    return ret;
}

void main_application_thread_fn(void)
{
    /* Wait for first connection before starting the application. */
    (void)await_connection(K_FOREVER);

    float temp;

    (void)nrf_cloud_alert_send(ALERT_TYPE_DEVICE_NOW_ONLINE, 0, NULL);

    LOG_INF("Waiting for modem to determine current date and time");
    if (!await_date_time_known(K_SECONDS(CONFIG_DATE_TIME_ESTABLISHMENT_TIMEOUT_SECONDS)))
    {
        LOG_WRN("Failed to determine valid date time. Proceeding anyways");
    }
    else
    {
        LOG_INF("Current date and time determined");
    }

    nrf_cloud_log_init();
    /* Send a direct log to the nRF Cloud web portal indicating the sample has started up. */
    nrf_cloud_log_send(LOG_LEVEL_INF, "Smart farm v%s",
                       "1.0");

    while (true)
    {
        k_timer_start(&sensor_sample_timer,
                      K_SECONDS(CONFIG_SENSOR_SAMPLE_INTERVAL_SECONDS), K_FOREVER);
        temp = read_from_temp_register();

        if (temp)
        {
            (void)send_sensor_data_to_cloud(NRF_CLOUD_JSON_APPID_VAL_TEMP, temp);
        }
        /* Wait out any remaining time on the sample interval timer. */
        k_timer_status_sync(&sensor_sample_timer);
    }
}
