# Hardware.md — 自作USBドングル式トラックボールマウス

## 0. このドキュメントの範囲（Phase 1）
- 目的：**ブレッドボード上でトラックボール（PMW3610）の動作を確認する**ところまで
- 構成：XIAO nRF52840 ＋ PMW3610動作確認用基板（BOB-PMW3610-SEIBOKU回路）＋ 3Dプリントのボールケース
- Phase 1の電源はXIAOのUSB給電（PMW3610基板へはXIAOの3V3のみ。**5Vは接続しない**）。単三＋TPS61220の昇圧は**単体評価のみ**（XIAOとの結合はPhase 2）
- ドングル（ESB無線）はPhase 2。部品は今回まとめて発注する

```
Phase 1-A（有線で追従確認）
[PC] ==USB== [XIAO nRF52840] --SPI 3線+MOT-- [PMW3610基板] ↔ [Φ44ボール / 3DPケース]
            (USB HIDマウスとして動作)

Phase 1-B（電源単体評価）
[単三×1] → [TPS61220 変換基板] → 3.0V → [抵抗負荷 / 電子負荷]
```

---

## 1. 部品リスト（発注用）

### 1.1 主要部品
|No|部品|型番|数量|購入先|価格(税込)|用途/備考|
|---|---|---|---|---|---|---|
|1|MCU|Seeed XIAO BLE nRF52840（無印。Senseではない）|1|[秋月 117341](https://akizukidenshi.com/catalog/g/g117341/)|—|マウス側MCU|
|2|USBドングル|Raytac nRF52840 MDBT50Q 開発用USBドングル（**Type-C, MDBT50Q-CX-40**）|1|[共立エレショップ OC6314](https://eleshop.jp/shop/g/gOC6314/)|—|受信側（Phase 2）。Open Bootloader書込み済→USB直挿しでDFU可（SWD不要）|
|3|光学センサ|PMW3610DM-SUDU ＋ LM18-LSI レンズ セット|1〜2|[TALP KEYBOARD](https://shop.talpkeyboard.com/products/pmw3610dm-sudu-lm18-lsi)|¥900/セット|予備1セット推奨（手はんだ失敗対策）|
|4|昇圧DC/DC|**TPS61220DCKR**（SC-70-6, 可変出力）|1〜2|DigiKey/Mouser等|—|リード付き（0.65mmピッチ）。固定3.3V版TPS61221は使わない（§4-1）|
|5|ボール|Φ44mm トラックボール用ボール|1|—|—|DEFT PRO系交換ボール等|
|6|支持球|Φ2.5mm セラミック球（ZrO2 or Si3N4）|3〜6|—|—|ボアΦ2.6に接着|

### 1.2 PMW3610動作確認用基板（BOB-PMW3610-SEIBOKU v1.2 そのまま）
出典：[snize/BOB-PMW3610-SEIBOKU](https://github.com/snize/BOB-PMW3610-SEIBOKU)（KiCad, CERN-OHL-P v2 → 製作時は著作権・ライセンス表示を残すこと）。基板30×20mm、部品はすべて0805（手はんだ用パッド）。
|Ref|値|ネット（KiCad PCBから抽出）|数量|
|---|---|---|---|
|U1|TLV70019DDCR（SOT-23-5）|IN/EN=+3V3, OUT=+1V9|1|
|U2|PMW3610DM-SUDU|—|1|
|C1, C3|0.1µF X7R|+3V3–GND（U2 VDDIO / U1 IN）|2|
|C2|1µF|+1V9–GND|1|
|C9|3.3µF/16V|+1V9–GND|1|
|C6|0.1µF X7R|+1V9–GND（U2 VDD）|1|
|C5|10µF X7R|VCP–GND|1|
|C8|0.01µF X7R|VCP–GND|1|
|C4|0.01µF X7R|+VCSEL–GND|1|
|C7|0.1µF X7R|CP–CN（チャージポンプ）|1|
|R1|10kΩ|NRESET → +3V3 プルアップ|1|
|J1|2×4 ピンヘッダ 2.54mm|—|1|

> 注：**TLV70019の出力は1.9V**（TLV700xxの末尾2桁＝出力電圧。1.8VはTLV70018）。PMW3610 VDD推奨範囲1.7〜2.1Vの範囲内で、TLV700の精度±2%なら1.862〜1.938V → OK。

J1ピン配置：
|J1|信号|J1|信号|
|---|---|---|---|
|1|+3V3|2|GND|
|3|NC|4|NC|
|5|SCLK|6|SDIO|
|7|MOT|8|NCS|

### 1.3 TPS61220 昇圧回路（単三1本 → 3.0V）
|Ref|値|推奨品番|数量|根拠|
|---|---|---|---|---|
|U|TPS61220DCKR（SC-70-6）|—|1|可変出力版（V_FB=0.5V）|
|L1|4.7µH、Isat≧0.5A|Coilcraft EPL3015-472MLB（DS参照設計）、Murata LQH3NP系 等|1|DS：4.7µHで全入出力範囲良好。Isat > I_SW max 400mA|
|C1（入力）|10µF X5R 6.3V以上|Murata GRM188R60J106ME84D|1|DS：入力10µF以上推奨|
|C2（出力）|10µF X5R 6.3V以上|Murata GRM188R60J106ME84D|1|DS：最小4.7µF、10µF推奨|
|C3（出力バルク）|47µF 6.3V以上（セラミック or 低ESR）|—|1|無線送信時のバースト電流用（§4-3。DS外の追加）|
|R1|1.0MΩ 1%|—|1|FB上側（VOUT–FB）|
|R2|200kΩ 1%|—|1|FB下側（FB–GND）。DS：R2＜500kΩ|
|—|SC-70-6／SOT-363 → DIP 変換基板（0.65mmピッチ）|—|1〜2|ブレッドボード用。L・Cは変換基板上に近接実装|
|—|単三電池ボックス 1本用（スイッチ付き）|—|1|TPS61220は停止中も入出力が導通するため、OFFは電池側スイッチで（§3.2）|

### 1.4 ブレッドボード周り
|部品|数量|備考|
|---|---|---|
|ブレッドボード（ハーフ以上）|1|手持ちで可|
|ジャンパワイヤ（オス-メス / メス-メス）|10〜|PMW3610基板J1⇔ブレッドボード|
|タクトスイッチ|3|左・右・中クリック（任意。キー仕様は未決）|
|電解 or セラミック 10µF|1|PMW3610基板の3V3入口に追加（配線が長いため。§4-2）|
|SWDデバッガ（J-Link EDU Mini 等）|0|今回は購入しない。ブートローダーを壊した場合の復旧用（必要になったら検討）|

---

## 2. 配線（Phase 1-A：XIAO ⇔ PMW3610基板）
XIAOピン→nRF52840ポートは [Seeed Wiki](https://wiki.seeedstudio.com/XIAO_BLE/) より。
|PMW3610基板 J1|信号|XIAO|nRF52840|備考|
|---|---|---|---|---|
|1|+3V3|3V3|—|XIAO内蔵LDO出力から供給|
|2|GND|GND|—|—|
|5|SCLK|D8|P1.13|SPIM SCK|
|6|SDIO|D10|P1.15|**MOSI/MISOを同一ピンに割当**（3線SPI）|
|7|MOT|D2|P0.28|Active Low、内部プルアップ有効|
|8|NCS|D3|P0.29|チップセレクト|
|—|左クリック|D0|P0.02|タクトSW→GND、内部プルアップ（任意）|
|—|右クリック|D1|P0.03|同上（任意）|
|—|中クリック|D4|P0.04|同上（任意）|

- 空き：D5, D9、D6/D7（UARTログ用に空けておく）
- SPIクロック：**≦2MHz**（PMW3610 DS：SCLK max 2MHz）→ DTで`spi-max-frequency = <2000000>`

---

## 3. 主要デバイスの確認結果

### 3.1 PMW3610DM-SUDU（PixArt DS）
|項目|値|
|---|---|
|VDD 推奨範囲|1.7 / 1.8 / 2.1 V（絶対最大 2.2V）|
|VDDIO 推奨範囲|1.7 / 1.8 / **3.3 V**（絶対最大 **3.4V**）|
|動作電流 Run / Rest1 / Rest2 / Rest3 / Shutdown|0.60mA / 36µA / 16µA / 7µA / 3µA|
|SCLK max|2MHz|
|レンズ基準面〜追従面 Z|2.2 / 2.4 / 2.6 mm（DOF ±0.2mm）|
|NRESET|Active Low（基板側で10kプルアップ）|
|MOTION|Active Low 出力|

### 3.2 TPS61220（TI DS「TPS6122x Low Input Voltage, 0.7V Boost Converter With 5.5µA Quiescent Current」）
|項目|値|
|---|---|
|パッケージ／ピン（DCK, SC-70-6）|1=VIN, 2=FB, 3=GND, 4=VOUT, 5=L, 6=EN|
|入力電圧範囲|0.7〜5.5V|
|起動条件|VIN=0.7V（R_Load≧150Ω）|
|出力電圧範囲（TPS61220）|1.8〜6.0V|
|V_FB|483 / 500 / 513 mV|
|I_FB|0.01µA typ|
|静止電流（I_O=0, VOUT=3.3V）|VIN：0.5 / 0.9µA、VOUT：5 / 7.5µA（typ / max）|
|シャットダウン電流（VEN=0V, VIN=1.2V）|0.2 / 0.5µA|
|スイッチ電流制限 I_SW（VOUT=3.3V）|200〜400mA|
|EN閾値（VIN≦1.5V）|VIL ≦ 0.2×VIN、VIH ≧ 0.8×VIN（フローティング禁止）|
|OVP|6.0V min|
|推奨L / C_IN / C_OUT|4.7µH / ≧10µF / ≧4.7µF（10µF推奨）|
|シャットダウン時|スイッチング停止。**整流MOSFETのバックゲートダイオード経由でVIN→VOUTが導通**（負荷切り離しなし）|

最大出力電流（DS Fig.1 の読み取り値、VOUT=3.3V、概略）：
|VIN|0.7V|0.9V|1.2V|1.5V|
|---|---|---|---|---|
|I_OUT(max)|約5mA|約15mA|約35mA|約55mA|

### 3.3 XIAO nRF52840 を3.0Vで動かせるか → **可**
|搭載部品|電源電圧範囲|3.0V給電|出典|
|---|---|---|---|
|nRF52840（VDD、Normal voltage mode）|1.7〜3.6V|OK|nRF52840 PS v1.5|
|QSPIフラッシュ P25Q16H（2MB）|2.3〜3.6V|OK（マージン0.7V）|P25Q16H DS|
|LDO SGM2040-3.3（VBUS/BAT→3V3）|—|3V3ピンへの逆給電可（下記）|SGM2040 DS|
|RGB LED|—|青はVfが高く暗くなる可能性あり（実害なし）|—|

- **SGM2040には逆流防止回路がある**（DS）：VOUT − VIN が約800mV（typ）を超えるとパスPFETがOFFになり、逆リークは0.4µA typ
  - 電池運用時はVBUS/BATが開放（≈0V）なので、3V3ピンに外部から3.0Vを入れても逆流しない
- **USBと電池を同時に接続した場合**：SGM2040の3.3Vが3V3ラインに出て、TPS61220はVOUTが目標（3.0V）より高いため停止する（VOUT>VINなのでバックゲートダイオードは逆バイアスとなり、電池側へは流れない）
  - このとき3V3ラインは3.3Vになる。PMW3610 VDDIOの推奨上限3.3Vに対して、LDO精度±2%を考えると最大3.37Vで、推奨上限は少し超えるが絶対最大3.4V未満
  - 開発中は許容範囲。製品の運用としては「USB接続時は電池OFF」を推奨
- **3V3ピン給電時の既知の問題**（Seeedフォーラム）：VBUSなしで起動するとHFCLKが自動で有効にならず、SPIM3（32MHz）が動かない
  - 対策：PMW3610には**SPIM0〜2を使う**（SCLKは2MHzなので十分）か、ファームでHFCLKを明示的に起動する

### 3.4 TLV70019DDCR（TI TLV700 DS）
- VIN 2.0〜5.5V、出力精度±2%、Iq 31µA typ
- 入出力1µFセラミック推奨。出力は実効0.1µF以上で安定
- ピン：1=IN, 2=GND, 3=EN, 4=NC, 5=OUT（SOT-23-5 DDC）

---

## 4. 設計計算・判断

### 4-1. 昇圧出力を3.3Vではなく**3.0V**にする
- PMW3610の**VDDIO推奨上限3.3V／絶対最大3.4V**（DS）
- 固定3.3V版TPS61221の出力精度は3.20〜3.41V（DS）。上限3.41Vが絶対最大3.4Vを超えるため使わない
- 可変版TPS61220で3.0Vに設定する。XIAO側の部品も3.0Vで問題なし（§3.3：nRF52840は1.7〜3.6V、P25Q16Hは2.3〜3.6V）。TLV70019の入力条件（≧2.0V、ドロップアウト込み）も満たす

### 4-2. FB抵抗
- 式：R1 = R2 × (V_OUT / V_FB − 1)（DS）
- 分圧器の電流はI_FBの約100倍以上が目安。DSの推奨はR2＜500kΩ（分圧電流1µA以上）
- **R1 = 1.0MΩ、R2 = 200kΩ**とすると：
  - V_OUT = 0.5 × (1 + 1000/200) = **3.00V**
  - V_FBばらつき（483〜513mV）込みで 2.90〜3.08V。抵抗1%を重ねても最悪約2.84〜3.14V → VDDIO推奨上限3.3V以下、P25Q16Hの下限2.3V以上でOK
  - 分圧器の電流：0.5V / 200kΩ = 2.5µA（I_FB 0.01µAの250倍）

### 4-3. 最大出力電流と負荷の見積もり（重要）
- 自前の概算（スイッチ電流制限の最小値で、DCMの三角波を想定）：I_OUT ≈ V_IN × (I_SW,min/2) × η / V_OUT
  - VIN=0.9Vのとき：0.9 × 0.1A × 0.8 / 3.0 ≈ **24mA**
  - VIN=1.2Vのとき：1.2 × 0.1 × 0.8 / 3.0 ≈ **32mA**
  - DS Fig.1の読み取り値（0.9Vで約15mA、1.2Vで約35mA）とおおむね一致する
- 負荷側の見積もり（ピーク）：
  - nRF52840の無線送信と受信：数mA〜十数mA（送信出力とDCDCの使用有無による）
  - CPU：数mA
  - PMW3610：Run 0.6mA
  - 合計で**ピーク10〜20mA程度**
- 判定：
  - 電池電圧1.2V以上なら十分足りる
  - **電池末期（0.9V付近）では余裕がほぼない**。無線のピークは短いため、出力側のバルクコンデンサで補う
  - 例：送信時に+10mAが200µs流れた場合、47µFで ΔV = 0.01 × 200e-6 / 47e-6 ≈ **43mV**
- 対策：
  - C3（47µF）を追加する
  - ファームで電池電圧を監視し、**1.0V付近で電池切れ警告・停止**とする
  - 送信出力は0dBm以下に抑える
  - XIAOのnRF52840内蔵DCDC（REG1）が使えるかを確認する（要確認）

### 4-4. 電池寿命の目安（3.0V系で再計算）
- 使えるエネルギー：1.2V × 2000mAh × 0.85 ≈ 2.04Wh → 3.0V換算で約680mAh
- 昇圧ICの自己消費（電池側換算）：TPS61220は I_Q(VOUT) × V_OUT / (V_IN × η) + I_Q(VIN) = 5µA × 3.0 / (1.2 × 0.85) + 0.5µA ≈ **15µA**
- 分圧抵抗の電流2.5µAも同じ換算で約7µAが加わる
- **注意（最終設計への課題）**：TLV70019のIq 31µAは、PMW3610のRest3（7µA）やShutdown（3µA）より大きい
  - 待機時のボトルネックになるため、本番基板ではnA級Iqの1.8〜1.9V LDOへの変更か、ロードスイッチでの遮断を検討する

### 4-5. ブレッドボード実装上の注意
- PMW3610基板の3V3入力側はC1・C3（各0.1µF）だけ。ジャンパ線が長いので、基板J1の近くに10µFを追加する
- スイッチング電源は配線長に敏感。ブレッドボードに直接組まず、**TPS61220の変換基板上にC1・L1・C2を最短配置**し、出力だけをブレッドボードへ出す
- ENはVINに直結する（VIN≦1.5VではVIH ≧ 0.8×VIN。直結なら1.0×VINで条件を満たす）

---

## 5. 注意事項・リスク

1. **TPS61220の出力電流**：電池末期の供給能力が小さい（§4-3）。電池電圧の監視と停止電圧の設定は必須。シャットダウン中も負荷が切り離されないため、電源OFFは電池側のスイッチで行う
2. **XIAOへの外部3.0V給電（Phase 2）**：Seeed Wikiでは3V3ピンは「出力」扱いだが、SGM2040の逆流防止により給電は可能（§3.3）
   - 運用は「USB接続時は電池OFF」とする
   - SPIはSPIM0〜2を使う
3. **ドングル（Type-C, MDBT50Q-CX-40）の書き込み**（2026-11-01 確認済み）
   - 工場出荷時の中身：
     - Nordic Open Bootloader（0xE0000〜）
     - **SoftDevice S113 v7.2.0**（FWID **0x102**、0x1000〜0x1BFFF、110,592バイト）
     - デモアプリ「nRF52 USB CDC BLE Demo」
   - ブートローダーへの入り方：**ボタンを押しながらUSBに挿し、LEDが点灯するまで2〜3秒押し続けてから離す**。Programmerに「Open DFU Bootloader」と表示されればOK（挿してから押すだけでは、デモアプリがボタンに反応するだけ）
   - ボードターゲット：`raytac_mdbt50q_cx_40_dongle/nrf52840`（`/bare`系は使わない）
   - SoftDeviceはブートローダー経由では消せないので、残したままアプリをその後ろ（0x1C000）に置く。ESBとUSBにSoftDeviceは不要。`prj.conf`に次を入れる：
     ```
     CONFIG_USE_DT_CODE_PARTITION=n
     CONFIG_FLASH_LOAD_OFFSET=0x1C000
     CONFIG_FLASH_LOAD_SIZE=0xC4000
     ```
     （0xC4000 = 0xE0000 − 0x1C000。ブートローダー手前まで）
   - Programmerの「Write」でSoftDeviceを聞かれたら、「Custom」→ **`0x102`** を入力する。`0x00`（SoftDeviceなし）だと `SdVersionFailure` で拒否される
   - コマンドで書く場合：`nrfutil nrf5sdk-tools pkg generate --hw-version 52 --sd-req 0x102 --application <hex> --application-version 1 app.zip` のあと、`nrfutil nrf5sdk-tools dfu usb-serial -pkg app.zip -p COMx`
   - 書き込み後に出る「Failed to detect device after reboot」は、アプリが起動してブートローダーとして見えなくなったためで、問題なし
   - ブートローダー領域を上書きするビルド（SWD前提の設定）は書き込まない
   - **LEDの実際のピンは P0.06**（2026-11-05 実機で確認）
     - 回路図の画像（D1＝P0.08）とは食い違っている。Zephyrのボード定義（`led0_d1` = P0.06）の方が正しかった
     - ボード標準の `led0` をそのまま使えばよい。**P0.08に切り替えるoverlayは入れない**
   - Programmerは、読み込んだファイルを残したまま次のファイルを追加すると両方まとめて書き込む（「Some of the HEX files have overlapping data」の警告が出る）。**書き込む前に必ず「Clear files」を押す**
   - ボードターゲットは名前の似た `raytac_mdbt50q_db_40`（デモボード用）と取り違えやすい。ビルドログの `-- Board:` 行で確認する
4. **PMW3610基板とボールケースの高さ**：
   - SEIBOKUのREADMEではレンズ部の厚みが約4.0mm。一方、発注済みケースは`h_pcb = 2.4`（仮値）で作っている
   - 実測して4.0mmなら、レンズが1.6mm近すぎる（ボール面からの距離0.8mm、DOF外）
   - 試験時はパッドと基板の間に**1.6mm厚スペーサ**（基板端材など）を挟んで合わせる。次の版で`h_pcb`を修正する
   - 基板30×20mmはケースのポケット24.1×32.1mmに、**20mm辺をX方向**に向ければ入る。SEIBOKUは基板中心と光学中心が一致しているので、ケース側の想定（オフセット0）とも合う
5. PMW3610は購入元の注意書きどおり、**はんだ付けの前に単体で動作確認**する（初期不良は交換対応）

---

## 6. Phase 1 確認手順・結果
|No|内容|結果|日付|
|---|---|---|---|
|P1-1|PMW3610基板の実装・電源確認（+3V3、+1V9）|✅ OK|2026-10-25|
|P1-2|MOT動作確認|✅ OK（P1-3で確認）|2026-10-25|
|P1-3|SPI通信・ΔX/ΔY取得|✅ OK：Product ID=0x3E、Rev ID=0x01、OBSERVATION1=0x2F（下位4bit=0xF）、動かした方向に応じてdx/dyの符号が変わる|2026-10-25|
|P1-4|USB HIDマウス動作（PC上でカーソル追従）|✅ OK|2026-10-25|
|P1-5|3DPケース＋Φ44ボールでの追従|✅ OK（ボール操作でカーソル移動を確認）|2026-10-25|
|P1-6|TPS61220単体評価|⏸ 保留：昇圧回路の基板製作時に実施|—|
|P1-7|測定結果を本ファイルと`trackball_cup.scad`に反映|△ 一部：センサ高さ（スペーサ厚）の実測値は未記録|—|

### 6.1 Phase 1 の実施環境（再現用）
- 開発環境：PlatformIO（`pmw3610_test_pio`）
  - `platform = https://github.com/Seeed-Studio/platform-seeedboards.git`
  - `board = seeed-xiao-afruitnrf52-nrf52840`（Adafruit nRF52ベース、非mbed）
  - `lib_archive = no`、`upload_protocol = nrfutil`（USB書込み）
- ファーム：3線SPIはGPIOビットバン（Mode 3、1MHz未満）。レジスタと初期化手順はZephyr `input_pmw3610.c`に準拠。CPIは800
- 給電：XIAOのUSB → XIAOの3V3 → センサ基板J1-1（5Vは未接続）
- `USE_USB_MOUSE`：0でシリアル出力、1でUSB HIDマウス

### 6.2 P1-6（保留）の実施内容
- 単三1本で3.0V出力を確認（無負荷／軽負荷時の電圧とリップル）
- 負荷3kΩ（1mA）、300Ω（10mA）、150Ω（20mA）で、VIN 0.9〜1.6Vの範囲の出力電圧を確認（安定化電源で電池電圧を模擬）
- 何mAまで出力を維持できるかを測り、§4-3の見積もりと比べる
- 起動電圧（VIN 0.7V、150Ω）と停止電圧の確認

---

## 6A. Phase 2：USBドングル経由の無線化
|No|内容|結果／備考|
|---|---|---|
|P2-1|nRF Connect SDK（VS Code拡張）の導入|✅ v3.4.1（`C:\ncs\v3.4.1`、ツールチェーン `C:\ncs\toolchains\4f5b6ad6dd`）。初回はスリープでダウンロードが途中で止まったため、スリープを無効にして再インストール（2026-11-01）|
|P2-2|ドングルにDFUで書き込めることを確認|✅ 完了（2026-11-05）：ZephyrのblinkyでLED（P0.06）の点滅と、USBシリアル（COMポート）の認識を確認。手順と設定は§5-3、経緯は§6A.2|
|P2-3|ESBの疎通確認|✅ 完了（2026-11-07）：`trackball_fw` で確認|
|P2-4|ドングル：ESB受信データをUSB HIDマウスレポートに変換|✅ 完了（2026-11-07）：`dongle_rx` で、トラックボール操作によるPCカーソル移動を確認。USBはHIDマウス＋ログ用CDC ACMの複合デバイス|
|P2-5|マウス側：PMW3610のΔX/ΔYとボタンをESBで送信|✅ 完了（2026-11-07）：`mouse_tx`。**センサはZephyr標準ドライバではなくGPIOビットバンで読む**（標準ドライバは4線SPI前提のため。Phase 1で実績のある方式を移植）|
|P2-6|遅延と取りこぼしの確認、送信間隔とスリープの調整|消費電流の測定はPhase 3（電池駆動）|

### 6A.1 ビルド・書き込みの要点（NCS v3.4.1）
|対象|ボードターゲット|成果物（sysbuild）|書き込み方法|
|---|---|---|---|
|XIAO nRF52840|`xiao_ble/nrf52840`|`build\<app>\zephyr\zephyr.uf2`|リセット2回押し → 出てきたUSBドライブへコピー|
|ドングル MDBT50Q-CX|`raytac_mdbt50q_cx_40_dongle/nrf52840`|`build_dongle\<app>\zephyr\zephyr.hex`|ブートローダーモード → Programmerで「Write」（SoftDeviceは「Custom」→0x102）|

- sysbuildでは、`build\zephyr\` に入るのは全体の設定だけ。アプリ本体の成果物は `build\<アプリ名>\zephyr\` にできる
- 同じアプリに複数のボード用ビルド設定を並べられる（APPLICATIONSパネルの「+ Add build configuration」）
- 完全な再ビルドは、コマンドパレットの「nRF Connect: Pristine Build」

### 6A.2 ドングルのLEDが点滅しなかった件の切り分け（2026-11-01〜11-05）
|順|試したこと|結果|わかったこと|
|---|---|---|---|
|1|`nrf52840dongle/nrf52840` でビルドし、SoftDeviceを `0x00` 指定で書き込み|`SdVersionFailure`|出荷時にS113 v7.2.0（0x102）が入っている → アプリを0x1C000に置き、`0x102` を指定する|
|2|`prj.conf` で0x1C000配置、`0x102` で書き込み|書き込みは成功、LEDが光らない|—|
|3|ビルドログを確認|ボードが `raytac_mdbt50q_db_40`（デモボード用）になっていた|ボードターゲットの選び間違い|
|4|`raytac_mdbt50q_cx_40_dongle` ＋ P0.08へのoverlay|光らない、COMポートも出ない|—|
|5|起動直後にP0.08を点灯させる診断コードを追加|光らない|—|
|6|Zephyrを使わない最小Lチカ（P0.08のみ、0x1C000配置）|光らない|—|
|7|最小Lチカ（P0.06とP0.08を同時に点滅）|点滅した|**起動経路（ブートローダー → SoftDevice → アプリ）は正常。LEDはP0.06**|
|8|blinkyを元に戻す（overlayと診断コードを削除）|**LEDが点滅、COMポートも認識**|完了|

- 原因：LEDのピンの思い込み（回路図の画像を信じてP0.08に変更していた）と、ボードターゲットの取り違え
- 4・5でCOMポートが出なかった理由は未確定。ボード標準の設定に戻した8では、COMポートは正常に出ている

## 7. 参考資料
- [TPS61220/1/2 データシート（TI）](https://www.ti.com/lit/ds/symlink/tps61220.pdf)
- [TPS61220 製品ページ（TI）](https://www.ti.com/product/TPS61220)
- [PMW3610DM-SUDU データシート](https://www.epsglobal.com/Media-Library/EPSGlobal/Products/files/pixart/PMW3610DM-SUDU.pdf?ext=.pdf)
- [TLV700 データシート](https://download.mikroe.com/documents/datasheets/TLV700_datasheet.pdf)
- [snize/BOB-PMW3610-SEIBOKU](https://github.com/snize/BOB-PMW3610-SEIBOKU)
- [Seeed Studio XIAO nRF52840 Wiki](https://wiki.seeedstudio.com/XIAO_BLE/)
- [Raytac: MDBT50Q-CX（USB-C Dongle）ユーザーマニュアル](https://raytac.blog/2024/07/10/firmware-coding-dfu-onto-mdbt50q-rxuser-manual-of-mdbt50q-cx-nrf52840-usb-c-dongle/)
- [SGM2040 データシート（SG Micro）](https://www.sg-micro.com/rect/assets/9d7e04e6-7450-4c5c-b94c-fa500b90b628/SGM2040.pdf)
- [P25Q16H データシート（Seeed配布）](https://files.seeedstudio.com/wiki/github_weiruanexample/Flash_P25Q16H-UXH-IR_Datasheet.pdf)
- [nRF52840 Product Specification v1.5](https://files.seeedstudio.com/wiki/XIAO-BLE/nRF52840_PS_v1.5.pdf)
- [Seeed Forum: 3V3給電時にSPIが動かない件](https://forum.seeedstudio.com/t/xiao-nrf52840-project-works-when-powered-with-5v-not-with-battery-or-3-3v/293397)
- [badjeff/zmk-pmw3610-driver](https://github.com/badjeff/zmk-pmw3610-driver)
