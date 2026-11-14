# trackball_fw — 自作USBドングル式トラックボール ファームウェア

nRF Connect SDK **v3.4.1** 用。マウス側とドングル側の2本のアプリで構成しています。

```
[PMW3610] --GPIO(3線SPI)--> [XIAO nRF52840: mouse_tx] ~~ESB 2.4GHz~~> [Raytacドングル: dongle_rx] --USB HID--> [PC]
```

## フォルダ構成
```
trackball_fw
├─ common\
│   └─ tb_protocol.h       ← 無線の設定とパケット形式（両方で共通。変えたら両方ビルドし直す）
├─ mouse_tx\               ← マウス側（XIAO）
│   ├─ src\main.c          ← センサとボタンを読んでESBで送る
│   ├─ src\pmw3610_bb.c    ← PMW3610の読み出し（Phase 1で動作確認したビットバン方式）
│   ├─ app.overlay         ← ピン割り当て
│   ├─ prj.conf
│   └─ CMakeLists.txt
├─ dongle_rx\              ← ドングル側（Raytac MDBT50Q-CX-40）
│   ├─ src\main.c          ← ESBで受けてUSBマウスとしてPCへ送る
│   ├─ app.overlay         ← USB HIDマウスの定義
│   ├─ prj.conf            ← SoftDevice対策の3行入り
│   ├─ Kconfig
│   └─ CMakeLists.txt
└─ prebuilt\               ← ビルド済みファイル（すぐ試したいとき用）
    ├─ mouse_tx_xiao.uf2
    └─ dongle_rx_raytac.hex
```

## 1. まず試す（ビルド済みファイルを使う場合）

### マウス側（XIAO）
1. XIAOのリセットボタンを2回素早く押す → USBドライブが出る
2. `prebuilt\mouse_tx_xiao.uf2` をドライブにコピー

### ドングル側（Raytac）
1. ドングルをボタンを押しながら挿し、LEDが点灯するまで2〜3秒押す（ブートローダーモード）
2. Programmerで **「Clear files」→「Add file」で `prebuilt\dongle_rx_raytac.hex`**
3. 「Write」→ SoftDeviceは **「Custom」→ `0x102`**
4. ドングルを挿し直す

## 2. 自分でビルドする場合
1. `trackball_fw` フォルダを丸ごと `C:\ncs\app\` にコピーする
2. VS Codeの nRF Connect で「Open an existing application」から `mouse_tx` と `dongle_rx` をそれぞれ開く
3. ビルド設定を追加する

|アプリ|Board target|書き込むファイル|
|---|---|---|
|`mouse_tx`|`xiao_ble/nrf52840`|`build\mouse_tx\zephyr\zephyr.uf2`|
|`dongle_rx`|`raytac_mdbt50q_cx_40_dongle/nrf52840`|`build\dongle_rx\zephyr\zephyr.hex`|

> `common` フォルダを `mouse_tx` / `dongle_rx` と同じ階層に置いたままにしてください（`../common` を参照しています）。

## 3. 配線（マウス側）
Phase 1と同じです。
|PMW3610基板 J1|XIAO|
|---|---|
|1：+3V3|3V3|
|2：GND|GND|
|5：SCLK|D8|
|6：SDIO|D10|
|7：MOT|D2|
|8：NCS|D3|

ボタン（任意）：タクトスイッチの片側を **D0（左）／D1（右）／D4（中）**、反対側をGNDへ。つながなくても動きます。

## 4. 動作の見方
|場所|LED|意味|
|---|---|---|
|XIAO|緑が点滅|ドングルへの送信成功（動かしている間チカチカする）|
|XIAO|赤が点灯|送信失敗中（ドングルが見つからない）。ドングルが応答すると消える|
|XIAO|赤が点灯したまま、緑も点かない|センサの初期化失敗（配線・電源を確認）|
|ドングル|青が点滅|マウスからパケットを受信した|

PCからドングルは「USBマウス」＋「USBシリアル（COMポート）」として見えます。

**ログの見方**：XIAOもドングルもUSBシリアル（COMポート）にログを出します。シリアルモニタ（115200bps）で開くと、次のような表示が出ます。
- XIAO：`PMW3610 Product ID=0x3E` / `PMW3610 init OK` / `ESB ready`
- ドングル：`HID interface ready` / `Trackball dongle ready`

## 5. 調整
`mouse_tx\src\main.c` の先頭にあります。
|項目|初期値|内容|
|---|---|---|
|`SENSOR_CPI`|800|感度（200〜3200、200刻み）|
|`INVERT_X` / `INVERT_Y`|0|カーソルの左右／上下が逆なら1|
|`SWAP_XY`|0|縦横が入れ替わっているなら1|

## 6. 仕組み（ざっくり）
- **マウス側**：1msごとにMOTピンを見て、動きがあれば動き量を読んで溜める。前の送信が終わっていれば、溜まった分をまとめて1パケットで送る。届かなかった分は戻して次で送り直すので、カーソルの移動量は失われない
- **ドングル側**：受信したパケットをUSBマウスのレポート（±127単位）に変換して1ms周期で送る。大きく動いたときは複数のレポートに分割する
- **無線**：ESB 2Mbps、チャネル40（2440MHz）、ACK付き・最大3回再送。アドレスは `common\tb_protocol.h` で変更できる
- **センサ**：Zephyr標準のPMW3610ドライバは4線SPI前提のため、Phase 1で動作確認済みのビットバン方式をそのまま移植している

## 7. 今後の課題
- 電池駆動に向けた省電力化（スリープ、送信間隔、センサのRestモード）
- ホイール／スクロール機能
- ペアリング（アドレスを機器ごとに変える）
