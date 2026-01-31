# MAX6675 + SparkFun CAN-Bus Shield ハードウェア変更概要

## 概要
MAX6675温度センサーをSparkFun CAN-Bus Shieldと併用するにあたり、ピン干渉を回避するための配線変更が必要です。

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
| D4, D5 | GPS (Software Serial) |
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

### 変更後のMAX6675接続

| MAX6675ピン | 変更前 | 変更後 | 備考 |
|-------------|--------|--------|------|
| **CS** | D10 | **D3** | 空きピンを使用 |
| **VCC** | D8 | **外部5V** | Arduino 5Vピン または 外部電源 |
| **GND** | D9 | **外部GND** | Arduino GND ピン |
| SCK | D13 | D13 | 変更なし (SPI共有) |
| SO (MISO) | D12 | D12 | 変更なし (SPI共有) |

---

## 配線図

```
Arduino/CAN-Bus Shield          MAX6675
========================        =======
     D3  ─────────────────────── CS
     5V  ─────────────────────── VCC
    GND  ─────────────────────── GND
    D13 (SCK)  ───────────────── SCK    (SPI共有)
    D12 (MISO) ───────────────── SO     (SPI共有)
```

---

## 注意事項

### 1. SPI共有について
- MAX6675とMCP2515(CAN)は同じSPIバスを共有します
- 各デバイスはCSピンで個別に制御されます
- ソフトウェアでSPI設定を切り替える処理が実装済みです

### 2. 電源について
- MAX6675の消費電流は最大1.5mA程度
- ArduinoのI/Oピンからの給電は不要になりました
- 外部5V電源を使用する場合はGNDを共通にしてください

### 3. D3ピンについて
- PWM対応ピンですが、本用途ではデジタル出力として使用
- 他のペリフェラル（LCD, GPS, ジョイスティック）と干渉しません

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
| データフォーマット | 温度×100 (int16) + 予約2バイト |

### 受信側でのデコード例
```cpp
int16_t tempInt = (data[0] << 8) | data[1];
float temperature = tempInt / 100.0;
```

---

## 更新履歴

| 日付 | 内容 |
|------|------|
| 2026/01/31 | 初版作成 - CAN-Bus Shield対応のためピン変更 |
