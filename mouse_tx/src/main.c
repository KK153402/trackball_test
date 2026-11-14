/*
 * マウス側（Seeed XIAO nRF52840）
 *   PMW3610の動き量とボタン状態を、ESBでドングルへ送る
 *
 *   ・1ms周期でMOTピンを確認し、動きがあれば動き量を読んで溜めておく
 *   ・前の送信が終わっていれば、溜まった分をまとめて1パケットで送る
 *   ・送信失敗（ドングルから応答なし）のときは、その分を戻して次回また送る
 *
 *   LED：緑＝送信成功のたびに点滅、赤＝送信失敗中（ドングルが見つからない）
 */
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <esb.h>

#include "pmw3610_bb.h"
#include "tb_protocol.h"

LOG_MODULE_REGISTER(mouse_tx, LOG_LEVEL_INF);

/* ===== 調整用 ===== */
#define SENSOR_CPI        800    /* 200〜3200（200刻み） */
#define INVERT_X          0      /* カーソルの左右が逆なら 1 */
#define INVERT_Y          0      /* カーソルの上下が逆なら 1 */
#define SWAP_XY           0      /* 縦横が入れ替わっているなら 1 */
#define LOOP_PERIOD_MS    1      /* センサ確認の周期 */
#define DEBOUNCE_COUNT    5      /* ボタンが同じ状態で何回続いたら確定するか（×周期） */
/* ================== */

#define USER_NODE DT_PATH(zephyr_user)

static const struct gpio_dt_spec btns[] = {
	GPIO_DT_SPEC_GET_BY_IDX(USER_NODE, btn_gpios, 0),   /* 左 */
	GPIO_DT_SPEC_GET_BY_IDX(USER_NODE, btn_gpios, 1),   /* 右 */
	GPIO_DT_SPEC_GET_BY_IDX(USER_NODE, btn_gpios, 2),   /* 中 */
};
static const struct gpio_dt_spec led_red   = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

/* ---- ESBイベント（割り込みから呼ばれる）→ メインループへ結果だけ渡す ---- */
enum tx_state { TX_IDLE, TX_BUSY, TX_DONE_OK, TX_DONE_FAIL };
static atomic_t tx_state = ATOMIC_INIT(TX_IDLE);

