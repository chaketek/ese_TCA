/*
 * Haltech TCA2 エミュレータ (MAX6675 2ch 温度計測 + CAN送信)
 * 
 * SparkFun CAN-Bus Shield と組み合わせて使用
 * 2つのMAX6675で計測した温度データをHaltech TCA2互換形式でCAN送信
 * 
 * 参考リポジトリ:
 *   https://github.com/blacksheepinc/Haltech-wideband-emulator
 * 
 * ピン割り当て (CAN-Bus Shieldとの干渉を回避):
 *   MAX6675 CH1 CS : D3
 *   MAX6675 CH2 CS : D4
 *   MAX6675 VCC    : 外部5V供給 (D8はCAN ShieldのLED2で使用)
 *   MAX6675 GND    : 外部GND接続 (D9はCAN ShieldのSD CSで使用)
 *   CAN CS         : D10 (CAN-Bus Shield標準)
 *   SD CS          : D9  (CAN-Bus Shield標準)
 *   SPI            : D11(MOSI), D12(MISO), D13(SCK) - 共有
 */

#include <SPI.h>
#include <Canbus.h>
#include <mcp2515.h>

// ============================================
// ピン定義
// ============================================
// MAX6675用ピン (CAN-Bus Shieldと干渉しないピンに変更)
#define MAX6675_CH1_CS    3    // MAX6675 CH1のチップセレクト
#define MAX6675_CH2_CS    4    // MAX6675 CH2のチップセレクト

// CAN-Bus Shield標準ピン (参考)
// #define CAN_CS     10  // MCP2515のCS (ライブラリ内部で使用)
// #define SD_CS       9  // SDカードのCS
#define LED_CH1_PIN       8    // CAN-Bus Shield LED2 - CH1状態表示
#define LED_CH2_PIN       7    // CAN-Bus Shield LED3 - CH2状態表示

// LED点滅設定
#define LED_SENSOR_ERROR_INTERVAL  200  // センサー異常時の点滅間隔 (ms) - 速い点滅
#define LED_BUSOFF_INTERVAL        100  // CANバスオフ時の点滅間隔 (ms) - 超高速点滅
#define LED_ACTIVITY_PULSE          10  // 正常時のアクティビティパルス (ms)

// ============================================
// CAN設定 (Haltech TCA2 互換)
// ============================================
#define CAN_ID_TCA2         0x2CC  // Haltech TCA2 CAN ID (716 decimal)
#define CAN_SEND_INTERVAL   50     // CAN送信間隔 (ms) - 20Hz

// 1Mbps用のCAN速度定義 (16MHz crystal, CNF1=0)
// SparkFunライブラリはCANSPEED_500(=1)までしか定義していないため追加
#define CANSPEED_1000       0      // CAN speed at 1000 kbps (1Mbps)

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

// ============================================
// セットアップ
// ============================================
void setup() {
  Serial.begin(115200);  // Haltechエミュレータに合わせて115200bps
  Serial.println(F("Haltech TCA2 Emulator (MAX6675 2ch)"));
  Serial.println(F("===================================="));
  Serial.println(F("Ref: github.com/blacksheepinc/Haltech-wideband-emulator"));
  
  // MAX6675のCSピン初期化 (両チャンネル)
  pinMode(MAX6675_CH1_CS, OUTPUT);
  pinMode(MAX6675_CH2_CS, OUTPUT);
  digitalWrite(MAX6675_CH1_CS, HIGH);  // CH1 CSをHIGHに (非選択状態)
  digitalWrite(MAX6675_CH2_CS, HIGH);  // CH2 CSをHIGHに (非選択状態)
  
  // CAN-Bus Shield LED 初期化 (状態インジケータ)
  pinMode(LED_CH1_PIN, OUTPUT);
  pinMode(LED_CH2_PIN, OUTPUT);
  digitalWrite(LED_CH1_PIN, LOW);
  digitalWrite(LED_CH2_PIN, LOW);
  
  // SPI初期化
  SPI.begin();
  
  // MAX6675の初期化完了を待つ（最大変換時間220ms）
  delay(250);
  
  // CAN-Bus初期化 (1Mbps - Haltech標準)
  Serial.print(F("Initializing CAN-Bus (1Mbps)... "));
  if (Canbus.init(CANSPEED_1000)) {  // 1Mbps (CNF1=0 for 16MHz crystal)
    Serial.println(F("OK"));
    canInitialized = true;
  } else {
    Serial.println(F("FAILED"));
    Serial.println(F("Check CAN-Bus Shield connection"));
    canInitialized = false;
  }
  
  Serial.println();
}

// ============================================
// MAX6675から温度を読み取る関数
// ============================================
float readTemperature(uint8_t csPin, float offset) {
  uint16_t value;
  
  // MAX6675用のSPI設定
  SPI.setBitOrder(MSBFIRST);
  SPI.setClockDivider(SPI_CLOCK_DIV4);
  SPI.setDataMode(SPI_MODE0);
  
  digitalWrite(csPin, LOW);
  delayMicroseconds(1);  // CSセットアップ時間
  value = SPI.transfer(0x00) << 8;  // Read high byte
  value |= SPI.transfer(0x00);      // Read low byte
  digitalWrite(csPin, HIGH);
  
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
// CANメッセージ送信関数 (Haltech TCA2互換フォーマット)
// ============================================
// データフォーマット (各チャンネル16ビット):
//   ビット0-3:  ダイアグコード (4ビット)
//   ビット4-15: 温度データ (12ビット)
// 変換式: 温度[°C] = raw * 2381 / 5850 - 250
// 逆変換: raw = (温度[°C] + 250) * 5850 / 2381
// ============================================
void sendTemperatureCAN(float temp_ch1, float temp_ch2) {
  if (!canInitialized) {
    return;
  }
  
  tCAN message;
  
  message.id = CAN_ID_TCA2;         // CAN ID: 0x716
  message.header.rtr = 0;           // データフレーム
  message.header.length = 8;        // データ長: 8バイト (4チャンネル分)
  
  // Haltech TCA2フォーマット:
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
  
  // CAN送信
  mcp2515_bit_modify(CANCTRL, (1 << REQOP2) | (1 << REQOP1) | (1 << REQOP0), 0);
  mcp2515_send_message(&message);
}

// ============================================
// CANバスオフ検出関数
// ============================================
// MCP2515のEFLGレジスタを読み取り、バスオフ状態を検出
bool checkCanBusOff() {
  if (!canInitialized) {
    return true;  // CAN未初期化はバスオフ扱い
  }
  
  // EFLGレジスタ (0x2D) を読み取り
  uint8_t eflg = mcp2515_read_register(EFLG);
  
  // TXBO (bit 5): 送信バスオフ状態
  // TXEP (bit 4): 送信エラーパッシブ
  // RXEP (bit 3): 受信エラーパッシブ
  // いずれかがセットされていればエラー状態
  return (eflg & ((1 << 5) | (1 << 4) | (1 << 3))) != 0;
}

// ============================================
// LED状態更新関数
// ============================================
// 正常時: 常時点灯 + 送信時に50ms消灯 (アクティビティパルス)
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
  
  // CAN送信 (100ms間隔)
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
      Serial.print(CAN_ID_TCA2, HEX);
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
