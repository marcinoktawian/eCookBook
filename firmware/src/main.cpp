#include <Arduino.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <GxEPD2_BW.h>
#include <SPI.h>

// reTerminal E1001 panel wiring (write-only SPI, no MISO).
constexpr int EPD_SCK = 7;
constexpr int EPD_MOSI = 9;
constexpr int EPD_CS = 10;
constexpr int EPD_DC = 11;
constexpr int EPD_RST = 12;
constexpr int EPD_BUSY = 13;

SPIClass epdSpi(HSPI);
GxEPD2_BW<GxEPD2_750_GDEY075T7, GxEPD2_750_GDEY075T7::HEIGHT> display(
    GxEPD2_750_GDEY075T7(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

void setup() {
  Serial.begin(115200);

  epdSpi.begin(EPD_SCK, -1, EPD_MOSI, -1);
  display.epd2.selectSPI(epdSpi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  display.init(115200);

  const char *text = "Hello, eCookBook!";
  display.setRotation(0);
  display.setFont(&FreeSansBold24pt7b);
  display.setTextColor(GxEPD_BLACK);

  int16_t x, y;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x, &y, &w, &h);

  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor((display.width() - w) / 2 - x, (display.height() - h) / 2 - y);
    display.print(text);
  } while (display.nextPage());

  display.hibernate();  // e-ink keeps the image without power
  Serial.println("Hello world drawn");
}

void loop() {}
