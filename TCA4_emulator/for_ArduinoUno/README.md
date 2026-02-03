# Haltech TCA4-A エミュレータ (Arduino UNO版)

Arduino UNO + SparkFun CAN-Bus Shield を使用した Haltech TCA4-A 熱電対アンプのエミュレータです。

## 概要

本プロジェクトは、MAX6675 熱電対センサーモジュールを使用して温度を計測し、SparkFun CAN-Bus Shield（MCP2515）でHaltech TCA4-A 互換の CAN フォーマットで Haltech ECU に送信します。

### 重要な注意事項

> ⚠️ **本エミュレータは Haltech TCA4-A (4チャンネル) のプロトコルを模擬していますが、実際に温度データを送信しているのは CH1 と CH2 の 2チャンネルのみです。**
> 
> CH3、CH4 は常に「オープン回路 (未接続)」として送信されます。

---

## ハードウェア構成

### 必要な部品

| 部品 | 数量 | 備考 |
|------|------|------|
| Arduino UNO (または互換機) | 1 | |
| SparkFun CAN-Bus Shield | 1 | DEV-13262 |
| MAX6675 熱電対モジュール | 2 | K型熱電対付き |

### ピン割り当て

| 機能 | Arduinoピン | 備考 |
|------|-------------|------|
| MAX6675 CH1 CS | D3 | チップセレクト |
| MAX6675 CH2 CS | D4 | チップセレクト |
| MAX6675 SCK | D13 | SPI共有 |
| MAX6675 SO (MISO) | D12 | SPI共有 |
| MAX6675 VCC | 外部5V | Arduino 5Vピンまたは外部電源 |
| MAX6675 GND | GND | 共通GND |
| LED CH1 (状態表示) | D8 | CAN-Bus Shield LED2 |
| LED CH2 (状態表示) | D7 | CAN-Bus Shield LED3 |
| CAN CS | D10 | CAN-Bus Shield標準 |

### 配線図

```
Arduino/CAN-Bus Shield          MAX6675 CH1       MAX6675 CH2
========================        ===========       ===========
     D3  ─────────────────────── CS
     D4  ──────────────────────────────────────── CS
     5V  ─────────────────────── VCC ──────────── VCC
    GND  ─────────────────────── GND ──────────── GND
    D13 (SCK)  ───────────────── SCK ──────────── SCK
    D12 (MISO) ───────────────── SO  ──────────── SO
```

---

## CAN通信仕様

共通仕様です。詳細は [README.md](../README.md) を参照してください。

| 項目 | 値 |
|------|-----|
| CAN速度 | 1 Mbps |
| CAN ID | 0x2CC (716 decimal) |
| データ長 | 8 バイト |
| 送信周期 | 50 ms (20 Hz) |

---

## LED状態表示

CAN-Bus Shield の LED2 (D8) と LED3 (D7) を使用して、各チャンネルの状態を表示します。

| 状態 | LED動作 | 視覚的印象 |
|------|---------|-----------|
| **正常動作** | 常時点灯 + 送信時に一瞬消灯 (10ms) | 安定した点灯、送信時に瞬く |
| **センサー異常** | 200ms ON / 200ms OFF | 速い点滅で警告 |
| **CANバスオフ** | 100ms ON / 100ms OFF (両LED同期) | 超高速点滅で重大エラー |

---

## 必要なライブラリ

### SparkFun CAN-Bus Arduino Library

- GitHub: https://github.com/sparkfun/SparkFun_CAN-Bus_Arduino_Library
- Arduino Library Manager からインストール可能

### 1Mbps対応について

SparkFun ライブラリは標準で 500kbps までしかサポートしていません。
本プロジェクトでは以下の定義を追加して 1Mbps に対応しています：

```cpp
#define CANSPEED_1000  0  // 16MHz crystal, CNF1=0
```

---

## シリアルモニタ出力

ボーレート: **115200 bps**

```
Haltech TCA2 Emulator (MAX6675 2ch)
====================================
Ref: github.com/blacksheepinc/Haltech-wideband-emulator
Initializing CAN-Bus (1Mbps)... OK

CH1: 25.50C | CH2: 26.25C -> CAN 0x2CC sent
CH1: 25.50C | CH2: 26.25C -> CAN 0x2CC sent
CH1: Error | CH2: 26.25C -> CAN 0x2CC sent
```

---

## ESP32版との違い

| 項目 | Arduino UNO | ESP32 |
|------|------------|-------|
| CANコントローラ | MCP2515 (外付け) | TWAI (内蔵) |
| CANトランシーバ | CAN-Bus Shieldに搭載 | 外付け必要 |
| SPI | ハードウェアSPI | VSPI |
| 動作電圧 | 5V | 3.3V |
| CPU速度 | 16MHz | 240MHz |

---

## 参考リンク

- [SparkFun CAN-Bus Shield](https://www.sparkfun.com/products/13262)
- [SparkFun CAN-Bus Arduino Library](https://github.com/sparkfun/SparkFun_CAN-Bus_Arduino_Library)
- [MAX6675 データシート](https://www.maximintegrated.com/en/products/sensors/MAX6675.html)
- [Haltech Wideband Emulator (参考実装)](https://github.com/blacksheepinc/Haltech-wideband-emulator)

---

## 更新履歴

| 日付 | 内容 |
|------|------|
| 2026/02/02 | 初版リリース |
| 2026/02/02 | LED状態表示機能追加 |
| 2026/02/02 | CANバスオフ検出機能追加 |
| 2026/02/03 | for_ArduinoUnoフォルダに移動 |
