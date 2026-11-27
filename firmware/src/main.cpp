#include <Arduino.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <GxEPD2_BW.h>
#include <SPI.h>

#include "button.h"

// reTerminal E1001 panel wiring (write-only SPI, no MISO).
constexpr int EPD_SCK = 7;
constexpr int EPD_MOSI = 9;
constexpr int EPD_CS = 10;
constexpr int EPD_DC = 11;
constexpr int EPD_RST = 12;
constexpr int EPD_BUSY = 13;

// Keys are active-low. Confirmed on hardware, viewed from the front.
constexpr int KEY_GREEN = 3;
constexpr int KEY_RIGHT = 4;
constexpr int KEY_LEFT = 5;
constexpr int KEY_PINS[] = {KEY_GREEN, KEY_RIGHT, KEY_LEFT};
constexpr size_t KEY_COUNT = sizeof(KEY_PINS) / sizeof(KEY_PINS[0]);

SPIClass epdSpi(HSPI);
GxEPD2_BW<GxEPD2_750_GDEY075T7, GxEPD2_750_GDEY075T7::HEIGHT> display(
    GxEPD2_750_GDEY075T7(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));
Button keys[KEY_COUNT];

struct KeyEvent {
  int pin;
  Press press;
};
QueueHandle_t keyEvents;

// Keys are sampled in their own task because a panel refresh blocks for ~1 s; polling from
// loop() would drop any press that starts and ends during a refresh.
void keyTask(void *) {
  for (;;) {
    for (size_t i = 0; i < KEY_COUNT; ++i) {
      Press p = keys[i].update(digitalRead(KEY_PINS[i]) == LOW, millis());
      if (p != Press::None) {
        KeyEvent e{KEY_PINS[i], p};
        xQueueSend(keyEvents, &e, 0);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void showCentered(const char *text, bool full) {
  int16_t x, y;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x, &y, &w, &h);

  if (full) {
    display.setFullWindow();
  } else {
    display.setPartialWindow(0, 0, display.width(), display.height());
  }
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor((display.width() - w) / 2 - x, (display.height() - h) / 2 - y);
    display.print(text);
  } while (display.nextPage());
}

void setup() {
  Serial.begin(115200);
  for (int pin : KEY_PINS) pinMode(pin, INPUT_PULLUP);

  epdSpi.begin(EPD_SCK, -1, EPD_MOSI, -1);
  display.epd2.selectSPI(epdSpi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  display.init(115200);
  display.setRotation(0);
  display.setFont(&FreeSansBold24pt7b);
  display.setTextColor(GxEPD_BLACK);

  showCentered("Press a key", true);

  keyEvents = xQueueCreate(16, sizeof(KeyEvent));
  xTaskCreate(keyTask, "keys", 2048, nullptr, 2, nullptr);
}

void loop() {
  KeyEvent e;
  xQueueReceive(keyEvents, &e, portMAX_DELAY);

  // Log every event, but redraw only the newest: presses made during a refresh are not lost,
  // and the screen does not replay a backlog one slow refresh at a time.
  char text[32];
  do {
    snprintf(text, sizeof(text), "GPIO%d  %s", e.pin, e.press == Press::Long ? "long" : "short");
    Serial.println(text);
  } while (xQueueReceive(keyEvents, &e, 0) == pdTRUE);
  showCentered(text, false);
}
