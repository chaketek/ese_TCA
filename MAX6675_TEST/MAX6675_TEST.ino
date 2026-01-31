/*
 * MAX6675 2ch 温度計測 + CAN送信
 * 
 * SparkFun CAN-Bus Shield と組み合わせて使用
 * 2つのMAX6675で計測した温度データをCANメッセージで送信
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

// ============================================
// CAN設定
// ============================================
#define CAN_ID_TEMPERATURE  0x100  // 温度データのCAN ID
#define CAN_SEND_INTERVAL   100    // CAN送信間隔 (ms)

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
float lastTemperature_CH1 = NAN;   // CH1の温度値をキャッシュ
float lastTemperature_CH2 = NAN;   // CH2の温度値をキャッシュ
bool canInitialized = false;

// MAX6675の変換時間は最大220ms
// 余裕を持って250ms以上の間隔で読み取る
#define TEMP_READ_INTERVAL  250    // 温度読み取り間隔 (ms)

// ============================================
// セットアップ
// ============================================
void setup() {
  Serial.begin(9600);
  Serial.println(F("MAX6675 2ch + CAN Transmitter"));
  Serial.println(F("============================="));
  
  // MAX6675のCSピン初期化 (両チャンネル)
  pinMode(MAX6675_CH1_CS, OUTPUT);
  pinMode(MAX6675_CH2_CS, OUTPUT);
  digitalWrite(MAX6675_CH1_CS, HIGH);  // CH1 CSをHIGHに (非選択状態)
  digitalWrite(MAX6675_CH2_CS, HIGH);  // CH2 CSをHIGHに (非選択状態)
  
  // SPI初期化
  SPI.begin();
  
  // MAX6675の初期化完了を待つ（最大変換時間220ms）
  delay(250);
  
  // CAN-Bus初期化 (500kbps)
  Serial.print(F("Initializing CAN-Bus... "));
  if (Canbus.init(CANSPEED_500)) {
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
// CANメッセージ送信関数 (2ch対応)
// ============================================
void sendTemperatureCAN(float temp_ch1, float temp_ch2) {
  if (!canInitialized) {
    return;
  }
  
  tCAN message;
  
  message.id = CAN_ID_TEMPERATURE;  // CAN ID
  message.header.rtr = 0;           // データフレーム
  message.header.length = 4;        // データ長: 4バイト (2ch x 2bytes)
  
  // float を バイト配列に変換
  // 方法: 温度を100倍して整数化 (小数点以下2桁の精度を保持)
  // Little Endian (Intel形式) で送信 - DBCファイルの定義に合わせる
  
  // CH1: data[0-1]
  int16_t temp1Int = isnan(temp_ch1) ? (int16_t)0x7FFF : (int16_t)(temp_ch1 * 100);
  message.data[0] = temp1Int & 0xFF;         // CH1 Low byte
  message.data[1] = (temp1Int >> 8) & 0xFF;  // CH1 High byte
  
  // CH2: data[2-3]
  int16_t temp2Int = isnan(temp_ch2) ? (int16_t)0x7FFF : (int16_t)(temp_ch2 * 100);
  message.data[2] = temp2Int & 0xFF;         // CH2 Low byte
  message.data[3] = (temp2Int >> 8) & 0xFF;  // CH2 High byte
  
  // CAN送信
  mcp2515_bit_modify(CANCTRL, (1 << REQOP2) | (1 << REQOP1) | (1 << REQOP0), 0);
  mcp2515_send_message(&message);
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
      Serial.print(F(" -> CAN 0x"));
      Serial.print(CAN_ID_TEMPERATURE, HEX);
      Serial.println(F(" sent"));
    } else {
      Serial.println(F(" (CAN disabled)"));
    }
  }
}
