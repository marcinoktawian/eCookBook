#include <unity.h>

#include "catalog.h"

void setUp() {}
void tearDown() {}

void test_parses_inline_tag_list() {
  const Recipe r = parseRecipe("---\ntitle: X\ntags: [zupy, \"mięso\", polskie ]\n---\n");
  TEST_ASSERT_EQUAL(3, r.tags.size());
  TEST_ASSERT_EQUAL_STRING("zupy", r.tags[0].c_str());
  TEST_ASSERT_EQUAL_STRING("mięso", r.tags[1].c_str());
  TEST_ASSERT_EQUAL_STRING("polskie", r.tags[2].c_str());
}

void test_polish_order_puts_diacritics_after_their_base_letter() {
  TEST_ASSERT_TRUE(polishLess("lody", "łazanki"));    // l < ł
  TEST_ASSERT_TRUE(polishLess("łazanki", "makaron")); // ł < m
  TEST_ASSERT_TRUE(polishLess("zupa", "żurek"));      // z < ż
  TEST_ASSERT_TRUE(polishLess("Żeberka", "żurek"));   // case-insensitive
  TEST_ASSERT_TRUE(polishLess("ser", "śledź"));       // s < ś
  TEST_ASSERT_FALSE(polishLess("żurek", "zupa"));
}

void test_collects_tags_with_counts_in_polish_order() {
  const std::vector<RecipeInfo> recipes = {
      {"Żurek", {"zupy", "mięso"}, "/a"}, {"Leczo", {"mięso"}, "/b"}, {"Łazanki", {"łatwe"}, "/c"}};
  const auto tags = collectTags(recipes);
  TEST_ASSERT_EQUAL(3, tags.size());
  TEST_ASSERT_EQUAL_STRING("łatwe", tags[0].name.c_str());
  TEST_ASSERT_EQUAL_STRING("mięso", tags[1].name.c_str());
  TEST_ASSERT_EQUAL(2, tags[1].count);
  TEST_ASSERT_EQUAL_STRING("zupy", tags[2].name.c_str());
}

void test_filters_by_tag_and_sorts_by_title() {
  const std::vector<RecipeInfo> recipes = {
      {"Żurek", {"zupy", "mięso"}, "/a"}, {"Leczo", {"mięso"}, "/b"}, {"Łazanki", {}, "/c"}};
  const auto meat = recipesWithTag(recipes, "mięso");
  TEST_ASSERT_EQUAL(2, meat.size());
  TEST_ASSERT_EQUAL(1, meat[0]);  // Leczo
  TEST_ASSERT_EQUAL(0, meat[1]);  // Żurek
  const auto all = recipesWithTag(recipes, "");
  TEST_ASSERT_EQUAL(3, all.size());
  TEST_ASSERT_EQUAL(2, all[1]);  // Łazanki between Leczo and Żurek
}

void test_green_key_cycles_between_sections() {
  const std::vector<Page> pages = {
      {{Block::Heading, "Składniki", true}, {Block::Bullet, "a", true}},
      {{Block::Bullet, "b", true}},
      {{Block::Heading, "Przygotowanie", true}, {Block::Text, "c", true}},
      {{Block::Text, "d", true}}};
  const auto sections = findSections(pages);
  TEST_ASSERT_EQUAL(2, sections.size());
  TEST_ASSERT_EQUAL(0, sectionOfPage(sections, 1));  // page 2 of "Składniki"
  TEST_ASSERT_EQUAL(1, sectionOfPage(sections, 3));
  TEST_ASSERT_EQUAL(2, nextSectionPage(sections, 0));
  TEST_ASSERT_EQUAL(2, nextSectionPage(sections, 1));
  TEST_ASSERT_EQUAL(0, nextSectionPage(sections, 3));  // wraps back to the ingredients
}

void test_green_key_does_nothing_without_headings() {
  const std::vector<Page> pages = {{{Block::Text, "a", true}}, {{Block::Text, "b", true}}};
  int returnPage = -1;
  TEST_ASSERT_EQUAL(1, toggleIngredients(findSections(pages), 1, returnPage));
}

void test_green_key_returns_to_the_exact_page_after_checking_ingredients() {
  // Ingredients on pages 0-1, preparation on pages 2-3.
  const std::vector<Section> sections = {{0, "Składniki"}, {2, "Przygotowanie"}};
  int returnPage = -1;
  TEST_ASSERT_EQUAL(0, toggleIngredients(sections, 3, returnPage));  // from 4/4 to ingredients
  TEST_ASSERT_EQUAL(3, returnPage);
  TEST_ASSERT_EQUAL(3, toggleIngredients(sections, 0, returnPage));  // back to 4/4, not 3/4
  TEST_ASSERT_EQUAL(-1, returnPage);
}

void test_green_key_returns_even_after_flipping_within_ingredients() {
  const std::vector<Section> sections = {{0, "Składniki"}, {2, "Przygotowanie"}};
  int returnPage = -1;
  toggleIngredients(sections, 3, returnPage);
  TEST_ASSERT_EQUAL(3, toggleIngredients(sections, 1, returnPage));  // user flipped to 2/4 first
}

void test_green_key_from_ingredients_without_memory_goes_to_preparation() {
  const std::vector<Section> sections = {{0, "Składniki"}, {2, "Przygotowanie"}};
  int returnPage = -1;
  TEST_ASSERT_EQUAL(2, toggleIngredients(sections, 0, returnPage));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_parses_inline_tag_list);
  RUN_TEST(test_polish_order_puts_diacritics_after_their_base_letter);
  RUN_TEST(test_collects_tags_with_counts_in_polish_order);
  RUN_TEST(test_filters_by_tag_and_sorts_by_title);
  RUN_TEST(test_green_key_cycles_between_sections);
  RUN_TEST(test_green_key_does_nothing_without_headings);
  RUN_TEST(test_green_key_returns_to_the_exact_page_after_checking_ingredients);
  RUN_TEST(test_green_key_returns_even_after_flipping_within_ingredients);
  RUN_TEST(test_green_key_from_ingredients_without_memory_goes_to_preparation);
  return UNITY_END();
}
