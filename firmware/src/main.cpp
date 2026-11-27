#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <SD.h>
#include <SPI.h>
#include <U8g2_for_Adafruit_GFX.h>

#include "button.h"
#include "catalog.h"
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
constexpr const char *ALL_RECIPES = "Wszystkie";

// Screen layout (800x480, landscape).
constexpr int MARGIN = 24;
constexpr int HEADER_BASELINE = 40;
constexpr int RULE_Y = 52;
constexpr int BODY_TOP = 66;
constexpr int LINE_HEIGHT = 26;
constexpr int BULLET_INDENT = 22;
constexpr size_t LINES_PER_PAGE = 15;
constexpr int ROW_HEIGHT = 36;
constexpr size_t ROWS_PER_PAGE = 11;

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

enum class Screen { Tags, Recipes, Recipe };
Screen screen = Screen::Tags;

std::vector<RecipeInfo> catalog;
std::vector<TagEntry> tags;  // tags[0] is the "all recipes" entry
size_t tagSel = 0;
std::vector<size_t> listed;  // indexes into `catalog` for the chosen tag
size_t recipeSel = 0;

Recipe recipe;
std::vector<Page> pages;
std::vector<Section> sections;
size_t currentPage = 0;
int returnPage = -1;  // page to go back to after a green-key look at the ingredients

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

// --- Drawing ---------------------------------------------------------------------------

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

void drawHeader(const std::string &title, const std::string &label) {
  u8g2.setForegroundColor(GxEPD_BLACK);
  u8g2.setFont(FONT_SMALL);
  const int labelWidth = u8g2.getUTF8Width(label.c_str());
  u8g2.drawUTF8(display.width() - MARGIN - labelWidth, HEADER_BASELINE, label.c_str());
  u8g2.setFont(FONT_TITLE);
  u8g2.drawUTF8(MARGIN, HEADER_BASELINE, title.c_str());
  display.fillRect(MARGIN, RULE_Y, display.width() - 2 * MARGIN, 2, GxEPD_BLACK);
}

void showMessage(const char *text) {
  beginDraw(true);
  do {
    display.fillScreen(GxEPD_WHITE);
    u8g2.setForegroundColor(GxEPD_BLACK);
    u8g2.setFont(FONT_TITLE);
    const int x = (display.width() - u8g2.getUTF8Width(text)) / 2;
    u8g2.drawUTF8(x, display.height() / 2, text);
  } while (display.nextPage());
}

// A scrolling list: shows the page of rows that holds `selected`, with that row inverted.
void drawList(const std::string &title, size_t count, size_t selected,
              const std::function<void(size_t, std::string &, std::string &)> &row, bool full) {
  const size_t first = selected / ROWS_PER_PAGE * ROWS_PER_PAGE;
  const std::string label = std::to_string(selected + 1) + "/" + std::to_string(count);
  const int rowWidth = display.width() - 2 * MARGIN;

  beginDraw(full);
  do {
    display.fillScreen(GxEPD_WHITE);
    drawHeader(title, label);
    u8g2.setFont(FONT_BODY);
    for (size_t i = first; i < count && i < first + ROWS_PER_PAGE; ++i) {
      std::string left, right;
      row(i, left, right);
      const int top = BODY_TOP + int(i - first) * ROW_HEIGHT;
      const int baseline = top + ROW_HEIGHT - 11;
      const bool sel = i == selected;
      if (sel) display.fillRect(MARGIN, top, rowWidth, ROW_HEIGHT, GxEPD_BLACK);
      u8g2.setForegroundColor(sel ? GxEPD_WHITE : GxEPD_BLACK);
      u8g2.setBackgroundColor(sel ? GxEPD_BLACK : GxEPD_WHITE);
      u8g2.drawUTF8(MARGIN + 10, baseline, left.c_str());
      const int rightWidth = u8g2.getUTF8Width(right.c_str());
      u8g2.drawUTF8(MARGIN + rowWidth - 10 - rightWidth, baseline, right.c_str());
    }
    u8g2.setForegroundColor(GxEPD_BLACK);
    u8g2.setBackgroundColor(GxEPD_WHITE);
  } while (display.nextPage());
}

