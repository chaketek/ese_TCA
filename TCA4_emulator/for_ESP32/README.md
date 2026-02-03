# Haltech TCA4-A エミュレータ (ESP32版)

ESP32 DevKitC + 外付けCANトランシーバを使用した Haltech TCA4-A 熱電対アンプのエミュレータです。

## 概要

本プロジェクトは、MAX6675 熱電対センサーモジュールを使用して温度を計測し、ESP32内蔵のTWAIコントローラでHaltech TCA4-A 互換の CAN フォーマットで Haltech ECU に送信します。

### 重要な注意事項

> ⚠️ **本エミュレータは Haltech TCA4-A (4チャンネル) のプロトコルを模擬していますが、実際に温度データを送信しているのは CH1 と CH2 の 2チャンネルのみです。**
> 
> CH3、CH4 は常に「オープン回路 (未接続)」として送信されます。

---

## ハードウェア構成

### 必要な部品

| 部品 | 数量 | 備考 |
|------|------|------|
| ESP32 DevKitC | 1 | ESP32-WROOM-32搭載 |
| CANトランシーバモジュール | 1 | SN65HVD230, MCP2551, TJA1050等 |
| MAX6675 熱電対モジュール | 2 | K型熱電対付き |
| LED | 1 | CH2用 (CH1はESP32内蔵LED使用) |
| 抵抗 330Ω | 1 | LED用 |

### ピン割り当て

| 機能 | ESP32 GPIO | 備考 |
|------|------------|------|
| MAX6675 CH1 CS | GPIO 5 | チップセレクト |
| MAX6675 CH2 CS | GPIO 17 | チップセレクト |
| MAX6675 SCK | GPIO 18 | VSPI CLK |
| MAX6675 SO (MISO) | GPIO 19 | VSPI MISO |
| CAN TX | GPIO 21 | → トランシーバ TXD |
| CAN RX | GPIO 22 | → トランシーバ RXD |
| LED CH1 (状態表示) | GPIO 2 | ESP32内蔵LED |
| LED CH2 (状態表示) | GPIO 4 | 外付けLED |

### 配線図

```
ESP32 DevKitC                   MAX6675 CH1       MAX6675 CH2
=============                   ===========       ===========
  GPIO 5  ────────────────────── CS
  GPIO 17 ──────────────────────────────────────── CS
  GPIO 18 (SCK)  ─────────────── SCK ──────────── SCK
  GPIO 19 (MISO) ─────────────── SO  ──────────── SO
  3.3V ───────────────────────── VCC ──────────── VCC
  GND  ───────────────────────── GND ──────────── GND


ESP32 DevKitC                   CAN Transceiver (SN65HVD230)
=============                   ============================
  GPIO 21 (TX) ──────────────── TXD
  GPIO 22 (RX) ──────────────── RXD
  3.3V ────────────────────────  VCC
  GND  ────────────────────────  GND
                                 CANH ──────────── CAN Bus High
                                 CANL ──────────── CAN Bus Low


ESP32 DevKitC                   LED (CH2用)
=============                   ===========
  GPIO 4  ────────[330Ω]─────── Anode (+)
  GND  ───────────────────────── Cathode (-)
```

### CANトランシーバについて

ESP32には**TWAIコントローラ**が内蔵されていますが、物理層のトランシーバは含まれていません。
以下のいずれかのトランシーバモジュールが必要です：

| トランシーバ | 電圧 | 備考 |
|-------------|------|------|
| **SN65HVD230** | 3.3V | ✅ ESP32に最適、推奨 |
| MCP2551 | 5V | レベルシフタ必要 |
| TJA1050 | 5V | レベルシフタ必要 |

> ⚠️ **SN65HVD230を推奨します。** ESP32は3.3V動作のため、5Vトランシーバを使用する場合はレベルシフタが必要です。

---

## CAN通信仕様

Arduino UNO版と同一です。詳細は [README.md](../README.md) を参照してください。

| 項目 | 値 |
|------|-----|
| CAN速度 | 1 Mbps |
| CAN ID | 0x2CC (716 decimal) |
| データ長 | 8 バイト |
| 送信周期 | 50 ms (20 Hz) |

---

## LED状態表示

Arduino UNO版と同一の表示パターンです。

| 状態 | LED動作 | 視覚的印象 |
|------|---------|-----------|
| **正常動作** | 常時点灯 + 送信時に一瞬消灯 (10ms) | 安定した点灯、送信時に瞬く |
| **センサー異常** | 200ms ON / 200ms OFF | 速い点滅で警告 |
| **CANバスオフ** | 100ms ON / 100ms OFF (両LED同期) | 超高速点滅で重大エラー |

---

## Arduino IDE 設定

### ボード設定

1. **ファイル** → **環境設定** → **追加のボードマネージャURL** に以下を追加:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```

2. **ツール** → **ボードマネージャ** から「esp32」をインストール

3. **ツール** → **ボード** → **ESP32 Arduino** → **ESP32 Dev Module** を選択

### ボード設定詳細

| 設定項目 | 値 |
|---------|-----|
| Board | ESP32 Dev Module |
| Upload Speed | 921600 |
| CPU Frequency | 240MHz (WiFi/BT) |
| Flash Frequency | 80MHz |
| Flash Mode | QIO |
| Flash Size | 4MB (32Mb) |
| Partition Scheme | Default 4MB with spiffs |

---

## 必要なライブラリ

ESP32のTWAIドライバは**ESP32 Arduino Core**に含まれているため、追加のライブラリインストールは不要です。

```cpp
#include "driver/twai.h"  // ESP32内蔵TWAIドライバ
```

---

## Arduino UNO版との違い

| 項目 | Arduino UNO | ESP32 |
|------|------------|-------|
| CANコントローラ | MCP2515 (外付け) | TWAI (内蔵) |
| CANトランシーバ | CAN-Bus Shieldに搭載 | 外付け必要 |
| SPI | ハードウェアSPI | VSPI |
| 動作電圧 | 5V | 3.3V |
| LED (CH1) | D8 (外付け) | GPIO 2 (内蔵) |
| CPU速度 | 16MHz | 240MHz |

---

## トラブルシューティング

### CAN通信ができない

1. **トランシーバの配線確認**: TX/RXが正しく接続されているか
2. **終端抵抗**: CANバスの両端に120Ω終端抵抗があるか
3. **電源**: トランシーバに正しく電源が供給されているか
4. **GND共通**: ESP32とトランシーバのGNDが接続されているか

### MAX6675が読めない

1. **CS/SCK/MISOの配線確認**
2. **VCC電圧**: MAX6675は3.3V-5V対応、ESP32の3.3Vでも動作可

### LEDが正しく動作しない

1. **GPIO 2の確認**: 一部のボードでは起動時にLOWが必要
2. **外付けLEDの極性**: アノード(+)がGPIOに接続されているか

---

## 参考リンク

- [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32)
- [ESP32 TWAI (CAN) ドキュメント](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/twai.html)
- [SN65HVD230 データシート](https://www.ti.com/product/SN65HVD230)
- [Haltech Wideband Emulator (参考実装)](https://github.com/blacksheepinc/Haltech-wideband-emulator)
- [IObox Emulator Haltech (参考実装)](https://github.com/ptmotorsport/IObox-emulator-haltech)

---

## 更新履歴

| 日付 | 内容 |
|------|------|
| 2026/02/03 | ESP32版初版作成 |
