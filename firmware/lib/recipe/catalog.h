#pragma once

#include <string>
#include <vector>

#include "recipe.h"

// What the device keeps in memory about every recipe on the card; the body is read on open.
struct RecipeInfo {
  std::string title;
  std::vector<std::string> tags;
  std::string path;
};

struct TagEntry {
  std::string name;
  size_t count;
};

// Case-insensitive ordering in the Polish alphabet (a ą b c ć ... z ź ż).
bool polishLess(const std::string &a, const std::string &b);

// Every distinct tag with its recipe count, in Polish order.
std::vector<TagEntry> collectTags(const std::vector<RecipeInfo> &recipes);

// Indexes into `recipes` that carry `tag` (all of them when `tag` is empty), sorted by title.
std::vector<size_t> recipesWithTag(const std::vector<RecipeInfo> &recipes, const std::string &tag);

// A heading's position in a paginated recipe.
struct Section {
  size_t page;
  std::string name;
};

std::vector<Section> findSections(const std::vector<Page> &pages);

// Index into `sections` of the section that `page` belongs to, or -1 before the first heading.
int sectionOfPage(const std::vector<Section> &sections, size_t page);

// First page of the section after the one holding `page`, wrapping to the first section.
// Returns `page` unchanged when the recipe has no headings.
size_t nextSectionPage(const std::vector<Section> &sections, size_t page);

// Green key in a recipe. Outside the ingredients (the first section) it remembers `page` in
// `returnPage` and jumps to the ingredients; inside them it goes back to the remembered page,
// or to the next section when nothing is remembered. `returnPage` is -1 when empty.
size_t toggleIngredients(const std::vector<Section> &sections, size_t page, int &returnPage);
