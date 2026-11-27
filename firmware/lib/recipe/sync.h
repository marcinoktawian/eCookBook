#pragma once

#include <map>
#include <string>
#include <vector>

// Settings read from /config.txt on the SD card (key=value lines, # comments).
struct Config {
  std::string ssid;
  std::string password;
  std::string server;  // e.g. http://ecookbook.pi-nas.lan, no trailing slash
};

Config parseConfig(const std::string &text);

// One line of the server's /index.txt (`sha256sum` format).
struct RemoteFile {
  std::string name;    // e.g. zurek.md
  std::string sha256;  // 64 lowercase hex digits
};

// File names the device will write: lowercase slug + ".md", nothing that could leave /recipes.
bool isSafeRecipeFile(const std::string &name);

// Parses /index.txt. Returns false on any malformed line, so a broken response never
// drives deletions.
bool parseIndex(const std::string &text, std::vector<RemoteFile> &out);

struct SyncPlan {
  std::vector<RemoteFile> download;  // new or changed on the server
  std::vector<std::string> remove;   // on the card but no longer on the server
};

// `local` maps file name to the SHA-256 of the copy on the card. An empty server index
// never removes anything: that is far more likely a broken server than an empty cookbook.
SyncPlan planSync(const std::vector<RemoteFile> &remote, const std::map<std::string, std::string> &local);
