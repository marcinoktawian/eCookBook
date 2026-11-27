#include "catalog.h"

#include <algorithm>
#include <map>
#include <stdint.h>

namespace {

std::vector<uint32_t> codePoints(const std::string &s) {
  std::vector<uint32_t> out;
  for (size_t i = 0; i < s.size();) {
    const unsigned char c = s[i];
    const int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    uint32_t cp = extra ? c & (0x3F >> extra) : c;
    for (int k = 1; k <= extra && i + k < s.size(); ++k) cp = (cp << 6) | (s[i + k] & 0x3F);
    out.push_back(cp);
    i += extra + 1;
  }
  return out;
}

uint32_t toLower(uint32_t c) {
  if (c >= 'A' && c <= 'Z') return c + 32;
  switch (c) {
    case 0x104: case 0x106: case 0x118: case 0x141: case 0x143:  // Ą Ć Ę Ł Ń
    case 0x15A: case 0x179: case 0x17B:                          // Ś Ź Ż
      return c + 1;
    case 0xD3:  // Ó
      return 0xF3;
  }
  return c;
}

constexpr uint32_t POLISH_ORDER[] = {'a', 0x105, 'b', 'c', 0x107, 'd', 'e', 0x119, 'f', 'g',
                                     'h', 'i',   'j', 'k', 'l',   0x142, 'm', 'n', 0x144, 'o',
                                     0xF3, 'p',  'q', 'r', 's',   0x15B, 't', 'u', 'v',  'w',
                                     'x', 'y',   'z', 0x17A, 0x17C};

// Letters sort by their place in the Polish alphabet; anything else keeps its code point,
// with ASCII punctuation and digits before letters.
uint32_t sortKey(uint32_t c) {
  c = toLower(c);
  for (size_t i = 0; i < sizeof(POLISH_ORDER) / sizeof(POLISH_ORDER[0]); ++i) {
    if (POLISH_ORDER[i] == c) return 0x10000 + i;
  }
  return c < 'a' ? c : 0x20000 + c;
}

}  // namespace

bool polishLess(const std::string &a, const std::string &b) {
  const std::vector<uint32_t> ca = codePoints(a), cb = codePoints(b);
  for (size_t i = 0; i < ca.size() && i < cb.size(); ++i) {
    const uint32_t ka = sortKey(ca[i]), kb = sortKey(cb[i]);
    if (ka != kb) return ka < kb;
  }
  if (ca.size() != cb.size()) return ca.size() < cb.size();
  return a < b;  // same letters, different case: keep a stable order
}

std::vector<TagEntry> collectTags(const std::vector<RecipeInfo> &recipes) {
  std::map<std::string, size_t> counts;
  for (const RecipeInfo &r : recipes) {
    for (const std::string &tag : r.tags) ++counts[tag];
  }
  std::vector<TagEntry> tags;
  for (const auto &kv : counts) tags.push_back({kv.first, kv.second});
  std::sort(tags.begin(), tags.end(),
            [](const TagEntry &x, const TagEntry &y) { return polishLess(x.name, y.name); });
  return tags;
}

std::vector<size_t> recipesWithTag(const std::vector<RecipeInfo> &recipes, const std::string &tag) {
  std::vector<size_t> out;
  for (size_t i = 0; i < recipes.size(); ++i) {
    const auto &tags = recipes[i].tags;
    if (tag.empty() || std::find(tags.begin(), tags.end(), tag) != tags.end()) out.push_back(i);
  }
  std::sort(out.begin(), out.end(),
            [&](size_t x, size_t y) { return polishLess(recipes[x].title, recipes[y].title); });
  return out;
}

std::vector<Section> findSections(const std::vector<Page> &pages) {
  std::vector<Section> sections;
  for (size_t p = 0; p < pages.size(); ++p) {
    for (const Line &line : pages[p]) {
      if (line.kind == Block::Heading && line.first) sections.push_back({p, line.text});
    }
  }
  return sections;
}

int sectionOfPage(const std::vector<Section> &sections, size_t page) {
  int found = -1;
  for (size_t i = 0; i < sections.size() && sections[i].page <= page; ++i) found = int(i);
  return found;
}

size_t nextSectionPage(const std::vector<Section> &sections, size_t page) {
  if (sections.empty()) return page;
  const size_t next = size_t(sectionOfPage(sections, page) + 1);
  return next < sections.size() ? sections[next].page : sections[0].page;
}

size_t toggleIngredients(const std::vector<Section> &sections, size_t page, int &returnPage) {
  if (sections.empty()) return page;
  if (sectionOfPage(sections, page) > 0) {
    returnPage = int(page);
    return sections[0].page;
  }
  if (returnPage >= 0) {
    const size_t target = size_t(returnPage);
    returnPage = -1;
    return target;
  }
  return nextSectionPage(sections, page);
}
