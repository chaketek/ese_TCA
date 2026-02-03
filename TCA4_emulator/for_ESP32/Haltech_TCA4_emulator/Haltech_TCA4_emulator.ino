/*
 * Haltech TCA4-A エミュレータ (MAX6675 2ch 温度計測 + CAN送信)
 * ESP32 DevKitC 版
 * 
 * ESP32内蔵TWAIコントローラ + 外付けCANトランシーバ(SN65HVD230等)を使用
 * 2つのMAX6675で計測した温度データをHaltech TCA4-A互換形式でCAN送信
 * 
 * 参考リポジトリ:
 *   https://github.com/blacksheepinc/Haltech-wideband-emulator
 *   https://github.com/ptmotorsport/IObox-emulator-haltech
 * 
 * ピン割り当て:
 *   MAX6675 CH1 CS : GPIO 5
 *   MAX6675 CH2 CS : GPIO 17
 *   MAX6675 SCK    : GPIO 18 (VSPI CLK)
 *   MAX6675 SO     : GPIO 19 (VSPI MISO)
 *   CAN TX         : GPIO 21
 *   CAN RX         : GPIO 22
 *   LED CH1        : GPIO 2  (内蔵LED)
 *   LED CH2        : GPIO 4
 */

#include <SPI.h>
#include "driver/twai.h"

// ============================================
// ピン定義
// ============================================
// MAX6675用ピン (VSPI使用)
#define MAX6675_CH1_CS    5     // MAX6675 CH1のチップセレクト
#define MAX6675_CH2_CS    17    // MAX6675 CH2のチップセレクト
#define MAX6675_SCK       18    // VSPI CLK
#define MAX6675_MISO      19    // VSPI MISO

// CANトランシーバ接続ピン (SN65HVD230等)
#define CAN_TX_PIN        21    // CAN TX → トランシーバのTXD
#define CAN_RX_PIN        22    // CAN RX → トランシーバのRXD

// LED状態表示ピン
#define LED_CH1_PIN       2     // ESP32内蔵LED - CH1状態表示
#define LED_CH2_PIN       4     // 外付けLED - CH2状態表示

// LED点滅設定
#define LED_SENSOR_ERROR_INTERVAL  200  // センサー異常時の点滅間隔 (ms) - 速い点滅
#define LED_BUSOFF_INTERVAL        100  // CANバスオフ時の点滅間隔 (ms) - 超高速点滅
#define LED_ACTIVITY_PULSE          10  // 正常時のアクティビティパルス (ms)

// ============================================
// CAN設定 (Haltech TCA4-A 互換)
// ============================================
#define CAN_ID_TCA4         0x2CC  // Haltech TCA4-A CAN ID (716 decimal)
#define CAN_SEND_INTERVAL   50     // CAN送信間隔 (ms) - 20Hz

// 診断コード
#define DIAG_NORMAL           0    // Normal operation
#define DIAG_SHORT_CIRCUIT    1    // Sensor Short Circuit
#define DIAG_OPEN_CIRCUIT     2    // Sensor Open Circuit
#define DIAG_UNDER_RANGE      3    // Under Range
#define DIAG_OVER_RANGE       4    // Over Range

// ============================================
// キャリブレーション設定
// ============================================
// 実測値が高い場合はマイナス、低い場合はプラスに設定
#define TEMP_OFFSET_CH1  0.0
#define TEMP_OFFSET_CH2  0.0

// ============================================
// グローバル変数
// ============================================
unsigned long lastSendTime = 0;
unsigned long lastReadTime = 0;
unsigned long lastSensorErrorBlinkTime = 0;  // センサー異常時LED点滅用タイマー
unsigned long lastBusOffBlinkTime = 0;       // CANバスオフ時LED点滅用タイマー
unsigned long lastActivityPulseTime = 0;     // アクティビティパルス用タイマー
float lastTemperature_CH1 = NAN;   // CH1の温度値をキャッシュ
float lastTemperature_CH2 = NAN;   // CH2の温度値をキャッシュ
bool canInitialized = false;
bool canBusOff = false;            // CANバスオフ状態
bool sensorErrorBlinkState = false;   // センサー異常時点滅状態
bool busOffBlinkState = false;        // CANバスオフ時点滅状態
bool activityPulseActive = false;     // アクティビティパルス中フラグ

