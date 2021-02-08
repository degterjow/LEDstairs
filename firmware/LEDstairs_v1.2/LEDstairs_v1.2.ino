/*
  Новые функции в прошивке 1.2 описаны в README.MD
  Автор: Геннадий Дегтерёв, 2021
  gennadij@degterjow.de

  Скетч к проекту "Подсветка лестницы"
  Страница проекта (схемы, описания): https://alexgyver.ru/ledstairs/
  Исходники на GitHub: https://github.com/AlexGyver/LEDstairs
  Нравится, как написан код? Поддержи автора! https://alexgyver.ru/support_alex/
  Автор: AlexGyver Technologies, 2019
  https://AlexGyver.ru/
*/

struct Step {
  int8_t start;
  int8_t led_amount;
  bool nightMode;  
};

#define STRIP_LED_AMOUNT 256  // кол-во светодиодов на всех ступеньках
#define STEP_AMOUNT 16        // количество ступенек

// описание всех ступенек с возможностью подсветки ЛЮБЫХ ступенек в ночном режиме
Step steps[STEP_AMOUNT] = { 
{ 0, 16, true },    // первая ступенька начинается с диода 0, всего диодов на ступенке 16, true - подсвечивается в ночном режиме
{ 16, 16, false },  // вторая ступенька начинается с диода 16 (первая 0-15), всего диодов на ступенке 16, false - не подсвечивается в ночном режиме
{ 32, 16, false },
{ 48, 16, false },
{ 64, 16, false },
{ 80, 16, false },
{ 96, 16, false },
{ 112, 16, false },
{ 128, 16, false },
{ 144, 16, false },
{ 160, 16, false },
{ 176, 16, false },
{ 192, 16, false },
{ 208, 16, false },
{ 224, 16, false },
{ 240, 16, true }
};

#define AUTO_BRIGHT 1     // автояркость вкл(1)/выкл(0) (с фоторезистором)
#define CUSTOM_BRIGHT 100  // ручная яркость

#define FADR_SPEED 300         // скорость переключения с одной ступеньки на другую, меньше - быстрее
#define START_EFFECT RAINBOW   // режим при старте COLOR, RAINBOW, FIRE
#define ROTATE_EFFECTS 1      // вкл(1)/выкл(0) - автосмена эффектов
#define TIMEOUT 15            // секунд, таймаут выключения ступенек после срабатывания одного из датчиков движения

int16_t NIGHT_LIGHT_BIT_MASK = 0b0100100100100100;  // последовательность диодов в ночном режиме, чтобы диоды не выгорали
#define NIGHT_LIGHT_COLOR mCOLOR(WHITE)  // по умолчанию белый
#define NIGHT_LIGHT_BRIGHT 50  // 0 - 255 яркость ночной подсветки
#define NIGHT_PHOTO_MAX 500   // максимальное значение фоторезистора для отключения подсветки, при освещении выше этого подсветка полностью отключается

#define RAILING 0      // вкл(1)/выкл(0) - подсветка перил
#define RAILING_LED_AMOUNT 75    // количество чипов WS2811 на ленте перил

// пины
// если перепутаны сенсоры - можно поменять их местами в коде! Вот тут
#define SENSOR_START 3   // пин датчика движения
#define SENSOR_END 2     // пин датчика движения
#define STRIP_PIN 12     // пин ленты ступенек
#define RAILING_PIN 11   // пин ленты перил
#define PHOTO_PIN A0     // пин фоторезистора

#define ORDER_BGR       // порядок цветов ORDER_GRB / ORDER_RGB / ORDER_BRG
#define COLOR_DEBTH 2   // цветовая глубина: 1, 2, 3 (в байтах)

// для разработчиков
#include <microLED.h>
#include <FastLED.h> // ФЛ для функции Noise


// ==== удобные макросы ====
#define FOR_i(from, to) for(int i = (from); i < (to); i++)
#define FOR_j(from, to) for(int j = (from); j < (to); j++)
#define FOR_k(from, to) for(int k = (from); k < (to); k++)
#define EVERY_MS(x) \
  static uint32_t tmr;\
  bool flag = millis() - tmr >= (x);\
  if (flag) tmr = millis();\
  if (flag)
//===========================

int railingSegmentLength = RAILING_LED_AMOUNT / STEP_AMOUNT;   // количество чипов WS2811 на сегмент ленты перил

LEDdata stripLEDs[STRIP_LED_AMOUNT];  // буфер ленты ступенек
microLED strip(stripLEDs, STRIP_LED_AMOUNT, STRIP_PIN);  // объект лента (НЕ МАТРИЦА) из за разного количества диодов на ступеньку!

LEDdata railingLEDs[RAILING_LED_AMOUNT];  // буфер ленты перил
microLED railing(railingLEDs, RAILING_LED_AMOUNT, RAILING_PIN);  // объект лента

