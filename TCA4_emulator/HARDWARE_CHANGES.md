# Haltech TCA2 エミュレータ (MAX6675 2ch + SparkFun CAN-Bus Shield)

## 概要
Haltech TCA2 (熱電対温度計測コントローラ) のエミュレータです。
2つのMAX6675温度センサーをSparkFun CAN-Bus Shieldと併用し、Haltech ECU互換のCANフォーマットで温度データを送信します。

### 参考リポジトリ
- Haltech Wideband Emulator: https://github.com/blacksheepinc/Haltech-wideband-emulator

---

## ピン干渉の問題

### SparkFun CAN-Bus Shield が使用するピン
| ピン | 用途 |
|------|------|
| D10 | MCP2515 (CAN) チップセレクト |
| D9 | SDカード チップセレクト |
| D8 | LED2 |
| D7 | LED3 |
| D6 | LCD TX |
| D4, D5 | GPS (Software Serial) ※本プロジェクトではGPS未使用 |
| D11 | SPI MOSI |
| D12 | SPI MISO |
| D13 | SPI SCK |
| A1-A5 | ジョイスティック |

### 変更前のMAX6675接続 (干渉あり)
| MAX6675ピン | 接続先 | 問題 |
|-------------|--------|------|
| CS | D10 | ❌ CAN CSと競合 |
| VCC | D8 | ❌ CAN Shield LED2と競合 |
| GND | D9 | ❌ SD CSと競合 |
| SCK | D13 | ✅ SPI共有OK |
| SO | D12 | ✅ SPI共有OK |

---

## 必要なハードウェア変更

### 変更後のMAX6675接続 (2ch対応)

| MAX6675ピン | 変更前 | CH1 | CH2 | 備考 |
|-------------|--------|-----|-----|------|
| **CS** | D10 | **D3** | **D4** | 空きピンを使用 |
| **VCC** | D8 | **外部5V** | **外部5V** | Arduino 5Vピン または 外部電源 |
| **GND** | D9 | **外部GND** | **外部GND** | Arduino GND ピン |
| SCK | D13 | D13 | D13 | 変更なし (SPI共有) |
| SO (MISO) | D12 | D12 | D12 | 変更なし (SPI共有) |

---

## 配線図

```
Arduino/CAN-Bus Shield          MAX6675 CH1       MAX6675 CH2
========================        ===========       ===========
     D3  ─────────────────────── CS
     D4  ──────────────────────────────────────── CS
     5V  ─────────────────────── VCC ──────────── VCC
    GND  ─────────────────────── GND ──────────── GND
    D13 (SCK)  ───────────────── SCK ──────────── SCK    (SPI共有)
    D12 (MISO) ───────────────── SO  ──────────── SO     (SPI共有)
```

---

## 注意事項

### 1. SPI共有について
- MAX6675 CH1、CH2、MCP2515(CAN)は同じSPIバスを共有します
- 各デバイスはCSピンで個別に制御されます
- ソフトウェアでSPI設定を切り替える処理が実装済みです

### 2. 電源について
- MAX6675の消費電流は最大1.5mA程度 (2個で約3mA)
- ArduinoのI/Oピンからの給電は不要になりました
- 外部5V電源を使用する場合はGNDを共通にしてください

### 3. D3, D4ピンについて
- D3: PWM対応ピン、本用途ではデジタル出力として使用
- D4: CAN-Bus ShieldではGPS用だが、GPS未使用のため転用可能
- 他のペリフェラル（LCD, ジョイスティック）と干渉しません

### 4. GPS機能について
- D4をMAX6675 CH2に使用するため、CAN-Bus ShieldのGPS機能は使用不可
- GPSが必要な場合は別のピン割り当てを検討してください

---

## ライブラリのインストール

SparkFun CAN-Bus Shieldを使用するには、以下のライブラリが必要です：

1. **SparkFun CAN-Bus Arduino Library**
   - GitHub: https://github.com/sparkfun/SparkFun_CAN-Bus_Arduino_Library
   - Arduino Library Managerからインストール可能

---

## CAN通信仕様 (Haltech TCA2互換)

| 項目 | 値 |
|------|-----|
| CAN速度 | 1Mbps |
| CAN ID | 0x2CC (716) |
| データ長 | 8バイト |
| 送信周期 | 50ms (20Hz) |
| チャンネル数 | 4 (TC1-TC4) |

### データフォーマット (Big Endian / MS First)
各チャンネルは16ビットで構成され、上位4ビットがダイアグ、下位12ビットが温度データです。

| バイト | ビット | 内容 |
|--------|--------|------|
| data[0] | 7-4 | TC1 Diagnostic (4bit) |
| data[0] | 3-0 | TC1 Temperature 上位4bit |
| data[1] | 7-0 | TC1 Temperature 下位8bit |
| data[2] | 7-4 | TC2 Diagnostic (4bit) |
| data[2] | 3-0 | TC2 Temperature 上位4bit |
| data[3] | 7-0 | TC2 Temperature 下位8bit |
| data[4] | 7-4 | TC3 Diagnostic (4bit) |
| data[4] | 3-0 | TC3 Temperature 上位4bit |
| data[5] | 7-0 | TC3 Temperature 下位8bit |
| data[6] | 7-4 | TC4 Diagnostic (4bit) |
| data[6] | 3-0 | TC4 Temperature 上位4bit |
| data[7] | 7-0 | TC4 Temperature 下位8bit |

### 温度変換式
```
温度[°C] = raw × 0.4071 - 250
逆変換:  raw = (温度[°C] + 250) / 0.4071
```

| パラメータ | 値 | 備考 |
|------------|-----|------|
| Factor | 0.4071 | ≈ 2381/5850 |
| Offset | -250 | |
| 有効範囲 | -250°C ～ +1417°C | 12bit (0-4095) |

### 診断コード (Diagnostic Codes) - 4ビット
| 値 | 状態 |
|----|------|
| 0 | Normal operation (正常) |
| 1 | Sensor Short Circuit (短絡) |
| 2 | Sensor Open Circuit (オープン) |
| 3 | Under Range (下限エラー) |
| 4 | Over Range (上限エラー) |

### 受信側でのデコード例
```cpp
// TC1 (Big Endian, MS First)
uint8_t diag1 = (data[0] >> 4) & 0x0F;
uint16_t raw1 = ((data[0] & 0x0F) << 8) | data[1];
float temperature_ch1 = raw1 * 2381.0 / 5850.0 - 250.0;

// TC2
uint8_t diag2 = (data[2] >> 4) & 0x0F;
uint16_t raw2 = ((data[2] & 0x0F) << 8) | data[3];
float temperature_ch2 = raw2 * 2381.0 / 5850.0 - 250.0;

// 診断コードチェック
if (diag1 == 2) {
  // CH1 センサーオープン
}
```

---

## 更新履歴

| 日付 | 内容 |
|------|------|
| 2026/01/31 | 初版作成 - CAN-Bus Shield対応のためピン変更 |
| 2026/02/01 | CH2追加 - D4ピンを使用した2ch対応 |
| 2026/02/01 | Haltech TCA2エミュレータ対応 - CAN ID 0x716, 1Mbps, Big Endian形式 |
