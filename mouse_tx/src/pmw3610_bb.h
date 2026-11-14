/* PMW3610 3線SPI（GPIOビットバン）ドライバ
 * Phase 1でArduino版として動作確認済みの手順を、そのままZephyrに移植したもの */
#ifndef PMW3610_BB_H_
#define PMW3610_BB_H_

#include <stdbool.h>
#include <stdint.h>

/* 初期化。成功で0、失敗で負の値（-ENODEV: Product ID不一致 など） */
int pmw3610_init(uint16_t cpi);

/* MOTピンがアクティブ（＝動きあり）ならtrue */
bool pmw3610_motion_pending(void);

/* 動き量を読み出す。動きがあればtrueを返し、dx/dyに12bitの符号付き値を入れる */
bool pmw3610_read_motion(int16_t *dx, int16_t *dy);

#endif
