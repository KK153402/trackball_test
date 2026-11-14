/*
 * PMW3610 3線SPI（GPIOビットバン）
 *   SPI Mode 3（CPOL=1, CPHA=1）、MSB first、書き込みはアドレスbit7=1
 *   レジスタと初期化手順は Zephyr drivers/input/input_pmw3610.c に準拠
 */
#include "pmw3610_bb.h"

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pmw3610_bb, LOG_LEVEL_INF);

#define USER_NODE DT_PATH(zephyr_user)

static const struct gpio_dt_spec sclk = GPIO_DT_SPEC_GET(USER_NODE, sclk_gpios);
static const struct gpio_dt_spec sdio = GPIO_DT_SPEC_GET(USER_NODE, sdio_gpios);
static const struct gpio_dt_spec ncs  = GPIO_DT_SPEC_GET(USER_NODE, ncs_gpios);
static const struct gpio_dt_spec mot  = GPIO_DT_SPEC_GET(USER_NODE, motion_gpios);

/* ---- レジスタ ---- */
#define REG_PROD_ID         0x00   /* 期待値 0x3E */
#define REG_REV_ID          0x01
#define REG_MOTION          0x02
#define REG_DELTA_X_L       0x03
#define REG_DELTA_Y_L       0x04
#define REG_DELTA_XY_H      0x05
#define REG_PERFORMANCE     0x11
#define REG_RUN_DOWNSHIFT   0x1B
#define REG_REST1_RATE      0x1C
#define REG_REST1_DOWNSHIFT 0x1D
#define REG_OBSERVATION1    0x2D
#define REG_POWER_UP_RESET  0x3A
#define REG_SPI_CLK_ON_REQ  0x41
#define REG_SPI_PAGE        0x7F   /* page0⇔page1 切替 */
#define REG_RES_STEP        0x05   /* page1 */

#define PRODUCT_ID          0x3E
#define CLK_ON              0xBA
#define CLK_OFF             0xB5
#define MOTION_MOT          BIT(7)

/* ---- ビットバンSPI ---- */
static inline void half_period(void)
{
	k_busy_wait(1);
}

static void send_byte(uint8_t v)
{
	gpio_pin_configure_dt(&sdio, GPIO_OUTPUT);
	for (int i = 7; i >= 0; i--) {
		gpio_pin_set_dt(&sclk, 0);          /* 立下りでデータを変える */
		gpio_pin_set_dt(&sdio, (v >> i) & 1);
		half_period();
		gpio_pin_set_dt(&sclk, 1);          /* 立上りでセンサが取り込む */
		half_period();
	}
}

static uint8_t recv_byte(void)
{
	uint8_t v = 0;

	gpio_pin_configure_dt(&sdio, GPIO_INPUT);
	for (int i = 7; i >= 0; i--) {
		gpio_pin_set_dt(&sclk, 0);          /* センサが立下りで出力 */
		half_period();
		gpio_pin_set_dt(&sclk, 1);          /* 立上りで読む */
		v |= (uint8_t)((gpio_pin_get_dt(&sdio) & 1) << i);
		half_period();
	}
	return v;
}

static uint8_t read_reg(uint8_t addr)
{
	uint8_t v;

	gpio_pin_set_dt(&ncs, 0);
	send_byte(addr & 0x7F);
	k_busy_wait(5);                             /* アドレス→データ間ウェイト */
	v = recv_byte();
	gpio_pin_set_dt(&ncs, 1);
	k_busy_wait(20);
	return v;
}

static void write_reg(uint8_t addr, uint8_t val)
{
	gpio_pin_set_dt(&ncs, 0);
	send_byte(addr | 0x80);
	send_byte(val);
	gpio_pin_set_dt(&ncs, 1);
	k_busy_wait(50);
}

static void clk_on(void)
{
	write_reg(REG_SPI_CLK_ON_REQ, CLK_ON);
	k_busy_wait(300);
}

