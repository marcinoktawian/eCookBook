#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <HTTPClient.h>
#include <SD.h>
#include <SPI.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <WiFi.h>
#include <mbedtls/sha256.h>

#include "button.h"
#include "catalog.h"
#include "recipe.h"
#include "sync.h"

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

constexpr const char *CONFIG_PATH = "/config.txt";
constexpr const char *DOWNLOAD_TMP = "/recipes/.download.tmp";  // hidden: never in the catalog
constexpr uint32_t WIFI_TIMEOUT_MS = 15000;
constexpr uint16_t HTTP_TIMEOUT_MS = 10000;
constexpr int MAX_INDEX_BYTES = 64 * 1024;

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

bool sdReady = false;
constexpr const char *NO_RECIPES = "Brak przepisów. Przytrzymaj prawy, żeby pobrać.";

// Reads the header of every .md file in RECIPES_DIR. Returns an error for the screen, or nullptr.
const char *loadCatalog() {
  catalog.clear();
  tags.clear();
  listed.clear();
  tagSel = recipeSel = 0;
  if (!sdReady) return "Brak karty SD";

  File dir = SD.open(RECIPES_DIR);
  if (!dir || !dir.isDirectory()) return NO_RECIPES;

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
  if (catalog.empty()) return NO_RECIPES;

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

// --- Sync with the home server ---------------------------------------------------------

std::string sha256File(const char *path) {
  File f = SD.open(path);
  if (!f) return "";
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts_ret(&ctx, 0);
  uint8_t buf[512];
  size_t n;
  while ((n = f.read(buf, sizeof(buf))) > 0) mbedtls_sha256_update_ret(&ctx, buf, n);
  uint8_t digest[32];
  mbedtls_sha256_finish_ret(&ctx, digest);
  mbedtls_sha256_free(&ctx);

  char hex[65];
  for (int i = 0; i < 32; ++i) snprintf(hex + 2 * i, 3, "%02x", digest[i]);
  return hex;
}

// Hashes of the recipe files on the card that sync manages. Files with other names (copied
// by hand) are left alone: they are neither compared nor removed.
std::map<std::string, std::string> localRecipes() {
  std::map<std::string, std::string> local;
  File dir = SD.open(RECIPES_DIR);
  if (!dir || !dir.isDirectory()) return local;
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    const std::string name = f.name();
    const String path = f.path();
    const bool managed = !f.isDirectory() && isSafeRecipeFile(name);
    f.close();
    if (managed) local[name] = sha256File(path.c_str());
  }
  return local;
}

bool fetchIndex(const std::string &server, std::string &out) {
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  if (!http.begin((server + "/index.txt").c_str())) return false;
  const int code = http.GET();
  const int size = http.getSize();
  const bool ok = code == HTTP_CODE_OK && size >= 0 && size <= MAX_INDEX_BYTES;
  if (ok) {
    const String body = http.getString();
    out.assign(body.c_str(), body.length());
  }
  http.end();
  return ok;
}

// Downloads to a temp file, checks size and hash, and only then replaces the recipe, so a
// failed or tampered download never leaves a broken recipe on the card.
bool downloadRecipe(const std::string &server, const RemoteFile &file) {
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  if (!http.begin((server + "/recipes/" + file.name).c_str())) return false;
  const int code = http.GET();
  const int size = http.getSize();
  bool ok = code == HTTP_CODE_OK && size >= 0 && size_t(size) <= MAX_RECIPE_BYTES;
  if (ok) {
    File tmp = SD.open(DOWNLOAD_TMP, FILE_WRITE);
    ok = tmp && http.writeToStream(&tmp) == size;
    tmp.close();
  }
  http.end();

  const std::string target = std::string(RECIPES_DIR) + "/" + file.name;
  ok = ok && sha256File(DOWNLOAD_TMP) == file.sha256;
  if (ok) {
    if (SD.exists(target.c_str())) SD.remove(target.c_str());  // FAT cannot rename over a file
    ok = SD.rename(DOWNLOAD_TMP, target.c_str());
  }
  if (SD.exists(DOWNLOAD_TMP)) SD.remove(DOWNLOAD_TMP);
  return ok;
}

// Prints a config value so stray quotes, spaces or invisible characters are obvious.
void logQuoted(const char *label, const std::string &value) {
  Serial.printf("%s: \"%s\" (%u bytes)", label, value.c_str(), unsigned(value.size()));
  for (unsigned char c : value) {
    if (c < 0x20 || c == 0x7F) Serial.printf(" [control 0x%02X]", c);
  }
  Serial.println();
}

volatile int wifiDisconnectReason = 0;

