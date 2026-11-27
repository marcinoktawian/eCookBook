#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <SD.h>
#include <SPI.h>
#include <U8g2_for_Adafruit_GFX.h>

#include "button.h"
#include "recipe.h"

// reTerminal E1001 wiring. The panel and the microSD card share SCK/MOSI on one SPI bus.
constexpr int SPI_SCK = 7;
constexpr int SPI_MISO = 8;  // SD only; the panel is write-only
constexpr int SPI_MOSI = 9;
constexpr int EPD_CS = 10;
constexpr int EPD_DC = 11;
constexpr int EPD_RST = 12;
constexpr int EPD_BUSY = 13;
constexpr int SD_CS = 14;
constexpr int SD_POWER = 16;  // HIGH powers the card

// Keys are active-low. Confirmed on hardware, viewed from the front.
constexpr int KEY_GREEN = 3;
constexpr int KEY_RIGHT = 4;
constexpr int KEY_LEFT = 5;
constexpr int KEY_PINS[] = {KEY_GREEN, KEY_RIGHT, KEY_LEFT};
constexpr size_t KEY_COUNT = sizeof(KEY_PINS) / sizeof(KEY_PINS[0]);

constexpr const char *RECIPES_DIR = "/recipes";
constexpr size_t MAX_RECIPE_BYTES = 32 * 1024;

// Screen layout (800x480, landscape).
constexpr int MARGIN = 24;
constexpr int HEADER_BASELINE = 40;
constexpr int RULE_Y = 52;
constexpr int BODY_TOP = 66;
constexpr int LINE_HEIGHT = 26;
constexpr int BULLET_INDENT = 22;
constexpr size_t LINES_PER_PAGE = 15;

const uint8_t *const FONT_TITLE = u8g2_font_ncenB24_te;
const uint8_t *const FONT_HEADING = u8g2_font_ncenB18_te;
const uint8_t *const FONT_BODY = u8g2_font_ncenR18_te;
const uint8_t *const FONT_SMALL = u8g2_font_ncenR14_te;

SPIClass spiBus(HSPI);
GxEPD2_BW<GxEPD2_750_GDEY075T7, GxEPD2_750_GDEY075T7::HEIGHT> display(
    GxEPD2_750_GDEY075T7(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));
U8G2_FOR_ADAFRUIT_GFX u8g2;
Button keys[KEY_COUNT];

struct KeyEvent {
  int pin;
  Press press;
};
QueueHandle_t keyEvents;

Recipe recipe;
std::vector<Page> pages;
size_t currentPage = 0;

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

const uint8_t *fontFor(Block::Kind kind) { return kind == Block::Heading ? FONT_HEADING : FONT_BODY; }

int measure(Block::Kind kind, const std::string &text) {
  u8g2.setFont(fontFor(kind));
  return u8g2.getUTF8Width(text.c_str());
}

void beginDraw(bool full) {
  if (full) {
    display.setFullWindow();
  } else {
    display.setPartialWindow(0, 0, display.width(), display.height());
  }
  display.firstPage();
}

void showMessage(const char *text) {
  beginDraw(true);
  do {
    display.fillScreen(GxEPD_WHITE);
    u8g2.setFont(FONT_TITLE);
    const int x = (display.width() - u8g2.getUTF8Width(text)) / 2;
    u8g2.drawUTF8(x, display.height() / 2, text);
  } while (display.nextPage());
}

void drawPage(bool full) {
  char pageLabel[16];
  snprintf(pageLabel, sizeof(pageLabel), "str. %u/%u", unsigned(currentPage + 1), unsigned(pages.size()));

  beginDraw(full);
  do {
    display.fillScreen(GxEPD_WHITE);

    u8g2.setFont(FONT_SMALL);
    const int labelWidth = u8g2.getUTF8Width(pageLabel);
    u8g2.drawUTF8(display.width() - MARGIN - labelWidth, HEADER_BASELINE, pageLabel);
    u8g2.setFont(FONT_TITLE);
    u8g2.drawUTF8(MARGIN, HEADER_BASELINE, recipe.title.c_str());
    display.fillRect(MARGIN, RULE_Y, display.width() - 2 * MARGIN, 2, GxEPD_BLACK);

    int baseline = BODY_TOP + LINE_HEIGHT - 6;
    for (const Line &line : pages[currentPage]) {
      u8g2.setFont(fontFor(line.kind));
      int x = MARGIN;
      if (line.kind == Block::Bullet) {
        if (line.first) display.fillCircle(MARGIN + 7, baseline - 7, 3, GxEPD_BLACK);
        x += BULLET_INDENT;
      }
      u8g2.drawUTF8(x, baseline, line.text.c_str());
      baseline += LINE_HEIGHT;
    }
  } while (display.nextPage());
}

// Loads the first .md file in RECIPES_DIR. Returns an error message for the screen, or nullptr.
const char *loadRecipe() {
  if (!SD.begin(SD_CS, spiBus, 10000000)) return "Brak karty SD";

  File dir = SD.open(RECIPES_DIR);
  if (!dir || !dir.isDirectory()) return "Brak folderu /recipes";

  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    const String name = f.name();
    if (f.isDirectory() || !name.endsWith(".md")) continue;
    if (f.size() > MAX_RECIPE_BYTES) return "Przepis jest za duży";

    const String text = f.readString();
    recipe = parseRecipe(std::string(text.c_str(), text.length()));
    if (recipe.title.empty()) recipe.title = std::string(name.c_str());
    Serial.printf("Loaded %s (%u bytes)\n", name.c_str(), unsigned(text.length()));
    return nullptr;
  }
  return "Brak przepisu w /recipes";
}

void setup() {
  Serial.begin(115200);
  for (int pin : KEY_PINS) pinMode(pin, INPUT_PULLUP);
  pinMode(SD_POWER, OUTPUT);
  digitalWrite(SD_POWER, HIGH);

  spiBus.begin(SPI_SCK, SPI_MISO, SPI_MOSI, -1);
  display.epd2.selectSPI(spiBus, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  display.init(115200);
  display.setRotation(0);
  u8g2.begin(display);
  u8g2.setFontMode(1);
  u8g2.setForegroundColor(GxEPD_BLACK);
  u8g2.setBackgroundColor(GxEPD_WHITE);

  if (const char *error = loadRecipe()) {
    Serial.println(error);
    showMessage(error);
  } else {
    pages = paginate(recipe.blocks, display.width() - 2 * MARGIN, BULLET_INDENT, LINES_PER_PAGE,
                     measure);
    drawPage(true);
  }

  keyEvents = xQueueCreate(16, sizeof(KeyEvent));
  xTaskCreate(keyTask, "keys", 2048, nullptr, 2, nullptr);
}

void loop() {
  KeyEvent e;
  xQueueReceive(keyEvents, &e, portMAX_DELAY);
  if (pages.empty()) return;

  // Apply every queued press, then draw once: fast flipping skips intermediate refreshes.
  const size_t before = currentPage;
  do {
    if (e.press != Press::Short) continue;
    if (e.pin == KEY_RIGHT && currentPage + 1 < pages.size()) ++currentPage;
    if (e.pin == KEY_LEFT && currentPage > 0) --currentPage;
  } while (xQueueReceive(keyEvents, &e, 0) == pdTRUE);

  if (currentPage != before) drawPage(false);
}
