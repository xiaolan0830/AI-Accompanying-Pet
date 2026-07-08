#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <driver/i2s.h>
#include <string.h>
#include <stdlib.h>

bool voiceSessionActive = false;
//==========================================
// 1. TFT 屏幕引脚
//==========================================
#define TFT_MOSI 13
#define TFT_SCLK 12
#define TFT_CS   16
#define TFT_DC   14
#define TFT_RST  15

Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);

//==========================================
// 2. WiFi 配置
//==========================================
const char* ssid = "Autumn_t";
const char* password = "770101tt";
IPAddress macIP(192, 168, 43, 133);
const int udpPort = 8888;
WiFiUDP udp;

//==========================================
// 3. 音频参数
//==========================================
#define SAMPLE_RATE     16000
#define MAX_TTS_SECONDS 50
#define MAX_PCM_BYTES   (SAMPLE_RATE * 2 * MAX_TTS_SECONDS)

//==========================================
// 4. 麦克风 I2S (INMP441)
//==========================================
#define MIC_WS   4
#define MIC_SD   6
#define MIC_SCK  5

int32_t i2sMicBuffer[256];
int16_t pcmMicBuffer[256];

//==========================================
// 5. 扬声器 I2S (MAX98357A)
//==========================================
#define SPK_BCK 10
#define SPK_WS  11
#define SPK_DIN 9

#define UDP_READ_BYTES 1024
uint8_t udpBuf[UDP_READ_BYTES];

uint8_t* ttsBuffer = nullptr;
size_t ttsBufferLen = 0;
bool receivingPCM = false;

//==========================================
// 6. STM32 通讯 (UART2)
//==========================================
#define RXD2 18
#define TXD2 17

String cmdBuffer = "";
bool cmdComplete = false;

//==========================================
// 7. 状态机定义
//==========================================
enum State {
  STATE_SLEEP,   // 初始休眠，显示 sleep 图
  STATE_HAPPY,   // 唤醒后，显示 happy 图
  STATE_SPEAK    // 喇叭播放中，显示 speak 动画
};

State currentState = STATE_SLEEP;
State previousState = STATE_SLEEP;  // 用于追踪状态变化

//==========================================
// 8. 动画状态
//==========================================
bool showOpen = false;
unsigned long lastSwitch = 0;
int animSpeed = 300;

//==========================================
// 9. 图片数据（占位，你需要替换为实际数据）
//==========================================

