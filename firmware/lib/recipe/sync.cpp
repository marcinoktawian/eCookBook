#include "sync.h"

#include <sstream>

namespace {

constexpr size_t MAX_INDEX_ENTRIES = 1000;
constexpr size_t MAX_NAME_LENGTH = 80;

std::string trim(const std::string &s) {
  const size_t begin = s.find_first_not_of(" \t\r");
  if (begin == std::string::npos) return "";
  const size_t end = s.find_last_not_of(" \t\r");
  return s.substr(begin, end - begin + 1);
}

bool isLowerHex(const std::string &s) {
  for (char c : s) {
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  }
  return true;
}

}  // namespace

Config parseConfig(const std::string &text) {
  Config config;
  // Windows Notepad may start the file with a UTF-8 BOM, which would hide the first key.
  const bool bom = text.compare(0, 3, "\xEF\xBB\xBF") == 0;
  std::istringstream in(bom ? text.substr(3) : text);
  std::string line;
  while (std::getline(in, line)) {
    line = trim(line);
    if (line.empty() || line[0] == '#') continue;
    const size_t eq = line.find('=');
    if (eq == std::string::npos) continue;
    const std::string key = trim(line.substr(0, eq));
    const std::string value = trim(line.substr(eq + 1));  // passwords may contain '='
    if (key == "wifi_ssid") config.ssid = value;
    if (key == "wifi_password") config.password = value;
    if (key == "server") config.server = value;
  }
  while (!config.server.empty() && config.server.back() == '/') config.server.pop_back();
  return config;
}

bool isSafeRecipeFile(const std::string &name) {
  const std::string ext = ".md";
  if (name.size() <= ext.size() || name.size() > MAX_NAME_LENGTH) return false;
  if (name.compare(name.size() - ext.size(), ext.size(), ext) != 0) return false;
  const std::string stem = name.substr(0, name.size() - ext.size());
  if (stem.front() == '-' || stem.back() == '-') return false;
  for (char c : stem) {
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
  }
  return true;
}

bool parseIndex(const std::string &text, std::vector<RemoteFile> &out) {
  out.clear();
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    line = trim(line);
    if (line.empty()) continue;
    const size_t space = line.find(' ');
    if (space != 64) return false;
    std::string name = trim(line.substr(space));
    if (!name.empty() && name[0] == '*') name.erase(0, 1);  // sha256sum binary-mode marker
    RemoteFile file{name, line.substr(0, 64)};
    if (!isLowerHex(file.sha256) || !isSafeRecipeFile(file.name)) return false;
    if (out.size() == MAX_INDEX_ENTRIES) return false;
    out.push_back(file);
  }
  return true;
}

SyncPlan planSync(const std::vector<RemoteFile> &remote, const std::map<std::string, std::string> &local) {
  SyncPlan plan;
  std::map<std::string, bool> onServer;
  for (const RemoteFile &file : remote) {
    onServer[file.name] = true;
    const auto it = local.find(file.name);
    if (it == local.end() || it->second != file.sha256) plan.download.push_back(file);
  }
  if (remote.empty()) return plan;
  for (const auto &kv : local) {
    if (!onServer.count(kv.first)) plan.remove.push_back(kv.first);
  }
  return plan;
}