// MAX6675の変換時間は最大220ms
// 余裕を持って250ms以上の間隔で読み取る
#define TEMP_READ_INTERVAL  250    // 温度読み取り間隔 (ms)

// SPIクラスインスタンス (VSPI使用)
SPIClass vspi(VSPI);

// ============================================
// セットアップ
// ============================================
void setup() {
  Serial.begin(115200);
  delay(1000);  // ESP32起動待ち
  
  Serial.println(F("Haltech TCA4-A Emulator (MAX6675 2ch) - ESP32"));
  Serial.println(F("============================================="));
  Serial.println(F("Ref: github.com/blacksheepinc/Haltech-wideband-emulator"));
  
  // MAX6675のCSピン初期化 (両チャンネル)
  pinMode(MAX6675_CH1_CS, OUTPUT);
  pinMode(MAX6675_CH2_CS, OUTPUT);
  digitalWrite(MAX6675_CH1_CS, HIGH);  // CH1 CSをHIGHに (非選択状態)
  digitalWrite(MAX6675_CH2_CS, HIGH);  // CH2 CSをHIGHに (非選択状態)
  
  // LED 初期化 (状態インジケータ)
  pinMode(LED_CH1_PIN, OUTPUT);
  pinMode(LED_CH2_PIN, OUTPUT);
  digitalWrite(LED_CH1_PIN, LOW);
  digitalWrite(LED_CH2_PIN, LOW);
  
  // SPI初期化 (VSPI)
  vspi.begin(MAX6675_SCK, MAX6675_MISO, -1, -1);  // SCK, MISO, MOSI(-1=未使用), SS(-1=未使用)
  
  // MAX6675の初期化完了を待つ（最大変換時間220ms）
  delay(250);
  
  // TWAI (CAN) 初期化 (1Mbps - Haltech標準)
  Serial.print(F("Initializing CAN (TWAI) at 1Mbps... "));
  if (initCAN()) {
    Serial.println(F("OK"));
    canInitialized = true;
  } else {
    Serial.println(F("FAILED"));
    Serial.println(F("Check CAN transceiver connection"));
    canInitialized = false;
  }
  
  Serial.println();
}

// ============================================
// TWAI (CAN) 初期化関数
// ============================================
bool initCAN() {
  // TWAI設定 (1Mbps)
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
    (gpio_num_t)CAN_TX_PIN, 
    (gpio_num_t)CAN_RX_PIN, 
    TWAI_MODE_NORMAL
  );
  
  // 1Mbps タイミング設定
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_1MBITS();
  
  // フィルタ設定 (すべて受信)
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  
  // TWAIドライバインストール
  if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
    return false;
  }
  
  // TWAIドライバ開始
  if (twai_start() != ESP_OK) {
    return false;
  }
  
  return true;
}

// ============================================
// MAX6675から温度を読み取る関数
// ============================================
float readTemperature(uint8_t csPin, float offset) {
  uint16_t value;
  
  // MAX6675用のSPI設定
  vspi.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  
  digitalWrite(csPin, LOW);
  delayMicroseconds(1);  // CSセットアップ時間
  value = vspi.transfer(0x00) << 8;  // Read high byte
  value |= vspi.transfer(0x00);      // Read low byte
  digitalWrite(csPin, HIGH);
  
  vspi.endTransaction();
  
  // ビット2: 熱電対オープン検出（1=オープン/未接続）
  if (value & 0x04) {
    return NAN;  // エラー時はNaNを返す
  }
  
  // ビット3-14が12ビットの温度データ
  // 分解能: 0.25°C (データシート確認済み)
  float temperature = (value >> 3) * 0.25;
  
  return temperature + offset;
}

