# ese_TCA - Haltech TCA4-A エミュレータプロジェクト

MAX6675 熱電対センサーを使用した Haltech TCA4-A 熱電対アンプのエミュレータ開発プロジェクトです。

## プロジェクト構成

```
ese_TCA/
├── TCA4_emulator/          # メインプロジェクト
│   ├── for_ArduinoUno/     # Arduino UNO + SparkFun CAN-Bus Shield版
│   ├── for_ESP32/          # ESP32 DevKitC + 外付けCANトランシーバ版
│   └── README.md           # 詳細ドキュメント
└── TSmasterPrj/            # TSMaster CANデバッグプロジェクト
```

## 概要

本プロジェクトは、MAX6675 K型熱電対モジュールで計測した温度データを、Haltech TCA4-A 互換の CAN フォーマットで送信するエミュレータです。

### 主な特徴

- **2プラットフォーム対応**: Arduino UNO / ESP32 DevKitC
- **Haltech互換**: TCA4-A プロトコルに準拠
- **CAN 1Mbps**: Haltech標準のCAN速度に対応
- **LED状態表示**: 正常/センサー異常/CANバスオフを視覚的に表示

## CAN通信仕様

| 項目 | 値 |
|------|-----|
| CAN速度 | 1 Mbps |
| CAN ID | 0x2CC (716 decimal) |
| データ長 | 8 バイト |
| 送信周期 | 50 ms (20 Hz) |

## クイックスタート

1. 使用するプラットフォームのフォルダを選択
   - Arduino UNO → `TCA4_emulator/for_ArduinoUno/`
   - ESP32 → `TCA4_emulator/for_ESP32/`

2. 各フォルダの README.md を参照してハードウェアを準備

3. `.ino` ファイルを Arduino IDE で開いてアップロード

## 参考リンク

- [Haltech Wideband Emulator](https://github.com/blacksheepinc/Haltech-wideband-emulator)
- [IObox Emulator Haltech](https://github.com/ptmotorsport/IObox-emulator-haltech)

## ライセンス

MIT License