// Connects to Wi-Fi. Returns "" on success, or the reason in words for the screen.
std::string connectWifi(const Config &config) {
  logQuoted("Wi-Fi SSID", config.ssid);
  Serial.printf("Wi-Fi password: %u characters\n", unsigned(config.password.size()));

  wifiDisconnectReason = 0;
  const wifi_event_id_t handler = WiFi.onEvent(
      [](WiFiEvent_t, WiFiEventInfo_t info) {
        wifiDisconnectReason = info.wifi_sta_disconnected.reason;
        Serial.printf("Wi-Fi disconnected, reason %d\n", wifiDisconnectReason);
      },
      ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  WiFi.mode(WIFI_STA);
  WiFi.begin(config.ssid.c_str(), config.password.c_str());
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) delay(200);
  WiFi.removeEvent(handler);

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Wi-Fi connected: IP %s, RSSI %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    return "";
  }
  const int reason = wifiDisconnectReason;
  Serial.printf("Wi-Fi failed: status %d, last reason %d\n", int(WiFi.status()), reason);

  switch (reason) {
    case WIFI_REASON_NO_AP_FOUND: {
      // Show what the radio can see, to spot a typo or a 5 GHz-only network. The pending
      // connection attempt must stop first, or the scan fails with WIFI_SCAN_FAILED (-2).
      WiFi.disconnect();
      const int count = WiFi.scanNetworks();
      Serial.printf("Visible networks (%d):\n", count);
      for (int i = 0; i < count && i < 15; ++i) {
        Serial.printf("  \"%s\" ch %d, %d dBm\n", WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i));
      }
      return "Nie widzę sieci \"" + config.ssid + "\"";
    }
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_AUTH_EXPIRE:
      return "Złe hasło do Wi-Fi";
    case 0:
      return "Wi-Fi: brak odpowiedzi";
    default:
      return "Wi-Fi: błąd " + std::to_string(reason);
  }
}

// The part of a sync that needs a connection. Returns the line to show on screen.
std::string syncOverWifi(const std::string &server) {
  std::string indexText;
  std::vector<RemoteFile> remote;
  if (!fetchIndex(server, indexText)) return "Serwer nie odpowiada";
  if (!parseIndex(indexText, remote)) return "Serwer zwrócił błędną listę";

  const SyncPlan plan = planSync(remote, localRecipes());
  int downloaded = 0, failed = 0;
  for (const RemoteFile &file : plan.download) downloadRecipe(server, file) ? ++downloaded : ++failed;
  for (const std::string &name : plan.remove) SD.remove((std::string(RECIPES_DIR) + "/" + name).c_str());

  if (!downloaded && !failed && plan.remove.empty()) return "Przepisy są aktualne";
  std::string result = "Pobrano " + std::to_string(downloaded) + ", usunięto " + std::to_string(plan.remove.size());
  if (failed) result += ", błędy: " + std::to_string(failed);
  return result;
}

// Runs one sync and returns the line to show on screen.
std::string syncRecipes() {
  std::string text;
  if (!sdReady) return "Brak karty SD";
  if (!readFile(CONFIG_PATH, text)) return "Brak pliku /config.txt na karcie";
  const Config config = parseConfig(text);
  if (config.ssid.empty() || config.server.empty()) return "Uzupełnij wifi_ssid i server w /config.txt";

  showMessage("Synchronizacja…");
  SD.mkdir(RECIPES_DIR);
  std::string result = connectWifi(config);
  if (result.empty()) result = syncOverWifi(config.server);

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);  // the radio is the biggest battery drain
  Serial.printf("Sync: %s\n", result.c_str());
  return result;
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

// (Re)reads the card and shows the tag list, or the reason there is nothing to show.
void showCatalog() {
  screen = Screen::Tags;
  if (const char *error = loadCatalog()) {
    Serial.println(error);
    showMessage(error);
  } else {
    draw(true);
  }
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

  sdReady = SD.begin(SD_CS, spiBus, 10000000);
  showCatalog();

  keyEvents = xQueueCreate(16, sizeof(KeyEvent));
  xTaskCreate(keyTask, "keys", 2048, nullptr, 2, nullptr);
}

void loop() {
  KeyEvent e;
  xQueueReceive(keyEvents, &e, portMAX_DELAY);

  // Apply every queued press, then draw once: fast presses skip intermediate refreshes.
  // Changing screens uses a full refresh to clear e-ink ghosting; moving within one is partial.
  // A long right press syncs from any screen, including "no recipes yet".
  const Screen beforeScreen = screen;
  const size_t before[] = {tagSel, recipeSel, currentPage};
  bool screenChanged = false;
  bool sync = false;
  do {
    if (e.press == Press::Long && e.pin == KEY_RIGHT) {
      sync = true;
    } else if (!catalog.empty()) {
      screenChanged |= handle(e);
    }
  } while (xQueueReceive(keyEvents, &e, 0) == pdTRUE);

  if (sync) {
    showMessage(syncRecipes().c_str());
    delay(2000);  // leave the result readable before the tag list replaces it
    showCatalog();
    xQueueReset(keyEvents);  // presses made during the sync were aimed at nothing
    return;
  }
  if (catalog.empty()) return;

  const bool moved = before[0] != tagSel || before[1] != recipeSel || before[2] != currentPage;
  if (screenChanged || screen != beforeScreen) {
    draw(true);
  } else if (moved) {
    draw(false);
  }
}
