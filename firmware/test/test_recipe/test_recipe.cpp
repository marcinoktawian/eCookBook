#include <unity.h>

#include "recipe.h"

// 10 px per UTF-8 code point, so Polish letters count once like on the device.
static int measure(Block::Kind, const std::string &text) {
  int points = 0;
  for (unsigned char c : text) points += (c & 0xC0) != 0x80;
  return points * 10;
}

void setUp() {}
void tearDown() {}

void test_parses_front_matter_title_and_blocks() {
  const Recipe r = parseRecipe(
      "---\n"
      "title: \"Żurek\"\n"
      "tags: [zupy]\n"
      "---\n"
      "\n"
      "## Składniki\n"
      "- 500 ml zakwasu\n"
      "* 2 jajka\n"
      "1. Zagotuj wodę.\n");
  TEST_ASSERT_EQUAL_STRING("Żurek", r.title.c_str());
  TEST_ASSERT_EQUAL(4, r.blocks.size());
  TEST_ASSERT_EQUAL(Block::Heading, r.blocks[0].kind);
  TEST_ASSERT_EQUAL_STRING("Składniki", r.blocks[0].text.c_str());
  TEST_ASSERT_EQUAL(Block::Bullet, r.blocks[1].kind);
  TEST_ASSERT_EQUAL_STRING("500 ml zakwasu", r.blocks[1].text.c_str());
  TEST_ASSERT_EQUAL(Block::Bullet, r.blocks[2].kind);
  TEST_ASSERT_EQUAL(Block::Text, r.blocks[3].kind);
  TEST_ASSERT_EQUAL_STRING("1. Zagotuj wodę.", r.blocks[3].text.c_str());
}

void test_file_without_front_matter_has_no_title() {
  const Recipe r = parseRecipe("Tylko tekst\n");
  TEST_ASSERT_EQUAL_STRING("", r.title.c_str());
  TEST_ASSERT_EQUAL(1, r.blocks.size());
}

void test_wraps_on_words_counting_utf8_letters() {
  // "żółć gęś" is 8 code points (80 px) but 13 bytes; it must fit in 80 px.
  const auto lines = wrapText("żółć gęś jaźń", 80, Block::Text, measure);
  TEST_ASSERT_EQUAL(2, lines.size());
  TEST_ASSERT_EQUAL_STRING("żółć gęś", lines[0].c_str());
  TEST_ASSERT_EQUAL_STRING("jaźń", lines[1].c_str());
}

void test_heading_never_ends_a_page() {
  std::vector<Block> blocks = {{Block::Text, "a"}, {Block::Text, "b"}, {Block::Heading, "H"},
                               {Block::Text, "c"}};
  // Lines: a, b, spacer, H, c. With 4 lines per page, H would land in the last slot.
  const auto pages = paginate(blocks, 100, 0, 4, measure);
  TEST_ASSERT_EQUAL(2, pages.size());
  TEST_ASSERT_EQUAL(3, pages[0].size());
  TEST_ASSERT_EQUAL_STRING("H", pages[1][0].text.c_str());
}

void test_page_does_not_start_with_spacer() {
  std::vector<Block> blocks = {{Block::Text, "a"}, {Block::Text, "b"}, {Block::Heading, "H"}};
  // Lines: a, b, spacer, H. The spacer would open page 2, so it is dropped.
  const auto pages = paginate(blocks, 100, 0, 2, measure);
  TEST_ASSERT_EQUAL(2, pages.size());
  TEST_ASSERT_EQUAL_STRING("H", pages[1][0].text.c_str());
}

void test_bullet_lines_are_narrower_and_marked_first() {
  std::vector<Block> blocks = {{Block::Bullet, "aaa bbb"}};
  const auto pages = paginate(blocks, 70, 10, 10, measure);
  TEST_ASSERT_EQUAL(2, pages[0].size());
  TEST_ASSERT_TRUE(pages[0][0].first);
  TEST_ASSERT_FALSE(pages[0][1].first);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_parses_front_matter_title_and_blocks);
  RUN_TEST(test_file_without_front_matter_has_no_title);
  RUN_TEST(test_wraps_on_words_counting_utf8_letters);
  RUN_TEST(test_heading_never_ends_a_page);
  RUN_TEST(test_page_does_not_start_with_spacer);
  RUN_TEST(test_bullet_lines_are_narrower_and_marked_first);
  return UNITY_END();
}