// --- SLEEP 图 (55行，不区分张嘴闭嘴) ---
const char sleep_0[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_1[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_2[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_3[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_4[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_5[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_6[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_7[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_8[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_9[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_10[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_11[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_12[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_13[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_14[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_15[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_16[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_17[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_18[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_19[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_20[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_21[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_22[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_23[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_24[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_25[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_26[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_27[] PROGMEM = "000000000000000000000000000000000000111000000000000000000000";
const char sleep_28[] PROGMEM = "000000000000000000000000000000000001111100000000000000000000";
const char sleep_29[] PROGMEM = "000000000000000000000000000000000001111100000000000000000000";
const char sleep_30[] PROGMEM = "000000000000000000000000000000000001111100000000000000000000";
const char sleep_31[] PROGMEM = "000000000000000000000000000000000011111000000000000000000000";
const char sleep_32[] PROGMEM = "000000000000000000000000001111111111111100000000000000000000";
const char sleep_33[] PROGMEM = "000000000000000000000011111111111111111110000000000000000000";
const char sleep_34[] PROGMEM = "000000000000000000001111111111111111111111000000000000000000";
const char sleep_35[] PROGMEM = "000000000000000000011111111111111111111111000000000000000000";
const char sleep_36[] PROGMEM = "000000000000000000011111111110000111111111000000000000000000";
const char sleep_37[] PROGMEM = "000000000000000000000110000000000000000100000000000000000000";
const char sleep_38[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_39[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_40[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_41[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_42[] PROGMEM = "000000000000000100000000000000000000000000000011110000000000";
const char sleep_43[] PROGMEM = "000000001111111111111110000000000000011111111111111100000000";
const char sleep_44[] PROGMEM = "000000001111111111111110000000000000011111111111111100000000";
const char sleep_45[] PROGMEM = "000000001111111111111110000000000000001111111111111100000000";
const char sleep_46[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_47[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_48[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_49[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_50[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_51[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_52[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_53[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_54[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_55[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_56[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_57[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_58[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char sleep_59[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";

const char* const img_sleep[] PROGMEM = {
  sleep_0, sleep_1, sleep_2, sleep_3, sleep_4, sleep_5, sleep_6, sleep_7, sleep_8, sleep_9,
  sleep_10, sleep_11, sleep_12, sleep_13, sleep_14, sleep_15, sleep_16, sleep_17, sleep_18, sleep_19,
  sleep_20, sleep_21, sleep_22, sleep_23, sleep_24, sleep_25, sleep_26, sleep_27, sleep_28, sleep_29,
  sleep_30, sleep_31, sleep_32, sleep_33, sleep_34, sleep_35, sleep_36, sleep_37, sleep_38, sleep_39,
  sleep_40, sleep_41, sleep_42, sleep_43, sleep_44, sleep_45, sleep_46, sleep_47, sleep_48, sleep_49,
  sleep_50, sleep_51, sleep_52, sleep_53, sleep_54, sleep_55, sleep_56, sleep_57, sleep_58, sleep_59
};
// --- HAPPY 图 (55行，单帧静态) ---
const char happy_0[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char happy_1[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char happy_2[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char happy_3[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char happy_4[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char happy_5[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char happy_6[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char happy_7[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char happy_8[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char happy_9[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char happy_10[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char happy_11[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char happy_12[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char happy_13[] PROGMEM = "000000000000000000000000001111111100000000000000000000000000";
const char happy_14[] PROGMEM = "000000000000000000000000011111111110000000000000000000000000";
const char happy_15[] PROGMEM = "000000000000000000000000111111111111100000000000000000000000";
const char happy_16[] PROGMEM = "000011101110111000000000111000000111100000011101100111000000";
const char happy_17[] PROGMEM = "000011101110111000000001111000000011110000011101110111000000";
const char happy_18[] PROGMEM = "000001110111011100000001110000000011110000011101111011100000";
const char happy_19[] PROGMEM = "000001111111111110000001100000000001110000001110111011110000";
const char happy_20[] PROGMEM = "000000111011101110000001111111111111110000000111011101110000";
const char happy_21[] PROGMEM = "000000011101110110000001111111111111110000000111011101110000";
const char happy_22[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char happy_23[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char happy_24[] PROGMEM = "000000000111000000000000000000000000000000000000011100000000";
const char happy_25[] PROGMEM = "000000000111100000000000000000000000000000000001111100000000";
const char happy_26[] PROGMEM = "000000000111111000000000000000000000000000000111111100000000";
const char happy_27[] PROGMEM = "000000000111111110000000000000000000000000011111111000000000";
const char happy_28[] PROGMEM = "000000000001111111000000000000000000000000111111100000000000";
const char happy_29[] PROGMEM = "000000000000011111110000000000000000000011111111000000000000";
const char happy_30[] PROGMEM = "000000000000001111111000000000000000000111111100000000000000";
const char happy_31[] PROGMEM = "000000000000000011111100000000000000001111110000000000000000";
const char happy_32[] PROGMEM = "000000000000000011111100000000000000001111100000000000000000";
const char happy_33[] PROGMEM = "000000000000000111111100000000000000001111110000000000000000";
const char happy_34[] PROGMEM = "000000000000011111110000000000000000000011111100000000000000";
const char happy_35[] PROGMEM = "000000000001111111100000000000000000000000111111000000000000";
const char happy_36[] PROGMEM = "000000000001111110000000000000000000000000001111110000000000";
const char happy_37[] PROGMEM = "000000000111111000000000000000000000000000000111111100000000";
const char happy_38[] PROGMEM = "000000000111110000000000000000000000000000000001111100000000";
const char happy_39[] PROGMEM = "000000000111000000000000000000000000000000000000011100000000";
const char happy_40[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char happy_41[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char happy_42[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char happy_43[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char happy_44[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char happy_45[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char happy_46[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char happy_47[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char happy_48[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char happy_49[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char happy_50[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char happy_51[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char happy_52[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char happy_53[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char happy_54[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";

const char* const img_happy[] PROGMEM = {
  happy_0, happy_1, happy_2, happy_3, happy_4, happy_5, happy_6, happy_7, happy_8, happy_9,
  happy_10, happy_11, happy_12, happy_13, happy_14, happy_15, happy_16, happy_17, happy_18, happy_19,
  happy_20, happy_21, happy_22, happy_23, happy_24, happy_25, happy_26, happy_27, happy_28, happy_29,
  happy_30, happy_31, happy_32, happy_33, happy_34, happy_35, happy_36, happy_37, happy_38, happy_39,
  happy_40, happy_41, happy_42, happy_43, happy_44, happy_45, happy_46, happy_47, happy_48, happy_49,
  happy_50, happy_51, happy_52, happy_53, happy_54
};
// --- SPEAK 闭嘴 (60行) ---
// --- SPEAK 闭嘴 (60行) ---
const char speak_closed_0[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_1[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_2[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_3[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_4[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_5[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_6[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_7[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_8[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_9[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_10[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_11[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_12[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_13[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_14[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_15[] PROGMEM = "000000000000000000000000000111110000000000000000000000000000";
const char speak_closed_16[] PROGMEM = "000000000000000000000000001111111100000000000000000000000000";
const char speak_closed_17[] PROGMEM = "000000000000000000000000011111111110000000000000000000000000";
const char speak_closed_18[] PROGMEM = "000000000000000000000000111111111110000000000000000000000000";
const char speak_closed_19[] PROGMEM = "000000000000000000000000111111111111000000000000000000000000";
const char speak_closed_20[] PROGMEM = "000000000000000000000000111111111111000000000000000000000000";
const char speak_closed_21[] PROGMEM = "000000000000000000000001111111111111000000000000000000000000";
const char speak_closed_22[] PROGMEM = "000000000000000000000001111111111111000000000000000000000000";
const char speak_closed_23[] PROGMEM = "000000000000000000000001111111111111000000000000000000000000";
const char speak_closed_24[] PROGMEM = "000000000000000000000001111111111111000000000000000000000000";
const char speak_closed_25[] PROGMEM = "000000000000000000000001111111111111000000000000000000000000";
const char speak_closed_26[] PROGMEM = "000000000000000000000001111111111111000000000000000000000000";
const char speak_closed_27[] PROGMEM = "000000000000000000000000011111111111000000000000000000000000";
const char speak_closed_28[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_29[] PROGMEM = "000000000000011000000000000000000000000000000100000000000000";
const char speak_closed_30[] PROGMEM = "000000000011111111000000000000000000000000111111100000000000";
const char speak_closed_31[] PROGMEM = "000000000111111111100000000000000000000001111111110000000000";
const char speak_closed_32[] PROGMEM = "000000001111111111110000000000000000000011111111111000000000";
const char speak_closed_33[] PROGMEM = "000000001111000011110000000000000000000111100000111100000000";
const char speak_closed_34[] PROGMEM = "000000011110000001111000000000000000000111000000011100000000";
const char speak_closed_35[] PROGMEM = "000000011110000000111000000000000000000111000000011110000000";
const char speak_closed_36[] PROGMEM = "000000011100100000111000000000000000000111000000011110000000";
const char speak_closed_37[] PROGMEM = "000000011101110000111000000000000000000111011100011110000000";
const char speak_closed_38[] PROGMEM = "000000011111110001111000000000000000000111111100011100000000";
const char speak_closed_39[] PROGMEM = "000000001111100011110000000000000000000111111000111100000000";
const char speak_closed_40[] PROGMEM = "000000001111111111110000000000000000000011111111111000000000";
const char speak_closed_41[] PROGMEM = "000000000111111111100000000000000000000001111111111000000000";
const char speak_closed_42[] PROGMEM = "000000000011111111000000000000000000000000111111110000000000";
const char speak_closed_43[] PROGMEM = "000000000000111100000000000000000000000000001111000000000000";
const char speak_closed_44[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_45[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_46[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_47[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_48[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_49[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_50[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_51[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_52[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_53[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_54[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_55[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_56[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_57[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_58[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_closed_59[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";

const char* const img_speak_closed[] PROGMEM = {
  speak_closed_0, speak_closed_1, speak_closed_2, speak_closed_3, speak_closed_4,
  speak_closed_5, speak_closed_6, speak_closed_7, speak_closed_8, speak_closed_9,
  speak_closed_10, speak_closed_11, speak_closed_12, speak_closed_13, speak_closed_14,
  speak_closed_15, speak_closed_16, speak_closed_17, speak_closed_18, speak_closed_19,
  speak_closed_20, speak_closed_21, speak_closed_22, speak_closed_23, speak_closed_24,
  speak_closed_25, speak_closed_26, speak_closed_27, speak_closed_28, speak_closed_29,
  speak_closed_30, speak_closed_31, speak_closed_32, speak_closed_33, speak_closed_34,
  speak_closed_35, speak_closed_36, speak_closed_37, speak_closed_38, speak_closed_39,
  speak_closed_40, speak_closed_41, speak_closed_42, speak_closed_43, speak_closed_44,
  speak_closed_45, speak_closed_46, speak_closed_47, speak_closed_48, speak_closed_49,
  speak_closed_50, speak_closed_51, speak_closed_52, speak_closed_53, speak_closed_54,
  speak_closed_55, speak_closed_56, speak_closed_57, speak_closed_58, speak_closed_59
};

// --- SPEAK 张嘴 (60行) ---
const char speak_open_0[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_1[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_2[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_3[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_4[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_5[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_6[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_7[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_8[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_9[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_10[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_11[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_12[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_13[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_14[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_15[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_16[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_17[] PROGMEM = "000000000000000000000000000111111000000000000000000000000000";
const char speak_open_18[] PROGMEM = "000000000000000000000000001111111100000000000000000000000000";
const char speak_open_19[] PROGMEM = "000000000000000000000000011111111110000000000000000000000000";
const char speak_open_20[] PROGMEM = "000000000000000000000000111111111111000000000000000000000000";
const char speak_open_21[] PROGMEM = "000000000000000000000000111111111111000000000000000000000000";
const char speak_open_22[] PROGMEM = "000000000000000000000001111111111111000000000000000000000000";
const char speak_open_23[] PROGMEM = "000000000000000000000001111111111111000000000000000000000000";
const char speak_open_24[] PROGMEM = "000000000000000000000001111111111111000000000000000000000000";
const char speak_open_25[] PROGMEM = "000000000000000000000000111111111111000000000000000000000000";
const char speak_open_26[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_27[] PROGMEM = "000000000000011000000000000000000000000000000100000000000000";
const char speak_open_28[] PROGMEM = "000000000011111111000000000000000000000000111111100000000000";
const char speak_open_29[] PROGMEM = "000000000111111111100000000000000000000001111111110000000000";
const char speak_open_30[] PROGMEM = "000000001111111111110000000000000000000011111111111000000000";
const char speak_open_31[] PROGMEM = "000000001111000011110000000000000000000111100000111100000000";
const char speak_open_32[] PROGMEM = "000000011110000001111000000000000000000111000000011100000000";
const char speak_open_33[] PROGMEM = "000000011110000000111000000000000000000111000000011110000000";
const char speak_open_34[] PROGMEM = "000000011100100000111000000000000000000111000000011110000000";
const char speak_open_35[] PROGMEM = "000000011101110000111000000000000000000111011100011110000000";
const char speak_open_36[] PROGMEM = "000000011111110001111000000000000000000111111100011100000000";
const char speak_open_37[] PROGMEM = "000000001111100011110000000000000000000111111000111100000000";
const char speak_open_38[] PROGMEM = "000000001111111111110000000000000000000011111111111000000000";
const char speak_open_39[] PROGMEM = "000000000111111111100000000000000000000001111111111000000000";
const char speak_open_40[] PROGMEM = "000000000011111111000000000000000000000000111111110000000000";
const char speak_open_41[] PROGMEM = "000000000000111100000000000000000000000000001111000000000000";
const char speak_open_42[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_43[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_44[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_45[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_46[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_47[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_48[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_49[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_50[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_51[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_52[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_53[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_54[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_55[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_56[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_57[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_58[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";
const char speak_open_59[] PROGMEM = "000000000000000000000000000000000000000000000000000000000000";

const char* const img_speak_open[] PROGMEM = {
  speak_open_0, speak_open_1, speak_open_2, speak_open_3, speak_open_4,
  speak_open_5, speak_open_6, speak_open_7, speak_open_8, speak_open_9,
  speak_open_10, speak_open_11, speak_open_12, speak_open_13, speak_open_14,
  speak_open_15, speak_open_16, speak_open_17, speak_open_18, speak_open_19,
  speak_open_20, speak_open_21, speak_open_22, speak_open_23, speak_open_24,
  speak_open_25, speak_open_26, speak_open_27, speak_open_28, speak_open_29,
  speak_open_30, speak_open_31, speak_open_32, speak_open_33, speak_open_34,
  speak_open_35, speak_open_36, speak_open_37, speak_open_38, speak_open_39,
  speak_open_40, speak_open_41, speak_open_42, speak_open_43, speak_open_44,
  speak_open_45, speak_open_46, speak_open_47, speak_open_48, speak_open_49,
  speak_open_50, speak_open_51, speak_open_52, speak_open_53, speak_open_54,
  speak_open_55, speak_open_56, speak_open_57, speak_open_58, speak_open_59
};

// ... 这里保持你原来的所有图片数据 ...

//==========================================
// 9. 图片尺寸参数
//==========================================
const int IMG_W_SLEEP = 60;
const int IMG_H_SLEEP = 60;

const int IMG_W_HAPPY = 60;
const int IMG_H_HAPPY = 55;

const int IMG_W_SPEAK = 60;
const int IMG_H_SPEAK = 60;

const int SCALE = 3;

int startX_SLEEP = (240 - IMG_W_SLEEP * SCALE) / 2;
int startY_SLEEP = (240 - IMG_H_SLEEP * SCALE) / 2;

int startX_HAPPY = (240 - IMG_W_HAPPY * SCALE) / 2;
int startY_HAPPY = (240 - IMG_H_HAPPY * SCALE) / 2;

int startX_SPEAK = (240 - IMG_W_SPEAK * SCALE) / 2;
int startY_SPEAK = (240 - IMG_H_SPEAK * SCALE) / 2;

// SPEAK 动画的嘴巴区域
// SPEAK 动画的嘴巴区域（图片已上下颠倒）
const int MOUTH_START = 17;  // 原34行对应倒置后的25行，取17-25范围
const int MOUTH_END = 25;    // 原42行对应倒置后的17行

//==========================================
// 11. 屏幕绘制函数
//==========================================

// 通用：完整绘制一帧
void drawFullImage(const char* const img[], int imgW, int imgH, int startX, int startY) {
  tft.fillScreen(ST77XX_BLACK);
  for (int y = 0; y < imgH; y++) {
    const char* row = (const char*)pgm_read_ptr(&img[y]);
    for (int x = 0; x < imgW; x++) {
      if (pgm_read_byte(&row[x]) == '1') {
        tft.fillRect(startX + x*SCALE, startY + y*SCALE, SCALE, SCALE, ST77XX_WHITE);
      }
    }
    yield();
  }
}

// SPEAK 专用：只更新嘴巴区域差异
void updateMouthDiff(const char* const img_new[], const char* const img_old[]) {
  for (int y = MOUTH_START; y <= MOUTH_END; y++) {
    const char* row_new = (const char*)pgm_read_ptr(&img_new[y]);
    const char* row_old = (const char*)pgm_read_ptr(&img_old[y]);
    
    for (int x = 0; x < IMG_W_SPEAK; x++) {
      char pixel_new = pgm_read_byte(&row_new[x]);
      char pixel_old = pgm_read_byte(&row_old[x]);
      
      if (pixel_new != pixel_old) {
        int xPos = startX_SPEAK + x * SCALE;
        int yPos = startY_SPEAK + y * SCALE;
        uint16_t color = (pixel_new == '1') ? ST77XX_WHITE : ST77XX_BLACK;
        tft.fillRect(xPos, yPos, SCALE, SCALE, color);
      }
    }
    yield();
  }
}

//==========================================
// 12. 状态切换函数
//==========================================
void enterState(State newState) {
  if (newState == currentState) return;
  
  previousState = currentState;
  currentState = newState;
  
  switch (newState) {
    case STATE_SLEEP:
      Serial.println("🎬 进入 SLEEP 状态");
      drawFullImage(img_sleep, IMG_W_SLEEP, IMG_H_SLEEP, startX_SLEEP, startY_SLEEP);
      break;
      
    case STATE_HAPPY:
      Serial.println("🎬 进入 HAPPY 状态");
      drawFullImage(img_happy, IMG_W_HAPPY, IMG_H_HAPPY, startX_HAPPY, startY_HAPPY);
      break;
      
    case STATE_SPEAK:
      Serial.println("🎬 进入 SPEAK 动画状态");
      // 先画闭嘴帧
      drawFullImage(img_speak_closed, IMG_W_SPEAK, IMG_H_SPEAK, startX_SPEAK, startY_SPEAK);
      showOpen = false;
      lastSwitch = millis();
      break;
  }
}

//==========================================
// 13. I2S 初始化（保持不变）
//==========================================
void initMicI2S() {
    i2s_config_t cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 256,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin = {
        .bck_io_num = MIC_SCK,
        .ws_io_num = MIC_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = MIC_SD
    };

    i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin);
    i2s_zero_dma_buffer(I2S_NUM_0);
}

void initSpeakerI2S() {
    i2s_config_t cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 256,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin = {
        .bck_io_num = SPK_BCK,
        .ws_io_num = SPK_WS,
        .data_out_num = SPK_DIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    esp_err_t err = i2s_driver_install(I2S_NUM_1, &cfg, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("Speaker I2S install failed: %d\n", err);
        return;
    }

    err = i2s_set_pin(I2S_NUM_1, &pin);
    if (err != ESP_OK) {
        Serial.printf("Speaker I2S pin failed: %d\n", err);
        return;
    }

    err = i2s_set_clk(I2S_NUM_1, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
    if (err != ESP_OK) {
        Serial.printf("Speaker I2S clk failed: %d\n", err);
    }

    i2s_zero_dma_buffer(I2S_NUM_1);
    Serial.println("Speaker I2S initialized");
}

//==========================================
// 14. TTS 缓存管理（保持不变）
//==========================================
void freeTtsBuffer() {
    if (ttsBuffer != nullptr) {
        free(ttsBuffer);
        ttsBuffer = nullptr;
    }
    ttsBufferLen = 0;
}

bool beginTtsBuffer() {
    freeTtsBuffer();
    ttsBuffer = (uint8_t*)malloc(MAX_PCM_BYTES);
    if (ttsBuffer == nullptr) {
        Serial.println("TTS buffer malloc failed!");
        return false;
    }
    ttsBufferLen = 0;
    return true;
}

bool appendTtsPcm(const uint8_t* data, size_t len) {
    if (ttsBuffer == nullptr || data == nullptr || len == 0) return false;
    if (ttsBufferLen + len > MAX_PCM_BYTES) {
        Serial.println("TTS buffer overflow!");
        return false;
    }
    memcpy(ttsBuffer + ttsBufferLen, data, len);
    ttsBufferLen += len;
    return true;
}

//==========================================
// 15. 播放 PCM
//==========================================
void playPcmToSpeaker(const uint8_t* data, size_t len) {
    if (data == nullptr || len == 0) return;
    len &= ~1UL;

    // 播放开始 → 进入 SPEAK 状态
    enterState(STATE_SPEAK);

    size_t offset = 0;
    while (offset < len) {
        // 在播放循环中维持动画
        animateSpeak();

        size_t written = 0;
        esp_err_t err = i2s_write(I2S_NUM_1, data + offset, len - offset, &written, 0);
        if (err != ESP_OK || written == 0) {
            Serial.printf("I2S write failed: err=%d, written=%u\n", err, (unsigned)written);
            break;
        }
        offset += written;
    }

    // 播放结束 → 回到 HAPPY
    Serial.println("✅ 播放完成，回到 HAPPY 状态");
    enterState(STATE_HAPPY);
}

//==========================================
// 16. SPEAK 动画更新（在 loop 或播放循环中调用）
//==========================================
void animateSpeak() {
  if (currentState != STATE_SPEAK) return;
  
  if (millis() - lastSwitch > animSpeed) {
    lastSwitch = millis();
    if (showOpen) {
      updateMouthDiff(img_speak_closed, img_speak_open);
      showOpen = false;
    } else {
      updateMouthDiff(img_speak_open, img_speak_closed);
      showOpen = true;
    }
  }
}

//==========================================
// 17. 处理 Mac 控制包（增加状态机集成）
//==========================================
bool handleControlPacket(const uint8_t* data, int len) {
    if (len == 9 && memcmp(data, "TTS_START", 9) == 0) {
        Serial.println("📢 收到 TTS_START → 准备播放");
        // 停止录音
        voiceSessionActive = false;  // 关闭语音会话
        receivingPCM = true;
        if (!beginTtsBuffer()) {
            receivingPCM = false;
            return true;
        }
        i2s_zero_dma_buffer(I2S_NUM_1);
        Serial2.println("TTS_START_RECEIVED");
        return true;
    }

    if (len == 7 && memcmp(data, "TTS_END", 7) == 0) {
        Serial.println("📢 收到 TTS_END → 开始播放");
        receivingPCM = false;

        if (ttsBufferLen > 0) {
            // playPcmToSpeaker 内部会切换 STATE_SPEAK，播完后回 STATE_HAPPY
            playPcmToSpeaker(ttsBuffer, ttsBufferLen);
        } else {
            Serial.println("⚠️ 无缓存数据");
        }

        freeTtsBuffer();
        Serial2.println("TTS_END_PLAYED");
        return true;
    }

    return false;
}

//==========================================
// 18. 处理 STM32 指令（增加 STOP 处理）
//==========================================
void processSTM32Command(String cmd) {
    cmd.trim();
    
    Serial.print("📩 收到 STM32: [");
    Serial.print(cmd);
    Serial.println("]");
    
    if (cmd == "WAKE") {
        Serial.println("   ✅ WAKE → 进入 HAPPY + 开始录音");
        
        // 进入 HAPPY 状态
        enterState(STATE_HAPPY);
        
        // 开始录音
        voiceSessionActive = true;
        
        // 通知 Mac
        if (WiFi.status() == WL_CONNECTED) {
            udp.beginPacket(macIP, udpPort);
            udp.print("VOICE_START");
            udp.endPacket();
            Serial2.println("WAKE_OK");
        } else {
            Serial2.println("WAKE_FAIL_WIFI");
        }
    }
    else if (cmd == "STOP") {
        Serial.println("   ✅ STOP → 进入 SLEEP");
        
        // 停止一切活动
        voiceSessionActive = false;
        receivingPCM = false;
        freeTtsBuffer();
        
        // 进入 SLEEP
        enterState(STATE_SLEEP);
        Serial2.println("STOP_OK");
    }
    else {
        Serial.println("   ❌ 未知指令");
        Serial2.println("UNKNOWN");
    }
}

//==========================================
// 19. WiFi 连接
//==========================================
void connectWiFi() {
    Serial.println("Connecting WiFi...");
    WiFi.begin(ssid, password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi Connected");
        Serial.print("ESP32 IP: ");
        Serial.println(WiFi.localIP());
        udp.begin(udpPort);
    } else {
        Serial.println("\nWiFi Connection Failed!");
    }
}

//==========================================
// 20. setup
//==========================================


void setup() {
    Serial.begin(115200);
    
    // 等待最多 3 秒
    unsigned long start = millis();
    while (!Serial && (millis() - start < 3000)) {
        ; // 等待，但不死循环
    }
    
    // ✅ 修改点 2: 确保打印被执行
    Serial.println("\n\n");
    Serial.println("========================================");
    Serial.println("   ESP32-S3 语音助手 + 表情屏幕");
    Serial.println("========================================");
    Serial.flush();  // 强制发送缓冲区
    
    // ✅ 修改点 3: 初始化其他串口前确认主串口工作
    delay(200);
    
    Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
    
    Serial.println("串口初始化完成");
    Serial.flush();
    
    delay(1000);
    // 初始化屏幕
    SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
    tft.init(240, 240);
    tft.setRotation(0);

    // 初始化 WiFi 和音频
    connectWiFi();
    initMicI2S();
    initSpeakerI2S();
    delay(500);

    // 初始状态：SLEEP
    enterState(STATE_SLEEP);

    Serial.println("✅ 系统就绪，等待 STM32 指令...\n");
}

//==========================================
// 21. loop
//==========================================
void loop() {
    // ---- 接收 STM32 串口数据 ----
    while (Serial2.available()) {
        char c = Serial2.read();
        if (c == '\n') {
            if (cmdBuffer.length() > 0) {
                cmdComplete = true;
            }
        } else {
            cmdBuffer += c;
        }
    }
    
    if (cmdComplete) {
        cmdComplete = false;
        processSTM32Command(cmdBuffer);
        cmdBuffer = "";
    }

    // ---- 接收 Mac UDP 数据 ----
    int packetSize = udp.parsePacket();
    if (packetSize > 0) {
        int n = udp.read(udpBuf, min(packetSize, (int)sizeof(udpBuf)));
        if (n > 0) {
            if (handleControlPacket(udpBuf, n)) {
                // 控制包已处理
            } else if (receivingPCM) {
                if (!appendTtsPcm(udpBuf, n)) {
                    Serial.println("❌ 追加 PCM 失败");
                    receivingPCM = false;
                    freeTtsBuffer();
                }
            }
        }
    }

    // ---- 录音（仅在 voiceSessionActive 且非 TTS 阶段）----
    if (voiceSessionActive && !receivingPCM) {
        size_t bytesRead = 0;
        esp_err_t err = i2s_read(I2S_NUM_0, i2sMicBuffer, sizeof(i2sMicBuffer), &bytesRead, 0);

        if (err == ESP_OK && bytesRead > 0) {
            int sampleCount = bytesRead / sizeof(int32_t);
            for (int i = 0; i < sampleCount; i++) {
                pcmMicBuffer[i] = (int16_t)(i2sMicBuffer[i] >> 14);
            }
            if (WiFi.status() == WL_CONNECTED) {
                udp.beginPacket(macIP, udpPort);
                udp.write((uint8_t*)pcmMicBuffer, sampleCount * sizeof(int16_t));
                udp.endPacket();
            }
        }
    }

    // ---- 维持 SPEAK 动画（如果当前在播放，由播放循环驱动；如果通过 loop 驱动也行）----
    // 注意：playPcmToSpeaker 内部用阻塞式 i2s_write + animateSpeak() 驱动
    // 如果你希望非阻塞播放，需要把播放也放到 loop 里，这里先保持简单
    
    delay(1);
}