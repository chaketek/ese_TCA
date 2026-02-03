# Haltech TCA4-A エミュレータ

MAX6675 熱電対センサーを使用した Haltech TCA4-A 熱電対アンプのエミュレータです。

## 対応プラットフォーム

| プラットフォーム | フォルダ | CANコントローラ |
|-----------------|---------|----------------|
| **Arduino UNO** | [for_ArduinoUno/](for_ArduinoUno/) | MCP2515 (SparkFun CAN-Bus Shield) |
| **ESP32 DevKitC** | [for_ESP32/](for_ESP32/) | 内蔵TWAI + 外付けトランシーバ |

詳細なハードウェア構成・配線については各フォルダのREADMEを参照してください。

## 概要

本プロジェクトは、MAX6675 熱電対センサーモジュールを使用して温度を計測し、Haltech TCA4-A 互換の CAN フォーマットで Haltech ECU に送信します。

### 重要な注意事項

> ⚠️ **本エミュレータは Haltech TCA4-A (4チャンネル) のプロトコルを模擬していますが、実際に温度データを送信しているのは CH1 と CH2 の 2チャンネルのみです。**
> 
> CH3、CH4 は常に「オープン回路 (未接続)」として送信されます。

---

## ハードウェア構成

プラットフォーム別のハードウェア構成は各フォルダのREADMEを参照してください：

- **Arduino UNO版**: [for_ArduinoUno/README.md](for_ArduinoUno/README.md)
- **ESP32版**: [for_ESP32/README.md](for_ESP32/README.md)

### 共通部品

| 部品 | 数量 | 備考 |
|------|------|------|
| MAX6675 熱電対モジュール | 2 | K型熱電対付き |

---

## プラットフォーム別ドキュメント

- **Arduino UNO版**: [for_ArduinoUno/README.md](for_ArduinoUno/README.md) - ピン配置、配線図
- **ESP32版**: [for_ESP32/README.md](for_ESP32/README.md) - ピン配置、配線図、Arduino IDE設定

---

## CAN通信仕様 (共通)

### 基本仕様

| 項目 | 値 |
|------|-----|
| CAN速度 | 1 Mbps |
| CAN ID | 0x2CC (716 decimal) |
| データ長 | 8 バイト |
| 送信周期 | 50 ms (20 Hz) |
| 対応チャンネル | 4 (TC1-TC4) ※実データはTC1, TC2のみ |

### データフォーマット

各チャンネルは 16 ビットで構成され、Big Endian (MS First) 形式です。

```
[上位4ビット: 診断コード] [下位12ビット: 温度データ]
```

| バイト | 内容 |
|--------|------|
| 0-1 | TC1: [Diag(4bit)\|Temp(12bit)] |
| 2-3 | TC2: [Diag(4bit)\|Temp(12bit)] |
| 4-5 | TC3: [Diag(4bit)\|Temp(12bit)] ※常にオープン |
| 6-7 | TC4: [Diag(4bit)\|Temp(12bit)] ※常にオープン |

### 温度変換式

```
温度[°C] = raw × 0.4071 - 250

逆変換: raw = (温度[°C] + 250) / 0.4071
       raw = (温度[°C] + 250) × 5850 / 2381
```

| パラメータ | 値 |
|------------|-----|
| Factor | 0.4071 (≈ 2381/5850) |
| Offset | -250 |
| 有効範囲 | -250°C ～ +1417°C |

### 診断コード

| 値 | 状態 | 説明 |
|----|------|------|
| 0 | Normal | 正常動作 |
| 1 | Short Circuit | センサー短絡 |
| 2 | Open Circuit | センサーオープン (未接続) |
| 3 | Under Range | 下限エラー |
| 4 | Over Range | 上限エラー |

---

## LED状態表示 (共通)

LED2 と LED3 (またはESP32版ではGPIO 2/4) を使用して、各チャンネルの状態を表示します。

### LED表示パターン

| 状態 | LED動作 | 視覚的印象 |
|------|---------|-----------|
| **正常動作** | 常時点灯 + 送信時に一瞬消灯 (10ms) | 安定した点灯、送信時に瞬く |
| **センサー異常** | 200ms ON / 200ms OFF | 速い点滅で警告 |
| **CANバスオフ** | 100ms ON / 100ms OFF (両LED同期) | 超高速点滅で重大エラー |

### 状態の優先順位

1. **CANバスオフ** (最優先) → 両LED同期で超高速点滅
2. **センサー異常** → 該当チャンネルのみ速い点滅
3. **正常** → 常時点灯 + アクティビティパルス

---

## キャリブレーション

温度オフセットを調整する場合は、以下の定数を変更してください：

```cpp
#define TEMP_OFFSET_CH1  0.0  // CH1のオフセット [°C]
#define TEMP_OFFSET_CH2  0.0  // CH2のオフセット [°C]
```

- 実測値が高い場合: マイナス値を設定
- 実測値が低い場合: プラス値を設定

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

- `Error`: センサーオープン (未接続または断線)
- `BUS ERROR!`: CANバスエラー検出時

---

## 必要なライブラリ

### Arduino UNO版
- **SparkFun CAN-Bus Arduino Library**
  - GitHub: https://github.com/sparkfun/SparkFun_CAN-Bus_Arduino_Library
  - Arduino Library Manager からインストール可能

### ESP32版
- **ESP32 Arduino Core** (追加ライブラリ不要)
  - TWAI ドライバは Core に含まれています

### Arduino UNO版 1Mbps対応について

SparkFun ライブラリは標準で 500kbps までしかサポートしていません。
本プロジェクトでは以下の定義を追加して 1Mbps に対応しています：

```cpp
#define CANSPEED_1000  0  // 16MHz crystal, CNF1=0
```

---

## DBCファイル

CAN解析ツール用の DBC ファイルが各フォルダに同梱されています：

- `for_ArduinoUno/Haltech_TCA4_emulator.dbc`
- `for_ESP32/Haltech_TCA4_emulator.dbc`

TSMaster、CANalyzer、BUSMASTER 等で使用できます。

---

## 参考リンク

- [Haltech TCA4-A 製品ページ](https://www.haltech.com/)
- [Haltech Wideband Emulator (参考実装)](https://github.com/blacksheepinc/Haltech-wideband-emulator)
- [IObox Emulator Haltech (参考実装)](https://github.com/ptmotorsport/IObox-emulator-haltech)
- [SparkFun CAN-Bus Shield](https://www.sparkfun.com/products/13262)
- [MAX6675 データシート](https://www.maximintegrated.com/en/products/sensors/MAX6675.html)

---

## ライセンス

MIT License

---

## 更新履歴

| 日付 | 内容 |
|------|------|
| 2026/02/02 | 初版リリース - Haltech TCA4-A エミュレータ完成 |
| 2026/02/02 | LED状態表示機能追加 (正常/センサー異常/CANバスオフ) |
| 2026/02/02 | CANバスオフ検出機能追加 |
| 2026/02/03 | **ESP32版追加** - TWAI内蔵コントローラ対応 |
| 2026/02/03 | フォルダ構成変更 (for_ArduinoUno, for_ESP32) |