static void on_esb_event(const struct esb_evt *event)
{
	struct esb_payload rx;

	switch (event->evt_id) {
	case ESB_EVENT_TX_SUCCESS:
		atomic_set(&tx_state, TX_DONE_OK);
		break;
	case ESB_EVENT_TX_FAILED:
		atomic_set(&tx_state, TX_DONE_FAIL);
		break;
	case ESB_EVENT_RX_RECEIVED:
		/* ACKに付いてくるデータは今は使わないので読み捨てる */
		while (esb_read_rx_payload(&rx) == 0) {
		}
		break;
	default:
		break;
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
	config.mode = ESB_MODE_PTX;
	config.bitrate = ESB_BITRATE_2MBPS;
	config.retransmit_delay = 600;
	config.retransmit_count = 3;
	config.tx_mode = ESB_TXMODE_AUTO;
	config.selective_auto_ack = false;   /* 全パケットで受信確認（ACK）をもらう */
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
	return err;
}

static int16_t clamp16(int32_t v)
{
	if (v > INT16_MAX) {
		return INT16_MAX;
	}
	if (v < INT16_MIN) {
		return INT16_MIN;
	}
	return (int16_t)v;
}

int main(void)
{
	int err;
	int32_t acc_dx = 0, acc_dy = 0;          /* まだ送っていない動き量 */
	int16_t inflight_dx = 0, inflight_dy = 0; /* 送信中のパケットの動き量 */
	uint8_t btn_stable = 0, btn_last_raw = 0, btn_sent = 0xFF;
	uint8_t btn_count = 0;
	uint8_t seq = 0;
	bool link_ok = true;
	int64_t tx_start_ms = 0;
	int64_t next_tx_ms = 0;   /* 送信失敗中は100msおきにだけ再送する */

	LOG_INF("Trackball mouse (XIAO nRF52840 / ESB PTX) start");

	gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
	for (size_t i = 0; i < ARRAY_SIZE(btns); i++) {
		gpio_pin_configure_dt(&btns[i], GPIO_INPUT);
	}

	err = pmw3610_init(SENSOR_CPI);
	if (err) {
		LOG_ERR("PMW3610 init failed (%d). Check wiring / power.", err);
		gpio_pin_set_dt(&led_red, 1);     /* 赤点灯のまま停止 */
		return 0;
	}

	err = esb_setup();
	if (err) {
		LOG_ERR("ESB init failed (%d)", err);
		gpio_pin_set_dt(&led_red, 1);
		return 0;
	}
	LOG_INF("ESB ready, channel %d", TB_RF_CHANNEL);

	while (1) {
		/* ---- 1) センサ ---- */
		if (pmw3610_motion_pending()) {
			int16_t dx, dy;

			if (pmw3610_read_motion(&dx, &dy)) {
#if SWAP_XY
				int16_t t = dx; dx = dy; dy = t;
#endif
#if INVERT_X
				dx = -dx;
#endif
#if INVERT_Y
				dy = -dy;
#endif
				acc_dx += dx;
				acc_dy += dy;
			}
		}

		/* ---- 2) ボタン（簡易チャタリング除去） ---- */
		uint8_t raw = 0;

		for (size_t i = 0; i < ARRAY_SIZE(btns); i++) {
			if (gpio_pin_get_dt(&btns[i]) == 1) {
				raw |= BIT(i);
			}
		}
		if (raw == btn_last_raw) {
			if (btn_count < DEBOUNCE_COUNT) {
				btn_count++;
			} else {
				btn_stable = raw;
			}
		} else {
			btn_last_raw = raw;
			btn_count = 0;
		}

		/* ---- 3) 送信結果の処理 ---- */
		int st = atomic_get(&tx_state);

		/* 念のため：送信完了の通知が50ms来なければ失敗扱いにする */
		if (st == TX_BUSY && (k_uptime_get() - tx_start_ms) > 50) {
			atomic_cas(&tx_state, TX_BUSY, TX_DONE_FAIL);
			st = atomic_get(&tx_state);
		}

		if (st == TX_DONE_OK) {
			inflight_dx = inflight_dy = 0;
			if (!link_ok) {
				LOG_INF("Link recovered");
				link_ok = true;
				gpio_pin_set_dt(&led_red, 0);
			}
			gpio_pin_toggle_dt(&led_green);
			atomic_set(&tx_state, TX_IDLE);
		} else if (st == TX_DONE_FAIL) {
			/* 届かなかった分は戻して、次のパケットでまとめて送る */
			acc_dx += inflight_dx;
			acc_dy += inflight_dy;
			inflight_dx = inflight_dy = 0;
			btn_sent = 0xFF;                  /* ボタン状態も送り直す */
			(void)esb_flush_tx();
			next_tx_ms = k_uptime_get() + 100;
			if (link_ok) {
				LOG_WRN("TX failed (dongle not responding?)");
				link_ok = false;
				gpio_pin_set_dt(&led_red, 1);
			}
			atomic_set(&tx_state, TX_IDLE);
		}

		/* ---- 4) 送信 ---- */
		if (atomic_get(&tx_state) == TX_IDLE && k_uptime_get() >= next_tx_ms &&
		    (acc_dx != 0 || acc_dy != 0 || btn_stable != btn_sent)) {
			struct esb_payload pkt = {
				.pipe = TB_PIPE,
				.length = TB_PKT_LEN,
				.noack = false,
			};

			inflight_dx = clamp16(acc_dx);
			inflight_dy = clamp16(acc_dy);
			acc_dx -= inflight_dx;
			acc_dy -= inflight_dy;

			pkt.data[0] = TB_PKT_TYPE_MOUSE;
			pkt.data[1] = btn_stable;
			sys_put_le16((uint16_t)inflight_dx, &pkt.data[2]);
			sys_put_le16((uint16_t)inflight_dy, &pkt.data[4]);
			pkt.data[6] = 0;
			pkt.data[7] = seq++;

			atomic_set(&tx_state, TX_BUSY);
			tx_start_ms = k_uptime_get();
			err = esb_write_payload(&pkt);
			if (err) {
				/* FIFOが詰まっている等：戻して次回 */
				acc_dx += inflight_dx;
				acc_dy += inflight_dy;
				inflight_dx = inflight_dy = 0;
				(void)esb_flush_tx();
				atomic_set(&tx_state, TX_IDLE);
			} else {
				btn_sent = btn_stable;
			}
		}

		k_msleep(LOOP_PERIOD_MS);
	}
	return 0;
}