int effSpeed;
int8_t effectDirection;
byte curBright = CUSTOM_BRIGHT;
enum {COLOR, RAINBOW, FIRE, EFFECTS_AMOUNT} curEffect = START_EFFECT;
byte effectCounter;
uint32_t timeoutCounter;
bool systemIdleState;
bool systemOffState;
uint16_t nightLightBitMask = NIGHT_LIGHT_BIT_MASK;

struct PirSensor {
  int8_t effectDirection;
  int8_t pin;
  bool lastState;
};

PirSensor startPirSensor = { 1, SENSOR_START, false};
PirSensor endPirSensor = { -1, SENSOR_END, false};

CRGBPalette16 firePalette;

int8_t minStepLength = steps[0].led_amount;

void setup() {
  Serial.begin(9600);
  setBrightness(curBright);    // яркость (0-255)
  clear();
  show();
  firePalette = CRGBPalette16(
                  getFireColor(0 * 16),
                  getFireColor(1 * 16),
                  getFireColor(2 * 16),
                  getFireColor(3 * 16),
                  getFireColor(4 * 16),
                  getFireColor(5 * 16),
                  getFireColor(6 * 16),
                  getFireColor(7 * 16),
                  getFireColor(8 * 16),
                  getFireColor(9 * 16),
                  getFireColor(10 * 16),
                  getFireColor(11 * 16),
                  getFireColor(12 * 16),
                  getFireColor(13 * 16),
                  getFireColor(14 * 16),
                  getFireColor(15 * 16)
                );
  // определяем минимальную ширину ступеньки для корректной работы эффекта огня
  FOR_i(1, STEP_AMOUNT) {
    if (steps[i].led_amount < minStepLength)
      minStepLength = steps[i].led_amount;
  }
  delay(100);
  clear();
  show();
}

void loop() {
  handlePirSensor(&startPirSensor);
  handlePirSensor(&endPirSensor);
  if (systemIdleState || systemOffState) {
    handlePhotoResistor();
    handleNightLight();
    delay(50);
  } else {
    effectFlow();
    handleTimeout();
  }
}

void handlePhotoResistor() {
#if (AUTO_BRIGHT == 1)
  EVERY_MS(3000) {            // каждые 3 сек
    int photo = analogRead(PHOTO_PIN);
    Serial.print("Photo resistor ");
    Serial.println(photo);
    systemOffState = photo > NIGHT_PHOTO_MAX;
    curBright = systemOffState ? 0 : map(photo, 30, 800, 10, 200);
    setBrightness(curBright);
  }
#endif
}

void handleNightLight() {
  EVERY_MS(60000) {
    nightLight();
  }
}

void nightLight() {
  if (systemOffState) {
    clear();
    show();
    return;
  }
  // циклически сдвигаем маску, чтобы диоды не выгорали
  nightLightBitMask = nightLightBitMask >> 1 | nightLightBitMask << 15;
  animatedSwitchOff(NIGHT_LIGHT_BRIGHT);
  clear();
  fillStepWithBitMask(0, NIGHT_LIGHT_COLOR, nightLightBitMask);
  fillStepWithBitMask(STEP_AMOUNT - 1, NIGHT_LIGHT_COLOR, nightLightBitMask);
  animatedSwitchOn(NIGHT_LIGHT_BRIGHT);
}

void handleTimeout() {
  if (millis() - timeoutCounter >= (TIMEOUT * 1000L)) {
    systemIdleState = true;
    if (effectDirection == 1) {
      stepFader(0, 1);
    } else {
      stepFader(1, 1);
    }
    nightLight();
  }
}

void handlePirSensor(PirSensor *sensor) {
  if (systemOffState) return;

  int newState = digitalRead(sensor->pin);
  if (newState && !sensor->lastState)
    timeoutCounter = millis(); // при срабатывании датчика устанавливаем заново timeout
    if (systemIdleState) {
      effectDirection = sensor->effectDirection;
      if (ROTATE_EFFECTS) {
        curEffect = ++effectCounter % EFFECTS_AMOUNT;
      }
      stepFader(effectDirection == 1 ? 0 : 1,  0);
      systemIdleState = false;
    }
  }
  sensor->lastState = newState;
}

// крутилка эффектов в режиме активной работы
void effectFlow() {
  static uint32_t tmr;
  if (millis() - tmr >= effSpeed) {
    tmr = millis();
    switch (curEffect) {
      case COLOR: staticColor(effectDirection, 0, STEP_AMOUNT); break;
      case RAINBOW: rainbowStripes(-effectDirection, 0, STEP_AMOUNT); break; // rainbowStripes - приёмный
      case FIRE: fireStairs(effectDirection, 0, 0); break;
    }
    show();
  }
}
