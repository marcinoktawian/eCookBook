#include "recipe.h"

#include <sstream>

namespace {

std::string trim(const std::string &s) {
  const size_t begin = s.find_first_not_of(" \t\r");
  if (begin == std::string::npos) return "";
  const size_t end = s.find_last_not_of(" \t\r");
  return s.substr(begin, end - begin + 1);
}

bool startsWith(const std::string &s, const char *prefix) { return s.rfind(prefix, 0) == 0; }

std::string unquote(const std::string &s) {
  if (s.size() >= 2 && (s.front() == '"' || s.front() == '\'') && s.back() == s.front()) {
    return s.substr(1, s.size() - 2);
  }
  return s;
}

}  // namespace

Recipe parseRecipe(const std::string &markdown) {
  Recipe recipe;
  std::istringstream in(markdown);
  std::string raw;
  bool firstLine = true;
  bool inFrontMatter = false;

  while (std::getline(in, raw)) {
    const std::string line = trim(raw);
    if (firstLine) {
      firstLine = false;
      if (line == "---") {
        inFrontMatter = true;
        continue;
      }
    }
    if (inFrontMatter) {
      if (line == "---") {
        inFrontMatter = false;
      } else if (startsWith(line, "title:")) {
        recipe.title = unquote(trim(line.substr(6)));
      }
      continue;
    }
    if (line.empty()) continue;

    if (line[0] == '#') {
      recipe.blocks.push_back({Block::Heading, trim(line.substr(line.find_first_not_of('#')))});
    } else if (startsWith(line, "- ") || startsWith(line, "* ")) {
      recipe.blocks.push_back({Block::Bullet, trim(line.substr(2))});
    } else {
      recipe.blocks.push_back({Block::Text, line});
    }
  }
  return recipe;
}

std::vector<std::string> wrapText(const std::string &text, int widthPx, Block::Kind kind,
                                  const Measure &measure) {
  std::vector<std::string> lines;
  std::istringstream words(text);
  std::string word, current;
  while (words >> word) {
    const std::string candidate = current.empty() ? word : current + " " + word;
    // A single word wider than the line stays on its own line rather than being split.
    if (!current.empty() && measure(kind, candidate) > widthPx) {
      lines.push_back(current);
      current = word;
    } else {
      current = candidate;
    }
  }
  if (!current.empty() || lines.empty()) lines.push_back(current);
  return lines;
}

std::vector<Page> paginate(const std::vector<Block> &blocks, int widthPx, int bulletIndentPx,
                           size_t linesPerPage, const Measure &measure) {
  std::vector<Line> lines;
  for (const Block &block : blocks) {
    if (block.kind == Block::Heading && !lines.empty()) lines.push_back({Block::Text, "", false});
    const int width = block.kind == Block::Bullet ? widthPx - bulletIndentPx : widthPx;
    const std::vector<std::string> wrapped = wrapText(block.text, width, block.kind, measure);
    for (size_t i = 0; i < wrapped.size(); ++i) lines.push_back({block.kind, wrapped[i], i == 0});
  }

  std::vector<Page> pages(1);
  for (size_t i = 0; i < lines.size(); ++i) {
    const Line &line = lines[i];
    if (pages.back().size() == linesPerPage) pages.emplace_back();
    if (pages.back().empty() && line.text.empty()) continue;
    const bool lastSlot = pages.back().size() + 1 == linesPerPage;
    if (line.kind == Block::Heading && lastSlot && i + 1 < lines.size()) pages.emplace_back();
    pages.back().push_back(line);
  }
  return pages;
}