// ============================================
// CANメッセージ送信関数 (Haltech TCA4-A互換フォーマット)
// ============================================
// データフォーマット (各チャンネル16ビット):
//   ビット0-3:  ダイアグコード (4ビット)
//   ビット4-15: 温度データ (12ビット)
// 変換式: 温度[°C] = raw * 0.4071 - 250
// 逆変換: raw = (温度[°C] + 250) * 5850 / 2381
// ============================================
void sendTemperatureCAN(float temp_ch1, float temp_ch2) {
  if (!canInitialized) {
    return;
  }
  
  twai_message_t message;
  
  message.identifier = CAN_ID_TCA4;    // CAN ID: 0x2CC
  message.extd = 0;                     // 標準ID (11bit)
  message.rtr = 0;                      // データフレーム
  message.data_length_code = 8;         // データ長: 8バイト (4チャンネル分)
  
  // Haltech TCA4-Aフォーマット:
  // - Big Endian (MS First)
  // - 各チャンネル16ビット: [Diag(4bit) | Temp(12bit)]
  // - 温度変換式: raw = (temp + 250) * 5850 / 2381
  
  // CH1 診断コードと温度raw値を計算
  uint8_t diag1 = DIAG_NORMAL;
  uint16_t raw1;
  if (isnan(temp_ch1)) {
    raw1 = 0xFFF;  // 12ビット最大値 (エラー)
    diag1 = DIAG_OPEN_CIRCUIT;
  } else {
    // 温度→raw値変換: raw = (temp + 250) * 5850 / 2381
    float rawFloat = (temp_ch1 + 250.0) * 5850.0 / 2381.0;
    raw1 = constrain((uint16_t)rawFloat, 0, 0xFFF);  // 12ビットに制限
  }
  
  // CH2 診断コードと温度raw値を計算
  uint8_t diag2 = DIAG_NORMAL;
  uint16_t raw2;
  if (isnan(temp_ch2)) {
    raw2 = 0xFFF;  // 12ビット最大値 (エラー)
    diag2 = DIAG_OPEN_CIRCUIT;
  } else {
    float rawFloat = (temp_ch2 + 250.0) * 5850.0 / 2381.0;
    raw2 = constrain((uint16_t)rawFloat, 0, 0xFFF);
  }
  
  // CH3, CH4 (未接続 - ダイアグ=オープン、温度=最大値)
  uint8_t diag3 = DIAG_OPEN_CIRCUIT;
  uint8_t diag4 = DIAG_OPEN_CIRCUIT;
  uint16_t raw3 = 0xFFF;
  uint16_t raw4 = 0xFFF;
  
  // Big Endian (MS First) でデータを格納
  // 各チャンネル: [上位バイト: Diag(4bit)|Temp上位4bit] [下位バイト: Temp下位8bit]
  
  // TC1: バイト0-1
  message.data[0] = ((diag1 & 0x0F) << 4) | ((raw1 >> 8) & 0x0F);
  message.data[1] = raw1 & 0xFF;
  
  // TC2: バイト2-3
  message.data[2] = ((diag2 & 0x0F) << 4) | ((raw2 >> 8) & 0x0F);
  message.data[3] = raw2 & 0xFF;
  
  // TC3: バイト4-5
  message.data[4] = ((diag3 & 0x0F) << 4) | ((raw3 >> 8) & 0x0F);
  message.data[5] = raw3 & 0xFF;
  
  // TC4: バイト6-7
  message.data[6] = ((diag4 & 0x0F) << 4) | ((raw4 >> 8) & 0x0F);
  message.data[7] = raw4 & 0xFF;
  
  // CAN送信 (タイムアウト: 10ms)
  twai_transmit(&message, pdMS_TO_TICKS(10));
}

// ============================================
// CANバスオフ検出関数
// ============================================
bool checkCanBusOff() {
  if (!canInitialized) {
    return true;  // CAN未初期化はバスオフ扱い
  }
  
  twai_status_info_t status;
  if (twai_get_status_info(&status) != ESP_OK) {
    return true;
  }
  
  // バスオフまたはエラー状態をチェック
  return (status.state == TWAI_STATE_BUS_OFF || 
          status.state == TWAI_STATE_RECOVERING ||
          status.tx_error_counter > 127 ||
          status.rx_error_counter > 127);
}