static void clk_off(void)
{
	write_reg(REG_SPI_CLK_ON_REQ, CLK_OFF);
}

static void set_cpi(uint16_t cpi)
{
	uint8_t v;

	if (cpi < 200) {
		cpi = 200;
	} else if (cpi > 3200) {
		cpi = 3200;
	}

	clk_on();
	write_reg(REG_SPI_PAGE, 0xFF);              /* page1へ */
	v = read_reg(REG_RES_STEP);
	v = (v & ~0x1F) | (uint8_t)(cpi / 200);
	write_reg(REG_RES_STEP, v);
	write_reg(REG_SPI_PAGE, 0x00);              /* page0へ戻す */
	clk_off();
}

static int16_t sext12(uint16_t v)
{
	return (v & 0x800) ? (int16_t)(v | 0xF000) : (int16_t)v;
}

int pmw3610_init(uint16_t cpi)
{
	uint8_t pid, rev, obs;

	if (!gpio_is_ready_dt(&sclk) || !gpio_is_ready_dt(&sdio) ||
	    !gpio_is_ready_dt(&ncs) || !gpio_is_ready_dt(&mot)) {
		return -ENODEV;
	}

	gpio_pin_configure_dt(&ncs, GPIO_OUTPUT_HIGH);
	gpio_pin_configure_dt(&sclk, GPIO_OUTPUT_HIGH);  /* Mode3: アイドルHigh */
	gpio_pin_configure_dt(&sdio, GPIO_INPUT);
	gpio_pin_configure_dt(&mot, GPIO_INPUT);
	k_msleep(50);                                     /* 電源投入後の待ち */

	write_reg(REG_POWER_UP_RESET, 0x5A);              /* ソフトリセット */
	k_msleep(10);

	pid = read_reg(REG_PROD_ID);
	rev = read_reg(REG_REV_ID);
	LOG_INF("PMW3610 Product ID=0x%02X (expect 0x3E), Rev ID=0x%02X", pid, rev);
	if (pid != PRODUCT_ID) {
		return -ENODEV;
	}

	clk_on();
	write_reg(REG_OBSERVATION1, 0x00);
	k_msleep(10);
	obs = read_reg(REG_OBSERVATION1);
	LOG_INF("OBSERVATION1=0x%02X (low nibble must be 0xF)", obs);
	if ((obs & 0x0F) != 0x0F) {
		clk_off();
		return -EIO;
	}

	for (uint8_t r = REG_MOTION; r <= REG_DELTA_XY_H; r++) {
		(void)read_reg(r);                        /* 溜まった動き量をクリア */
	}

	write_reg(REG_PERFORMANCE, 0x0D);
	write_reg(REG_RUN_DOWNSHIFT, 0x04);
	write_reg(REG_REST1_RATE, 0x04);
	write_reg(REG_REST1_DOWNSHIFT, 0x0F);
	clk_off();

	set_cpi(cpi);
	LOG_INF("PMW3610 init OK, CPI=%u", cpi);
	return 0;
}

bool pmw3610_motion_pending(void)
{
	return gpio_pin_get_dt(&mot) == 1;                /* Active Low → 1=動きあり */
}

bool pmw3610_read_motion(int16_t *dx, int16_t *dy)
{
	uint8_t m = read_reg(REG_MOTION);                 /* 先にMOTIONを読む（Δがラッチされる） */

	if (!(m & MOTION_MOT)) {
		return false;
	}

	uint8_t xl = read_reg(REG_DELTA_X_L);
	uint8_t yl = read_reg(REG_DELTA_Y_L);
	uint8_t xyh = read_reg(REG_DELTA_XY_H);

	*dx = sext12((uint16_t)(((xyh << 4) & 0xF00) | xl));
	*dy = sext12((uint16_t)(((xyh << 8) & 0xF00) | yl));
	return true;
}
