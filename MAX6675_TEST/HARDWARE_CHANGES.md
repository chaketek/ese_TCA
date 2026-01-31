# MAX6675 2ch + SparkFun CAN-Bus Shield ハードウェア変更概要

## 概要
2つのMAX6675温度センサーをSparkFun CAN-Bus Shieldと併用するにあたり、ピン干渉を回避するための配線変更が必要です。

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

## CAN通信仕様

| 項目 | 値 |
|------|-----|
| CAN速度 | 500kbps |
| 温度データのCAN ID | 0x100 |
| データ長 | 4バイト |
| 送信周期 | 100ms |

### データフォーマット (Little Endian)
| バイト | 内容 |
|--------|------|
| data[0] | Temperature_CH1 Low byte |
| data[1] | Temperature_CH1 High byte |
| data[2] | Temperature_CH2 Low byte |
| data[3] | Temperature_CH2 High byte |

※温度値は100倍した整数値 (例: 25.50°C → 2550)
※エラー時は 0x7FFF を送信

### 受信側でのデコード例
```cpp
// CH1
int16_t temp1Int = data[0] | (data[1] << 8);
float temperature_ch1 = temp1Int / 100.0;

// CH2
int16_t temp2Int = data[2] | (data[3] << 8);
float temperature_ch2 = temp2Int / 100.0;

// エラーチェック
if (temp1Int == 0x7FFF) {
  // CH1 エラー（熱電対オープン）
}
```

---

## 更新履歴

| 日付 | 内容 |
|------|------|
| 2026/01/31 | 初版作成 - CAN-Bus Shield対応のためピン変更 |
| 2026/02/01 | CH2追加 - D4ピンを使用した2ch対応 |
