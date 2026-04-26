#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#define NRF_SAADC_AIN6 6
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <stdlib.h>
#include <string.h>

/* AEM10330 status pins — inputs from PMIC */
#define PMIC_GPIO_DEV    DEVICE_DT_GET(DT_NODELABEL(gpio0))
#define PIN_ST_LOAD      3
#define PIN_ST_STO       4
#define PIN_ST_STO_RDY   28
#define PIN_ST_STO_OVDIS 29

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define I2C_DEV DEVICE_DT_GET(DT_NODELABEL(i2c0))

static const struct adc_channel_cfg batt_ch_cfg = {
	.gain             = ADC_GAIN_1_6,
	.reference        = ADC_REF_INTERNAL,
	.acquisition_time = ADC_ACQ_TIME_DEFAULT,
	.channel_id       = 0,
	.input_positive   = NRF_SAADC_AIN6,
};

/* VEML7700 */
#define VEML7700_ADDR     0x10
#define VEML7700_REG_CONF 0x00
#define VEML7700_REG_ALS  0x04

/* SHTC3 */
#define SHTC3_ADDR        0x70
#define SHTC3_CMD_WAKEUP  0x3517
#define SHTC3_CMD_MEASURE 0x7CA2
#define SHTC3_CMD_SLEEP   0xB098

/* Custom BLE service and characteristic UUIDs */
#define BT_UUID_SENSOR_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)
#define BT_UUID_LUX_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1)
#define BT_UUID_TEMP_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef2)
#define BT_UUID_HUM_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef3)
#define BT_UUID_INTERVAL_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef4)
#define BT_UUID_PMIC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef5)
#define BT_UUID_BATT_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef6)
#define BT_UUID_UPTIME_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef7)

#define BT_UUID_SENSOR_SERVICE  BT_UUID_DECLARE_128(BT_UUID_SENSOR_VAL)
#define BT_UUID_LUX_CHAR        BT_UUID_DECLARE_128(BT_UUID_LUX_VAL)
#define BT_UUID_TEMP_CHAR       BT_UUID_DECLARE_128(BT_UUID_TEMP_VAL)
#define BT_UUID_HUM_CHAR        BT_UUID_DECLARE_128(BT_UUID_HUM_VAL)
#define BT_UUID_INTERVAL_CHAR   BT_UUID_DECLARE_128(BT_UUID_INTERVAL_VAL)
#define BT_UUID_PMIC_CHAR       BT_UUID_DECLARE_128(BT_UUID_PMIC_VAL)
#define BT_UUID_BATT_CHAR       BT_UUID_DECLARE_128(BT_UUID_BATT_VAL)
#define BT_UUID_UPTIME_CHAR     BT_UUID_DECLARE_128(BT_UUID_UPTIME_VAL)

/* Sensor values shared with BLE */
static uint32_t ble_lux;
static int32_t  ble_temp;        /* millidegrees C */
static uint32_t ble_hum;         /* milli-percent RH */
static uint32_t sleep_interval_s = 60;
static uint8_t  ble_pmic[4] = {0, 0, 0, 0};
static uint32_t ble_batt_mv = 0; /* battery voltage in mV (×2 from divider) */
static uint32_t ble_uptime_s = 0;

/* GATT read/write callbacks */

static ssize_t read_lux(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			 void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &ble_lux, sizeof(ble_lux));
}

static ssize_t read_temp(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			  void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &ble_temp, sizeof(ble_temp));
}

static ssize_t read_hum(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			 void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &ble_hum, sizeof(ble_hum));
}

static ssize_t read_pmic(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			  void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, ble_pmic, sizeof(ble_pmic));
}

static ssize_t read_batt(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			  void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &ble_batt_mv, sizeof(ble_batt_mv));
}

static ssize_t read_uptime(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			    void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &ble_uptime_s, sizeof(ble_uptime_s));
}

static ssize_t read_interval(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			      void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &sleep_interval_s, sizeof(sleep_interval_s));
}

static ssize_t write_interval(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			       const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
	if (len != sizeof(uint32_t)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}
	uint32_t val;
	memcpy(&val, buf, sizeof(val));
	if (val < 5 || val > 3600) {
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}
	sleep_interval_s = val;
	LOG_INF("Sleep interval updated to %u s", sleep_interval_s);
	return len;
}

/*
 * GATT attribute index map (for bt_gatt_notify calls):
 *   [0]     primary service
 *   [1..3]  lux  (decl, value, CCC)       → notify at [2]
 *   [4..6]  temp (decl, value, CCC)       → notify at [5]
 *   [7..9]  hum  (decl, value, CCC)       → notify at [8]
 *   [10..11] interval (decl, value)       — read/write, no CCC
 *   [12..14] pmic (decl, value, CCC)      → notify at [13]
 *   [15..17] batt (decl, value, CCC)      → notify at [16]
 *   [18..20] uptime (decl, value, CCC)    → notify at [19]
 */
