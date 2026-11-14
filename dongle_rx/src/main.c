/*
 * ドングル側（Raytac MDBT50Q-CX-40, nRF52840）
 *   ESBでマウスからのパケットを受信し、USB HIDマウスとしてPCへ送る
 *
 *   ・PCからは「USBマウス」＋「USBシリアル（ログ用COMポート）」に見える
 *   ・LED（P0.06）：パケットを受信するたびに点滅
 */
#include <sample_usbd.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usbd_hid.h>
#include <zephyr/logging/log.h>
#include <esb.h>

#include "tb_protocol.h"

LOG_MODULE_REGISTER(dongle_rx, LOG_LEVEL_INF);

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

/* 3ボタン＋X/Y/ホイール の標準的なマウス（4バイトのレポート） */
static const uint8_t hid_report_desc[] = HID_MOUSE_REPORT_DESC(3);
#define REPORT_LEN 4

/* ---- 受信データの受け渡し（ESB割り込み → メインスレッド） ---- */
struct mouse_msg {
	uint8_t buttons;
	int16_t dx;
	int16_t dy;
	int8_t wheel;
};
K_MSGQ_DEFINE(mouse_msgq, sizeof(struct mouse_msg), 32, 4);

static volatile bool hid_ready;

static void on_esb_event(const struct esb_evt *event)
{
	struct esb_payload rx;

	if (event->evt_id != ESB_EVENT_RX_RECEIVED) {
		return;
	}

	while (esb_read_rx_payload(&rx) == 0) {
		if (rx.length < TB_PKT_LEN || rx.data[0] != TB_PKT_TYPE_MOUSE) {
			continue;
		}
		struct mouse_msg m = {
			.buttons = rx.data[1],
			.dx = (int16_t)sys_get_le16(&rx.data[2]),
			.dy = (int16_t)sys_get_le16(&rx.data[4]),
			.wheel = (int8_t)rx.data[6],
		};
		(void)k_msgq_put(&mouse_msgq, &m, K_NO_WAIT);
	}
}

static int esb_setup(void)
{
	const uint8_t base0[4] = TB_BASE_ADDR_0;
	const uint8_t base1[4] = TB_BASE_ADDR_1;
	const uint8_t prefix[8] = TB_ADDR_PREFIX;
	struct esb_config config = ESB_DEFAULT_CONFIG;
	int err;

	config.protocol = ESB_PROTOCOL_ESB_DPL;
	config.mode = ESB_MODE_PRX;
	config.bitrate = ESB_BITRATE_2MBPS;
	config.selective_auto_ack = false;
	config.event_handler = on_esb_event;

	err = esb_init(&config);
	if (!err) {
		err = esb_set_base_address_0(base0);
	}
	if (!err) {
		err = esb_set_base_address_1(base1);
	}
	if (!err) {
		err = esb_set_prefixes(prefix, ARRAY_SIZE(prefix));
	}
	if (!err) {
		err = esb_set_rf_channel(TB_RF_CHANNEL);
	}
	if (!err) {
		err = esb_start_rx();
	}
	return err;
}

/* ---- USB HID ---- */
static void hid_iface_ready(const struct device *dev, const bool ready)
{
	LOG_INF("HID interface %s", ready ? "ready" : "not ready");
	hid_ready = ready;
}

static int hid_get_report(const struct device *dev, const uint8_t type,
			  const uint8_t id, const uint16_t len, uint8_t *const buf)
{
	return 0;
}

static const struct hid_device_ops hid_ops = {
	.iface_ready = hid_iface_ready,
	.get_report = hid_get_report,
};

static int8_t take_step(int32_t *v)
{
	int32_t s = CLAMP(*v, -127, 127);

	*v -= s;
	return (int8_t)s;
}

int main(void)
{
	const struct device *hid_dev;
	struct usbd_context *usbd;
	int err;

	gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);

	hid_dev = DEVICE_DT_GET_ONE(zephyr_hid_device);
	if (!device_is_ready(hid_dev)) {
		LOG_ERR("HID device not ready");
		return 0;
	}
	err = hid_device_register(hid_dev, hid_report_desc, sizeof(hid_report_desc), &hid_ops);
	if (err) {
		LOG_ERR("HID register failed (%d)", err);
		return 0;
	}

	usbd = sample_usbd_init_device(NULL);
	if (usbd == NULL) {
		LOG_ERR("USB init failed");
		return 0;
	}
	err = usbd_enable(usbd);
	if (err) {
		LOG_ERR("USB enable failed (%d)", err);
		return 0;
	}

	err = esb_setup();
	if (err) {
		LOG_ERR("ESB init failed (%d)", err);
		return 0;
	}
	LOG_INF("Trackball dongle ready (ESB PRX, channel %d)", TB_RF_CHANNEL);

	while (1) {
		UDC_STATIC_BUF_DEFINE(report, REPORT_LEN);
		struct mouse_msg m;
		int32_t dx, dy, wheel;
		uint8_t buttons;

		k_msgq_get(&mouse_msgq, &m, K_FOREVER);
		buttons = m.buttons;
		dx = m.dx;
		dy = m.dy;
		wheel = m.wheel;

		/* 溜まっているパケットのうち、ボタン状態が同じものはまとめる */
		while (k_msgq_peek(&mouse_msgq, &m) == 0 && m.buttons == buttons) {
			k_msgq_get(&mouse_msgq, &m, K_NO_WAIT);
			dx += m.dx;
			dy += m.dy;
			wheel += m.wheel;
		}

		gpio_pin_toggle_dt(&led);

		if (!hid_ready) {
			continue;
		}

		/* 1レポートで送れるのは ±127 まで。超えた分は分割して送る */
		do {
			report[0] = buttons;
			report[1] = (uint8_t)take_step(&dx);
			report[2] = (uint8_t)take_step(&dy);
			report[3] = (uint8_t)take_step(&wheel);

			err = hid_device_submit_report(hid_dev, REPORT_LEN, report);
			if (err) {
				LOG_WRN("HID submit failed (%d)", err);
				break;
			}
		} while (dx != 0 || dy != 0 || wheel != 0);
	}
	return 0;
}
