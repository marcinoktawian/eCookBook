#pragma once

#include <functional>
#include <string>
#include <vector>

// A recipe file is Markdown with a YAML front matter header (see docs/vision.md).
// Only the subset the device renders is parsed: headings, bullet items and paragraphs.
struct Block {
  enum Kind { Heading, Bullet, Text };
  Kind kind;
  std::string text;
};

struct Recipe {
  std::string title;
  std::vector<std::string> tags;  // from `tags: [a, b]` (inline list only)
  std::vector<Block> blocks;
};

Recipe parseRecipe(const std::string &markdown);

// One wrapped line on screen. `first` marks the first line of its block (where a bullet is drawn).
struct Line {
  Block::Kind kind;
  std::string text;
  bool first;
};
using Page = std::vector<Line>;

// Width in pixels of `text` drawn in the font used for `kind`.
using Measure = std::function<int(Block::Kind kind, const std::string &text)>;

std::vector<std::string> wrapText(const std::string &text, int widthPx, Block::Kind kind,
                                  const Measure &measure);

// Lays blocks out into fixed-height pages. Bullet lines are `bulletIndentPx` narrower.
// A heading never ends a page, and a page never starts with a blank spacer line.
std::vector<Page> paginate(const std::vector<Block> &blocks, int widthPx, int bulletIndentPx,
                           size_t linesPerPage, const Measure &measure);
