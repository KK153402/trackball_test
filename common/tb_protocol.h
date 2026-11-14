/*
 * マウス（XIAO）⇔ ドングル 共通定義
 *   ESB（Enhanced ShockBurst）の無線設定とパケット形式
 *   ※ この値を変えたら、マウス側とドングル側の両方をビルドし直すこと
 */
#ifndef TB_PROTOCOL_H_
#define TB_PROTOCOL_H_

#include <stdint.h>

/* ---- 無線設定 ---- */
#define TB_RF_CHANNEL      40          /* 2400 + 40 = 2440 MHz（0〜100） */
#define TB_BASE_ADDR_0     { 0x4B, 0x5A, 0x4D, 0x31 } /* 適当な固有値。他の機器と被らないように */
#define TB_BASE_ADDR_1     { 0xC2, 0xC2, 0xC2, 0xC2 }
#define TB_ADDR_PREFIX     { 0xA5, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8 }
#define TB_PIPE            0           /* パイプ0を使う */

/* ---- パケット形式（8バイト） ---- */
#define TB_PKT_TYPE_MOUSE  0x01
#define TB_PKT_LEN         8

/* data[0] : 種類（TB_PKT_TYPE_MOUSE）
 * data[1] : ボタン bit0=左, bit1=右, bit2=中
 * data[2..3] : dX（int16, リトルエンディアン）
 * data[4..5] : dY（int16, リトルエンディアン）
 * data[6] : ホイール（int8、今は未使用で0）
 * data[7] : シーケンス番号（デバッグ用）
 */
#define TB_BTN_LEFT        0x01
#define TB_BTN_RIGHT       0x02
#define TB_BTN_MIDDLE      0x04

#endif /* TB_PROTOCOL_H_ */