// ============================================
// LED状態更新関数
// ============================================
// 正常時: 常時点灯 + 送信時に10ms消灯 (アクティビティパルス)
// センサー異常時: 200ms間隔で速い点滅
// CANバスオフ時: 両LED同時に100ms間隔で超高速点滅
void updateLEDs(bool ch1_ok, bool ch2_ok, unsigned long currentTime) {
  // CANバスオフ状態をチェック
  canBusOff = checkCanBusOff();
  
  // CANバスオフ時の点滅タイミング更新
  if (currentTime - lastBusOffBlinkTime >= LED_BUSOFF_INTERVAL) {
    lastBusOffBlinkTime = currentTime;
    busOffBlinkState = !busOffBlinkState;
  }
  
  // センサー異常時の点滅タイミング更新
  if (currentTime - lastSensorErrorBlinkTime >= LED_SENSOR_ERROR_INTERVAL) {
    lastSensorErrorBlinkTime = currentTime;
    sensorErrorBlinkState = !sensorErrorBlinkState;
  }
  
  // アクティビティパルス終了チェック
  if (activityPulseActive && (currentTime - lastActivityPulseTime >= LED_ACTIVITY_PULSE)) {
    activityPulseActive = false;
  }
  
  // ========== 優先度1: CANバスオフ ==========
  if (canBusOff) {
    // 両LED同時に超高速点滅 (重大エラー)
    digitalWrite(LED_CH1_PIN, busOffBlinkState);
    digitalWrite(LED_CH2_PIN, busOffBlinkState);
    return;
  }
  
  // ========== 優先度2: センサー状態に応じた表示 ==========
  
  // CH1 LED制御
  if (ch1_ok) {
    // 正常時: 常時点灯、アクティビティパルス中は消灯
    digitalWrite(LED_CH1_PIN, activityPulseActive ? LOW : HIGH);
  } else {
    // センサー異常時: 200ms間隔で速い点滅
    digitalWrite(LED_CH1_PIN, sensorErrorBlinkState);
  }
  
  // CH2 LED制御
  if (ch2_ok) {
    // 正常時: 常時点灯、アクティビティパルス中は消灯
    digitalWrite(LED_CH2_PIN, activityPulseActive ? LOW : HIGH);
  } else {
    // センサー異常時: 200ms間隔で速い点滅
    digitalWrite(LED_CH2_PIN, sensorErrorBlinkState);
  }
}

// ============================================
// アクティビティパルス開始関数
// ============================================
void startActivityPulse(unsigned long currentTime) {
  activityPulseActive = true;
  lastActivityPulseTime = currentTime;
}

// ============================================
// メインループ
// ============================================
void loop() {
  unsigned long currentTime = millis();
  
  // 温度読み取り (250ms間隔 - MAX6675の変換時間220msを考慮)
  if (currentTime - lastReadTime >= TEMP_READ_INTERVAL) {
    lastReadTime = currentTime;
    lastTemperature_CH1 = readTemperature(MAX6675_CH1_CS, TEMP_OFFSET_CH1);
    lastTemperature_CH2 = readTemperature(MAX6675_CH2_CS, TEMP_OFFSET_CH2);
  }
  
  // CAN送信 (50ms間隔)
  if (currentTime - lastSendTime >= CAN_SEND_INTERVAL) {
    lastSendTime = currentTime;
    
    // シリアルモニタに出力
    Serial.print(F("CH1: "));
    if (isnan(lastTemperature_CH1)) {
      Serial.print(F("Error"));
    } else {
      Serial.print(lastTemperature_CH1, 2);
      Serial.print(F("C"));
    }
    
    Serial.print(F(" | CH2: "));
    if (isnan(lastTemperature_CH2)) {
      Serial.print(F("Error"));
    } else {
      Serial.print(lastTemperature_CH2, 2);
      Serial.print(F("C"));
    }
    
    // CAN送信
    if (canInitialized) {
      sendTemperatureCAN(lastTemperature_CH1, lastTemperature_CH2);
      
      // アクティビティパルス開始 (正常時に一瞬消灯)
      startActivityPulse(currentTime);
      
      Serial.print(F(" -> CAN 0x"));
      Serial.print(CAN_ID_TCA4, HEX);
      if (canBusOff) {
        Serial.println(F(" sent (BUS ERROR!)"));
      } else {
        Serial.println(F(" sent"));
      }
    } else {
      Serial.println(F(" (CAN disabled)"));
    }
  }
  
  // LED状態更新 (毎ループ実行 - センサー正常/異常/CANバスオフに応じて制御)
  if (canInitialized) {
    bool ch1_ok = !isnan(lastTemperature_CH1);
    bool ch2_ok = !isnan(lastTemperature_CH2);
    updateLEDs(ch1_ok, ch2_ok, currentTime);
  }
}
