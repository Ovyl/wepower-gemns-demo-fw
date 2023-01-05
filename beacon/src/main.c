/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/types.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/util_macro.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <stdlib.h>
#include <math.h>

#include "app_types.h"
#include "app_fram.h"
#include "app_encryption.h"
#include "app_accel.h"
#include "app_temp_pressure.h"


#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define MANUF_DATA_CUSTOM_START_INDEX 2
#define PAYLOAD_DEVICE_ID_INDEX 18
#define PAYLOAD_STATUS_BYTE_INDEX 20
#define PAYLOAD_RFU_BYTE_INDEX 21
#define PAYLOAD_ENCRYPTION_STATUS_ENC 0
#define PAYLOAD_ENCRYPTION_STATUS_CLEAR 1

static we_power_data_ble_adv_t we_power_data;
static struct bt_data we_power_adv_data;

#define DEVICE_ID 3
static u16_u8_t device_id;

//                                           |--------- ENCRYPTED ENCRYPTED ENCRYPTED ENCRYPTED ENCRYPTED ENCRYPTED ENCRYPTED ------------ |  id    id   status RFU
static uint8_t manf_data[22] = { 0x50, 0x57, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4F};
static struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
    BT_DATA(BT_DATA_MANUFACTURER_DATA, manf_data, 22),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0x50, 0x57)
};
/* Set Scan Response data */
static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};
static struct bt_le_adv_param ble_adv_data;

static void bt_ready(int err)
{
	//char addr_s[BT_ADDR_LE_STR_LEN];
	//bt_addr_le_t addr = {0};

    memcpy(&ble_adv_data, BT_LE_ADV_NCONN_IDENTITY, sizeof(ble_adv_data));
    ble_adv_data.interval_min = 0x20; // 20ms
    ble_adv_data.interval_max = 0x22; // 30ms

	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	/* Start advertising */
	err = bt_le_adv_start(&ble_adv_data, ad, ARRAY_SIZE(ad),
			      sd, ARRAY_SIZE(sd));
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	/* For connectable advertising you would use
	 * bt_le_oob_get_local().  For non-connectable non-identity
	 * advertising an non-resolvable private address is used;
	 * there is no API to retrieve that.
	 */

	// bt_id_get(&addr, &count);
	// bt_addr_le_to_str(&addr, addr_s, sizeof(addr_s));

	// printk("Beacon started, advertising as %s\n", addr_s);
}

void main(void)
{
    printk("Starting WePower gemns BLE Beacon Demo!\n"); 

    // Set the ID, we do this in the clear AND encrypted, for verification
    device_id.u16 = DEVICE_ID;
    we_power_data.data_fields.id = device_id;

    // Get the counter out of fram and increment it
    uint32_t fram_counter = 0;
    if(app_fram_service(&fram_counter) == FRAM_SUCCESS)
    {
        // Success! Add it to the payload
        we_power_data.data_fields.fram_counter.u32 = fram_counter;
    }
    else
    {
         we_power_data.data_fields.fram_counter.u32 = 0xFFFFFFFF;
    }
    
    // Get the accelerometer data
    static accel_data_t accel_data;
    if(app_accel_service(&accel_data) == ACCEL_SUCCESS)
    {
        // Success!
        we_power_data.data_fields.accel_x.i16 = accel_data.x_accel;
        we_power_data.data_fields.accel_y.i16 = accel_data.y_accel;
        we_power_data.data_fields.accel_z.i16 = accel_data.z_accel;
    }
    else
    {
        we_power_data.data_fields.accel_x.i16 = 0xFFFF;
        we_power_data.data_fields.accel_y.i16 = 0xFFFF;
        we_power_data.data_fields.accel_z.i16 = 0xFFFF;
    }

    // Get temp and pressure
    temp_pressure_data_t temp_pressure_data;
    if(app_temp_pressure_service(&temp_pressure_data) == TEMP_PRESSURE_SUCCESS)
    {
        we_power_data.data_fields.pressure.i16 = temp_pressure_data.pressure;
        we_power_data.data_fields.temp.i16 = temp_pressure_data.temp;
    }
    else
    {
        we_power_data.data_fields.pressure.i16 = 0xFFFF;
        we_power_data.data_fields.temp.i16 = 0xFFFF;
    }

    // Handle encryption
    static uint8_t cipher_text[DATA_SIZE_BYTES];

#define ENCRYPT
#ifdef ENCRYPT
    // Not we want to encrypt the data
    if(app_encrypt_payload(we_power_data.data_bytes, DATA_SIZE_BYTES, cipher_text, DATA_SIZE_BYTES) == ENCRYPTION_ERROR)
    {
        memcpy(cipher_text, we_power_data.data_bytes, 16);
        manf_data[PAYLOAD_STATUS_BYTE_INDEX] = PAYLOAD_ENCRYPTION_STATUS_CLEAR;
    }
    else
    {
        manf_data[PAYLOAD_STATUS_BYTE_INDEX] = PAYLOAD_ENCRYPTION_STATUS_ENC;
    }
#else
    memcpy(cipher_text, we_power_data.data_bytes, 16);
    manf_data[PAYLOAD_STATUS_BYTE_INDEX] = PAYLOAD_ENCRYPTION_STATUS_CLEAR;
#endif

    printk("Payload - Cleartext: ");
    for(int i = 0; i < DATA_SIZE_BYTES; i++)
    {
        printk("%02X ", we_power_data.data_bytes[i]);
    }
    printk("\n");

    printk("Payload - Encrypted: ");
    for(int i = 0; i < DATA_SIZE_BYTES; i++)
    {
        printk("%02X ", cipher_text[i]);
    }
    printk("\n");

    // Build the BLE adv data
    memcpy(&manf_data[MANUF_DATA_CUSTOM_START_INDEX], cipher_text, DATA_SIZE_BYTES);
    memcpy(&manf_data[PAYLOAD_DEVICE_ID_INDEX], device_id.u8, 2);

    we_power_adv_data.type = BT_DATA_SVC_DATA16;
    we_power_adv_data.data = manf_data;
    we_power_adv_data.data_len = sizeof(manf_data);

    // Init and run the BLE
	int err = bt_enable(bt_ready);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
	}
    printk("BLE Running...\n");  

    //k_msleep(400);
    //bt_le_adv_stop();
}