void drawRecipe(bool full) {
  std::string label = "str. " + std::to_string(currentPage + 1) + "/" + std::to_string(pages.size());
  const int section = sectionOfPage(sections, currentPage);
  if (section >= 0) label = sections[section].name + " · " + label;

  beginDraw(full);
  do {
    display.fillScreen(GxEPD_WHITE);
    drawHeader(recipe.title, label);
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

void draw(bool full) {
  switch (screen) {
    case Screen::Tags:
      drawList("Tagi", tags.size(), tagSel,
               [](size_t i, std::string &left, std::string &right) {
                 left = tags[i].name;
                 right = std::to_string(tags[i].count);
               },
               full);
      break;
    case Screen::Recipes:
      drawList(tags[tagSel].name, listed.size(), recipeSel,
               [](size_t i, std::string &left, std::string &) { left = catalog[listed[i]].title; },
               full);
      break;
    case Screen::Recipe:
      drawRecipe(full);
      break;
  }
}

// --- Data ------------------------------------------------------------------------------

bool readFile(const char *path, std::string &out) {
  File f = SD.open(path);
  if (!f || f.size() > MAX_RECIPE_BYTES) return false;
  const String text = f.readString();
  out.assign(text.c_str(), text.length());
  return true;
}

// Reads the header of every .md file in RECIPES_DIR. Returns an error for the screen, or nullptr.
const char *loadCatalog() {
  if (!SD.begin(SD_CS, spiBus, 10000000)) return "Brak karty SD";

  File dir = SD.open(RECIPES_DIR);
  if (!dir || !dir.isDirectory()) return "Brak folderu /recipes";

  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    const String name = f.name();
    const String path = f.path();
    if (f.isDirectory() || !name.endsWith(".md")) continue;
    f.close();

    std::string text;
    if (!readFile(path.c_str(), text)) {
      Serial.printf("Skipped %s (unreadable or over %u bytes)\n", path.c_str(), unsigned(MAX_RECIPE_BYTES));
      continue;
    }
    const Recipe r = parseRecipe(text);
    catalog.push_back({r.title.empty() ? std::string(name.c_str()) : r.title, r.tags, path.c_str()});
  }
  if (catalog.empty()) return "Brak przepisu w /recipes";

  tags = collectTags(catalog);
  tags.insert(tags.begin(), {ALL_RECIPES, catalog.size()});
  Serial.printf("Catalog: %u recipes, %u tags\n", unsigned(catalog.size()), unsigned(tags.size() - 1));
  return nullptr;
}

bool openRecipe(const RecipeInfo &info) {
  std::string text;
  if (!readFile(info.path.c_str(), text)) return false;
  recipe = parseRecipe(text);
  recipe.title = info.title;
  pages = paginate(recipe.blocks, display.width() - 2 * MARGIN, BULLET_INDENT, LINES_PER_PAGE,
                   measure);
  sections = findSections(pages);
  currentPage = 0;
  returnPage = -1;
  return true;
}

// --- Navigation ------------------------------------------------------------------------

// Lists wrap around (last -> first); recipe pages stop at the ends so you never land back on
// the ingredients by flipping one page too far.
void step(size_t &index, size_t count, int pin, bool wrap) {
  if (count == 0) return;
  if (pin == KEY_RIGHT) index = index + 1 < count ? index + 1 : wrap ? 0 : index;
  if (pin == KEY_LEFT) index = index > 0 ? index - 1 : wrap ? count - 1 : index;
}

// Applies one key event. Returns true when the screen changed (which wants a full refresh).
bool handle(const KeyEvent &e) {
  const bool shortPress = e.press == Press::Short;
  const bool back = e.press == Press::Long && e.pin == KEY_LEFT;
  const bool open = shortPress && e.pin == KEY_GREEN;

  switch (screen) {
    case Screen::Tags:
      if (open) {
        listed = recipesWithTag(catalog, tagSel == 0 ? "" : tags[tagSel].name);
        recipeSel = 0;
        screen = Screen::Recipes;
        return true;
      }
      if (shortPress) step(tagSel, tags.size(), e.pin, true);
      break;

    case Screen::Recipes:
      if (back) {
        screen = Screen::Tags;
        return true;
      }
      if (open) {
        if (!openRecipe(catalog[listed[recipeSel]])) {
          Serial.printf("Cannot open %s\n", catalog[listed[recipeSel]].path.c_str());
          break;
        }
        screen = Screen::Recipe;
        return true;
      }
      if (shortPress) step(recipeSel, listed.size(), e.pin, true);
      break;

    case Screen::Recipe:
      if (back) {
        screen = Screen::Recipes;
        return true;
      }
      if (open) currentPage = toggleIngredients(sections, currentPage, returnPage);
      if (shortPress) step(currentPage, pages.size(), e.pin, false);
      break;
  }
  return false;
}

// --- Main ------------------------------------------------------------------------------

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

  if (const char *error = loadCatalog()) {
    Serial.println(error);
    showMessage(error);
  } else {
    draw(true);
  }

  keyEvents = xQueueCreate(16, sizeof(KeyEvent));
  xTaskCreate(keyTask, "keys", 2048, nullptr, 2, nullptr);
}

void loop() {
  KeyEvent e;
  xQueueReceive(keyEvents, &e, portMAX_DELAY);
  if (catalog.empty()) return;

  // Apply every queued press, then draw once: fast presses skip intermediate refreshes.
  // Changing screens uses a full refresh to clear e-ink ghosting; moving within one is partial.
  const Screen beforeScreen = screen;
  const size_t before[] = {tagSel, recipeSel, currentPage};
  bool screenChanged = false;
  do {
    screenChanged |= handle(e);
  } while (xQueueReceive(keyEvents, &e, 0) == pdTRUE);

  const bool moved = before[0] != tagSel || before[1] != recipeSel || before[2] != currentPage;
  if (screenChanged || screen != beforeScreen) {
    draw(true);
  } else if (moved) {
    draw(false);
  }
}