BT_GATT_SERVICE_DEFINE(sensor_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_SENSOR_SERVICE),

	BT_GATT_CHARACTERISTIC(BT_UUID_LUX_CHAR,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_READ, read_lux, NULL, NULL),
	BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

	BT_GATT_CHARACTERISTIC(BT_UUID_TEMP_CHAR,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_READ, read_temp, NULL, NULL),
	BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

	BT_GATT_CHARACTERISTIC(BT_UUID_HUM_CHAR,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_READ, read_hum, NULL, NULL),
	BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

	BT_GATT_CHARACTERISTIC(BT_UUID_INTERVAL_CHAR,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
		BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
		read_interval, write_interval, NULL),

	BT_GATT_CHARACTERISTIC(BT_UUID_PMIC_CHAR,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_READ, read_pmic, NULL, NULL),
	BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

	BT_GATT_CHARACTERISTIC(BT_UUID_BATT_CHAR,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_READ, read_batt, NULL, NULL),
	BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

	BT_GATT_CHARACTERISTIC(BT_UUID_UPTIME_CHAR,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_READ, read_uptime, NULL, NULL),
	BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

/* Advertising data */
static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_SENSOR_VAL),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
		sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static const struct bt_le_adv_param adv_param =
	BT_LE_ADV_PARAM_INIT(BT_LE_ADV_OPT_CONN,
			     BT_GAP_ADV_FAST_INT_MIN_2,
			     BT_GAP_ADV_FAST_INT_MAX_2,
			     NULL);

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("BLE disconnected (reason %u), restarting advertising", reason);
	bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.disconnected = disconnected,
};

/* --- Sensor functions --- */

static int veml7700_init(const struct device *i2c)
{
	uint8_t buf[] = {VEML7700_REG_CONF, 0x00, 0x00};
	return i2c_write(i2c, buf, sizeof(buf), VEML7700_ADDR);
}

static int veml7700_read_lux(const struct device *i2c, uint16_t *raw)
{
	uint8_t reg = VEML7700_REG_ALS;
	uint8_t data[2];

	int ret = i2c_write_read(i2c, VEML7700_ADDR, &reg, 1, data, 2);
	if (ret == 0) {
		*raw = (data[1] << 8) | data[0];
	}
	return ret;
}

static int shtc3_send_cmd(const struct device *i2c, uint16_t cmd)
{
	uint8_t buf[2] = {cmd >> 8, cmd & 0xFF};
	return i2c_write(i2c, buf, 2, SHTC3_ADDR);
}

static int shtc3_read(const struct device *i2c, int32_t *temp_mdeg, uint32_t *rh_mpct)
{
	int ret;

	ret = shtc3_send_cmd(i2c, SHTC3_CMD_WAKEUP);
	if (ret != 0) {
		return ret;
	}
	k_usleep(300);

	ret = shtc3_send_cmd(i2c, SHTC3_CMD_MEASURE);
	if (ret != 0) {
		return ret;
	}
	k_msleep(15);

	uint8_t data[6];
	ret = i2c_read(i2c, data, sizeof(data), SHTC3_ADDR);
	if (ret != 0) {
		return ret;
	}

	uint16_t raw_t  = (data[0] << 8) | data[1];
	uint16_t raw_rh = (data[3] << 8) | data[4];

	*temp_mdeg = -45000 + (int32_t)((175000LL * raw_t) / 65535);
	*rh_mpct   = (uint32_t)((100000ULL * raw_rh) / 65535);

	shtc3_send_cmd(i2c, SHTC3_CMD_SLEEP);
	return 0;
}

