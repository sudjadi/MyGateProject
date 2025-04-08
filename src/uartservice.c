/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

//#include <zephyr/types.h>
//#include <stddef.h>
//#include <string.h>
//#include <errno.h>
#include <zephyr/sys/printk.h>
//#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
//#include <zephyr/drivers/gpio.h>
//#include <soc.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
//#include <zephyr/bluetooth/conn.h>
//#include <zephyr/bluetooth/uuid.h>
//#include <zephyr/bluetooth/gatt.h>

//#include <bluetooth/services/lbs.h>
#include <bluetooth/services/nus.h>
#include <zephyr/settings/settings.h>
//#include "include\uartservice.h"
//#include <dk_buttons_and_leds.h>

#define DEVICE_NAME             CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN         (sizeof(DEVICE_NAME) - 1)


#define RUN_STATUS_LED          DK_LED1
#define CON_STATUS_LED          DK_LED2
#define RUN_LED_BLINK_INTERVAL  1000

#define USER_LED                DK_LED3

#define USER_BUTTON             DK_BTN1_MSK

char rxBuf[10],txBuf[10];

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_VAL),  // Advertise NUS UUID
};

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("Connection failed, err 0x%02x %s\n", err, bt_hci_err_to_str(err));
		return;
	}
    printk("Connected. Enforcing secure pairing...\n");
    bt_conn_set_security(conn, BT_SECURITY_L2);  // Force Passkey Entry
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected, reason 0x%02x %s\n", reason, bt_hci_err_to_str(reason));
//	bt_unpair(BT_ID_DEFAULT, NULL);
//	dk_set_led_off(CON_STATUS_LED);
}

#ifdef CONFIG_BT_SMP
static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		printk("Security changed: %s level %u\n", addr, level);
	} else {
		printk("Security failed: %s level %u err %d %s\n", addr, level, err,
		       bt_security_err_to_str(err));
	}
	bt_unpair(BT_ID_DEFAULT, NULL);
}
#endif



#if defined(CONFIG_BT_SMP)
static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Passkey for %s: %06u\n", addr, passkey);
}

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing cancelled: %s\n", addr);
}

static void pairing_complete(struct bt_conn *conn, bool bonded)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing completed: %s, bonded: %d\n", addr, bonded);
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing failed conn: %s, reason %d %s\n", addr, reason,
	       bt_security_err_to_str(reason));
}

static struct bt_conn_auth_cb conn_auth_callbacks = {
	.passkey_display = auth_passkey_display,
	.passkey_entry = NULL,
	.cancel = auth_cancel,
};

static struct bt_conn_auth_info_cb conn_auth_info_callbacks = {
	.pairing_complete = pairing_complete,
	.pairing_failed = pairing_failed
};
#else
static struct bt_conn_auth_cb conn_auth_callbacks;
static struct bt_conn_auth_info_cb conn_auth_info_callbacks;
#endif

//static void app_led_cb(bool led_state)
//{
//	dk_set_led(USER_LED, led_state);
//}

//static bool app_button_cb(void)
//{
//	return app_button_state;
//}
static void nus_data_received(struct bt_conn *conn, const uint8_t *data, uint16_t length)
{
    // Print the received data
    printk("Received length: %d\n", length);
	for (int count=0;count < length;count++){
		printk("%c",data[count]);
	}
	printk("\n");
	bt_nus_send(NULL, data, length);
//	for (int count=0;count<length;count++){
//		bt_nus_send(NULL,txBuf,length);
//	}
}

static void nus_data_sent(struct bt_conn *conn)
{
    // Print whether sending data was successful
	int err;
    if (err == 0) {
        printk("Data successfully sent\n");
    } else {
        printk("Data sending failed, err: %d\n", err);
    }
}
static const struct bt_nus_cb nus_callbacks = {
    .received = nus_data_received,  // Data received callback
    .sent = nus_data_sent,      // Data sent callback
};
//static struct bt_lbs_cb lbs_callbacs = {
//	.led_cb    = app_led_cb,
//	.button_cb = app_button_cb,
//};

/*
static void button_changed(uint32_t button_state, uint32_t has_changed)
{
	if (has_changed & USER_BUTTON) {
		uint32_t user_button_state = button_state & USER_BUTTON;

		bt_lbs_send_button_state(user_button_state);
		app_button_state = user_button_state ? true : false;
	}
}
*/
/*
static int init_button(void)
{
	int err;

	err = dk_buttons_init(button_changed);
	if (err) {
		printk("Cannot init buttons (err: %d)\n", err);
	}

	return err;
}
*/
int initUsartService(void)
{
	int err;
	
	printk("Starting Bluetooth Peripheral LBS example\n");

	if (IS_ENABLED(CONFIG_BT_SMP)) {
		err = bt_conn_auth_cb_register(&conn_auth_callbacks);
		if (err) {
			printk("Failed to register authorization callbacks.\n");
			return 1;
		}

		err = bt_conn_auth_info_cb_register(&conn_auth_info_callbacks);
		if (err) {
			printk("Failed to register authorization info callbacks.\n");
			return 1;
		}
	}

	// Set a fixed passkey (6-digit number)
    err = bt_passkey_set(654321);
    if (err) {
        printk("Failed to set passkey (err %d)\n", err);
    } else {
        printk("Fixed passkey set to 123456\n");
    }
	
	BT_CONN_CB_DEFINE(conn_callbacks) = {
		.connected        = connected,
		.disconnected     = disconnected,
	#ifdef CONFIG_BT_SMP
		.security_changed = security_changed,
	#endif
	};

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return 1;
	}

	printk("Bluetooth initialized\n");
	//comment out for testing : to force pairing mode all the time
	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	/*
	err = bt_lbs_init(&lbs_callbacs);
	if (err) {
		printk("Failed to init LBS (err:%d)\n", err);
		return 0;
	}*/
	err = bt_nus_init(&nus_callbacks);
    if (err) {
        printk("Failed to initialize NUS (err: %d)\n", err);
        return 1;
    }

//	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad),sd, ARRAY_SIZE(sd));
	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return 1;
	}

	printk("Advertising successfully started\n");
    return 0;

//	for (;;) {
//	//	dk_set_led(RUN_STATUS_LED, (++blink_status) % 2);
//		k_sleep(K_MSEC(RUN_LED_BLINK_INTERVAL));
//	}
}
