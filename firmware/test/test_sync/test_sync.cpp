#include <unity.h>

#include "sync.h"

static const std::string A(64, 'a');
static const std::string B(64, 'b');

void setUp() {}
void tearDown() {}

void test_parses_config_with_comments_and_equals_in_password() {
  const Config c = parseConfig(
      "# eCookBook\n"
      "wifi_ssid = Martin Router King\n"
      "wifi_password=pa=ss\n"
      "server=http://ecookbook.pi-nas.lan/\n");
  TEST_ASSERT_EQUAL_STRING("Martin Router King", c.ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("pa=ss", c.password.c_str());
  TEST_ASSERT_EQUAL_STRING("http://ecookbook.pi-nas.lan", c.server.c_str());
}

void test_config_ignores_utf8_bom_and_crlf() {
  const Config c = parseConfig("\xEF\xBB\xBFwifi_ssid=Dom\r\nserver=http://x\r\n");
  TEST_ASSERT_EQUAL_STRING("Dom", c.ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("http://x", c.server.c_str());
}

void test_safe_names_cannot_leave_the_recipes_folder() {
  TEST_ASSERT_TRUE(isSafeRecipeFile("zurek.md"));
  TEST_ASSERT_TRUE(isSafeRecipeFile("placki-2.md"));
  TEST_ASSERT_FALSE(isSafeRecipeFile("../config.md"));
  TEST_ASSERT_FALSE(isSafeRecipeFile("a/b.md"));
  TEST_ASSERT_FALSE(isSafeRecipeFile(".md"));
  TEST_ASSERT_FALSE(isSafeRecipeFile("Zurek.md"));
  TEST_ASSERT_FALSE(isSafeRecipeFile("zurek.txt"));
  TEST_ASSERT_FALSE(isSafeRecipeFile("-x.md"));
}

void test_parses_sha256sum_index() {
  std::vector<RemoteFile> files;
  TEST_ASSERT_TRUE(parseIndex(A + "  zurek.md\n" + B + " *lazanki.md\n\n", files));
  TEST_ASSERT_EQUAL(2, files.size());
  TEST_ASSERT_EQUAL_STRING("zurek.md", files[0].name.c_str());
  TEST_ASSERT_EQUAL_STRING("lazanki.md", files[1].name.c_str());
  TEST_ASSERT_EQUAL_STRING(B.c_str(), files[1].sha256.c_str());
}

void test_rejects_whole_index_on_any_bad_line() {
  std::vector<RemoteFile> files;
  TEST_ASSERT_FALSE(parseIndex(A + "  zurek.md\n<html>error</html>\n", files));
  TEST_ASSERT_FALSE(parseIndex(A + "  ../../config.md\n", files));
  TEST_ASSERT_FALSE(parseIndex(std::string(64, 'G') + "  zurek.md\n", files));
}

void test_plans_downloads_and_removals() {
  const std::vector<RemoteFile> remote = {{"zurek.md", A}, {"lazanki.md", B}, {"nowy.md", A}};
  const std::map<std::string, std::string> local = {{"zurek.md", A}, {"lazanki.md", A}, {"stary.md", B}};
  const SyncPlan plan = planSync(remote, local);
  TEST_ASSERT_EQUAL(2, plan.download.size());  // lazanki changed, nowy is new
  TEST_ASSERT_EQUAL_STRING("lazanki.md", plan.download[0].name.c_str());
  TEST_ASSERT_EQUAL_STRING("nowy.md", plan.download[1].name.c_str());
  TEST_ASSERT_EQUAL(1, plan.remove.size());
  TEST_ASSERT_EQUAL_STRING("stary.md", plan.remove[0].c_str());
}

void test_empty_server_index_never_wipes_the_card() {
  const std::map<std::string, std::string> local = {{"zurek.md", A}};
  const SyncPlan plan = planSync({}, local);
  TEST_ASSERT_EQUAL(0, plan.download.size());
  TEST_ASSERT_EQUAL(0, plan.remove.size());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_parses_config_with_comments_and_equals_in_password);
  RUN_TEST(test_config_ignores_utf8_bom_and_crlf);
  RUN_TEST(test_safe_names_cannot_leave_the_recipes_folder);
  RUN_TEST(test_parses_sha256sum_index);
  RUN_TEST(test_rejects_whole_index_on_any_bad_line);
  RUN_TEST(test_plans_downloads_and_removals);
  RUN_TEST(test_empty_server_index_never_wipes_the_card);
  return UNITY_END();
}