int main(void)
{
	LOG_INF("Sensor bringup started");
	k_msleep(500);

	int err = bt_enable(NULL);
	if (err) {
		LOG_ERR("BLE enable failed: %d", err);
		return err;
	}

	err = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("BLE advertising start failed: %d", err);
		return err;
	}
	LOG_INF("BLE advertising as \"%s\"", CONFIG_BT_DEVICE_NAME);

	/* PMIC GPIO */
	const struct device *pmic_gpio = PMIC_GPIO_DEV;
	if (!device_is_ready(pmic_gpio)) {
		LOG_ERR("GPIO device not ready");
		return -1;
	}
	gpio_pin_configure(pmic_gpio, PIN_ST_LOAD,     GPIO_INPUT | GPIO_PULL_DOWN);
	gpio_pin_configure(pmic_gpio, PIN_ST_STO,      GPIO_INPUT | GPIO_PULL_DOWN);
	gpio_pin_configure(pmic_gpio, PIN_ST_STO_RDY,  GPIO_INPUT | GPIO_PULL_DOWN);
	gpio_pin_configure(pmic_gpio, PIN_ST_STO_OVDIS, GPIO_INPUT | GPIO_PULL_DOWN);
	LOG_INF("PMIC GPIO pins configured");

	/* Battery ADC — manual config, AIN6 = P0.30, voltage divider ×2 */
	const struct device *adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc));
	if (!device_is_ready(adc_dev)) {
		LOG_ERR("ADC not ready");
		return -1;
	}
	err = adc_channel_setup(adc_dev, &batt_ch_cfg);
	if (err) {
		LOG_ERR("ADC channel setup failed: %d", err);
		return err;
	}
	LOG_INF("Battery ADC initialized (AIN6 / P0.30)");

	static int16_t adc_buf;
	static struct adc_sequence adc_seq = {
		.channels    = BIT(0),
		.buffer      = &adc_buf,
		.buffer_size = sizeof(adc_buf),
		.resolution  = 12,
	};

	/* I2C sensors */
	const struct device *i2c_dev = I2C_DEV;
	if (!device_is_ready(i2c_dev)) {
		LOG_ERR("I2C device not ready");
		return -1;
	}
	LOG_INF("I2C bus ready");

	if (veml7700_init(i2c_dev) != 0) {
		LOG_ERR("VEML7700 init failed");
		return -1;
	}
	LOG_INF("VEML7700 initialized");

	if (shtc3_send_cmd(i2c_dev, SHTC3_CMD_WAKEUP) != 0) {
		LOG_ERR("SHTC3 not found — check wiring at address 0x70");
		return -1;
	}
	k_usleep(300);
	shtc3_send_cmd(i2c_dev, SHTC3_CMD_SLEEP);
	LOG_INF("SHTC3 initialized");

	int cycle = 0;

	while (1) {
		LOG_INF("--- Wake cycle %d (interval: %u s) ---", cycle++, sleep_interval_s);

		/* PMIC status */
		int st_load  = gpio_pin_get(pmic_gpio, PIN_ST_LOAD);
		int st_sto   = gpio_pin_get(pmic_gpio, PIN_ST_STO);
		int st_rdy   = gpio_pin_get(pmic_gpio, PIN_ST_STO_RDY);
		int st_ovdis = gpio_pin_get(pmic_gpio, PIN_ST_STO_OVDIS);

		LOG_INF("PMIC: ST_LOAD=%d  ST_STO=%d  ST_STO_RDY=%d  ST_STO_OVDIS=%d",
			st_load, st_sto, st_rdy, st_ovdis);

		if (st_rdy == 1 && st_ovdis == 1) {
			LOG_ERR("PMIC: invalid state — RDY and OVDIS both high. Check wiring.");
		} else if (st_rdy == 1) {
			LOG_INF("PMIC: battery HEALTHY (>3.50V)");
		} else if (st_ovdis == 1) {
			LOG_WRN("PMIC: battery DEPLETED (<3.00V)");
		} else {
			LOG_WRN("PMIC: battery MID (3.00-3.50V)");
		}

		ble_pmic[0] = (uint8_t)st_load;
		ble_pmic[1] = (uint8_t)st_sto;
		ble_pmic[2] = (uint8_t)st_rdy;
		ble_pmic[3] = (uint8_t)st_ovdis;
		bt_gatt_notify(NULL, &sensor_svc.attrs[13], ble_pmic, sizeof(ble_pmic));

		/* Battery voltage via ADC */
		if (adc_read(adc_dev, &adc_seq) == 0) {
			int32_t mv = adc_buf;
			adc_raw_to_millivolts(600, ADC_GAIN_1_6, 12, &mv);
			ble_batt_mv = (uint32_t)(mv * 2); /* ×2 for 1:2 voltage divider */
			LOG_INF("Battery: %u mV (raw ADC: %d)", ble_batt_mv, adc_buf);
		} else {
			LOG_ERR("Battery ADC read failed");
		}
		bt_gatt_notify(NULL, &sensor_svc.attrs[16], &ble_batt_mv, sizeof(ble_batt_mv));

		/* Uptime */
		ble_uptime_s = (uint32_t)(k_uptime_get() / 1000);
		bt_gatt_notify(NULL, &sensor_svc.attrs[19], &ble_uptime_s, sizeof(ble_uptime_s));

		/* Environmental sensors */
		uint16_t raw_lux;
		if (veml7700_read_lux(i2c_dev, &raw_lux) == 0) {
			ble_lux = (uint32_t)(raw_lux * 288) / 10000;
			LOG_INF("VEML7700: raw=%u  lux=%u", raw_lux, ble_lux);
		} else {
			LOG_ERR("VEML7700 read failed");
		}

		if (shtc3_read(i2c_dev, &ble_temp, &ble_hum) == 0) {
			LOG_INF("SHTC3: temp=%d.%03d C  rh=%u.%03u %%",
				ble_temp / 1000, abs(ble_temp % 1000),
				ble_hum / 1000, ble_hum % 1000);
		} else {
			LOG_ERR("SHTC3 read failed");
		}

		bt_gatt_notify(NULL, &sensor_svc.attrs[2], &ble_lux, sizeof(ble_lux));
		bt_gatt_notify(NULL, &sensor_svc.attrs[5], &ble_temp, sizeof(ble_temp));
		bt_gatt_notify(NULL, &sensor_svc.attrs[8], &ble_hum, sizeof(ble_hum));

		LOG_INF("Sleeping for %u s...", sleep_interval_s);
		k_msleep(sleep_interval_s * 1000);
	}

	return 0;
}
