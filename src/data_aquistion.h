
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <zephyr/drivers/i2c.h>
#include <cJSON.h>
#include <net/nrf_cloud_codec.h>
#include <net/nrf_cloud_log.h>
#include <net/nrf_cloud_alert.h>
#include <date_time.h>

#include "cloud_connection.h"

#define CONFIG_SENSOR_SAMPLE_INTERVAL_SECONDS 30
#define CONFIG_DATE_TIME_ESTABLISHMENT_TIMEOUT_SECONDS 30
#define STTS751_TEMP_HIGH_REG 0x00
#define STTS751_TEMP_LOW_REG 0x02
#define STTS751_CONFIG_REG 0x03

#define I2C2_NODE DT_NODELABEL(stts751)

int write_to_temp_sensor();

float read_from_temp_register();

void main_application_thread_fn(void);
