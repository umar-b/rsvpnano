#include <unity.h>

#include <map>
#include <string>

#include "net/WifiCredentialStore.h"

using net::pickConnectionOrder;
using net::WifiCredentialStore;
using net::WifiScanEntry;
using net::WifiSlot;

namespace {

// In-memory store standing in for NVS Preferences. Strings and uints share one
// keyspace, mirroring how Preferences keys are unique per type in practice.
class MemoryStore : public net::WifiKeyValueStore {
 public:
  bool has(const char *key) const override {
    return strings_.count(key) != 0 || uints_.count(key) != 0;
  }
  std::string getString(const char *key, const std::string &fallback = "") const override {
    auto it = strings_.find(key);
    return it == strings_.end() ? fallback : it->second;
  }
  void putString(const char *key, const std::string &value) override { strings_[key] = value; }
  uint32_t getUInt(const char *key, uint32_t fallback = 0) const override {
    auto it = uints_.find(key);
    return it == uints_.end() ? fallback : it->second;
  }
  void putUInt(const char *key, uint32_t value) override { uints_[key] = value; }
  void remove(const char *key) override {
    strings_.erase(key);
    uints_.erase(key);
  }

  std::map<std::string, std::string> strings_;
  std::map<std::string, uint32_t> uints_;
};

const char *kLegacySsid = "wifi_ssid";
const char *kLegacyPass = "wifi_pass";

}  // namespace

void test_save_uses_first_free_slot() {
  MemoryStore mem;
  WifiCredentialStore store(mem);

  TEST_ASSERT_EQUAL_INT(0, store.save("home", "pw1"));
  TEST_ASSERT_EQUAL_INT(1, store.save("office", "pw2"));
  TEST_ASSERT_EQUAL_INT(2, store.count());
}

void test_save_existing_ssid_updates_in_place() {
  MemoryStore mem;
  WifiCredentialStore store(mem);
  store.save("home", "old");
  store.save("office", "x");
  TEST_ASSERT_EQUAL_INT(0, store.save("home", "new"));
  TEST_ASSERT_EQUAL_INT(2, store.count());
  TEST_ASSERT_EQUAL_STRING("new", store.findBySsid("home").password.c_str());
}

void test_save_replaces_least_recently_used_when_full() {
  MemoryStore mem;
  WifiCredentialStore store(mem);
  // Fill all five; each save bumps recency, so "a" is the oldest.
  store.save("a", "1");
  store.save("b", "2");
  store.save("c", "3");
  store.save("d", "4");
  store.save("e", "5");
  TEST_ASSERT_EQUAL_INT(5, store.count());

  // Touch "a" so "b" becomes the LRU.
  store.markUsed("a");
  const int slot = store.save("f", "6");
  TEST_ASSERT_EQUAL_INT(5, store.count());
  // "b" was evicted, not "a".
  TEST_ASSERT_EQUAL_INT(-1, store.findBySsid("b").index);
  TEST_ASSERT_TRUE(store.findBySsid("a").index >= 0);
  TEST_ASSERT_TRUE(store.findBySsid("f").index >= 0);
  TEST_ASSERT_EQUAL_INT(slot, store.findBySsid("f").index);
}

void test_forget_clears_slot() {
  MemoryStore mem;
  WifiCredentialStore store(mem);
  store.save("home", "pw");
  TEST_ASSERT_TRUE(store.forget("home"));
  TEST_ASSERT_EQUAL_INT(0, store.count());
  TEST_ASSERT_FALSE(store.forget("home"));
}

void test_list_skips_empty_slots() {
  MemoryStore mem;
  WifiCredentialStore store(mem);
  store.save("a", "1");
  store.save("b", "2");
  store.forget("a");
  auto slots = store.list();
  TEST_ASSERT_EQUAL_INT(1, static_cast<int>(slots.size()));
  TEST_ASSERT_EQUAL_STRING("b", slots[0].ssid.c_str());
}

void test_migrate_legacy_into_slot_and_clears_legacy_keys() {
  MemoryStore mem;
  mem.putString(kLegacySsid, "  home  ");  // whitespace trimmed on migrate.
  mem.putString(kLegacyPass, "secret");
  WifiCredentialStore store(mem);

  TEST_ASSERT_TRUE(store.migrateLegacy(kLegacySsid, kLegacyPass));
  TEST_ASSERT_EQUAL_INT(1, store.count());
  WifiSlot slot = store.findBySsid("home");
  TEST_ASSERT_EQUAL_STRING("secret", slot.password.c_str());
  // Legacy keys are gone afterwards.
  TEST_ASSERT_FALSE(mem.has(kLegacySsid));
  TEST_ASSERT_FALSE(mem.has(kLegacyPass));
  // Idempotent: second call does nothing.
  TEST_ASSERT_FALSE(store.migrateLegacy(kLegacySsid, kLegacyPass));
  TEST_ASSERT_EQUAL_INT(1, store.count());
}

void test_migrate_legacy_empty_ssid_just_clears() {
  MemoryStore mem;
  mem.putString(kLegacySsid, "   ");
  mem.putString(kLegacyPass, "x");
  WifiCredentialStore store(mem);
  TEST_ASSERT_FALSE(store.migrateLegacy(kLegacySsid, kLegacyPass));
  TEST_ASSERT_EQUAL_INT(0, store.count());
  TEST_ASSERT_FALSE(mem.has(kLegacySsid));
}

void test_migrate_legacy_no_keys_is_noop() {
  MemoryStore mem;
  WifiCredentialStore store(mem);
  TEST_ASSERT_FALSE(store.migrateLegacy(kLegacySsid, kLegacyPass));
  TEST_ASSERT_EQUAL_INT(0, store.count());
}

void test_pick_order_visible_strongest_first() {
  MemoryStore mem;
  WifiCredentialStore store(mem);
  store.save("a", "1");
  store.save("b", "2");
  store.save("c", "3");
  auto slots = store.list();

  std::vector<WifiScanEntry> scan = {{"c", -40}, {"a", -80}, {"b", -55}};
  auto order = pickConnectionOrder(slots, scan);
  TEST_ASSERT_EQUAL_INT(3, static_cast<int>(order.size()));
  // c (-40) strongest, then b (-55), then a (-80). Slot indices: a=0,b=1,c=2.
  TEST_ASSERT_EQUAL_INT(2, order[0]);
  TEST_ASSERT_EQUAL_INT(1, order[1]);
  TEST_ASSERT_EQUAL_INT(0, order[2]);
}

void test_pick_order_unseen_networks_come_after_visible() {
  MemoryStore mem;
  WifiCredentialStore store(mem);
  store.save("seen", "1");    // slot 0
  store.save("hidden", "2");  // slot 1, not in scan
  auto slots = store.list();

  std::vector<WifiScanEntry> scan = {{"seen", -70}};
  auto order = pickConnectionOrder(slots, scan);
  TEST_ASSERT_EQUAL_INT(2, static_cast<int>(order.size()));
  TEST_ASSERT_EQUAL_INT(0, order[0]);  // visible first
  TEST_ASSERT_EQUAL_INT(1, order[1]);  // unseen fallback
}

void test_pick_order_uses_strongest_rssi_for_duplicate_ssid() {
  MemoryStore mem;
  WifiCredentialStore store(mem);
  store.save("a", "1");  // slot 0
  store.save("b", "2");  // slot 1
  auto slots = store.list();

  // "a" seen twice; its strongest (-30) should beat b (-50).
  std::vector<WifiScanEntry> scan = {{"a", -90}, {"b", -50}, {"a", -30}};
  auto order = pickConnectionOrder(slots, scan);
  TEST_ASSERT_EQUAL_INT(0, order[0]);
  TEST_ASSERT_EQUAL_INT(1, order[1]);
}

void test_pick_order_empty_when_no_slots() {
  std::vector<WifiSlot> slots;
  std::vector<WifiScanEntry> scan = {{"x", -40}};
  auto order = pickConnectionOrder(slots, scan);
  TEST_ASSERT_EQUAL_INT(0, static_cast<int>(order.size()));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_save_uses_first_free_slot);
  RUN_TEST(test_save_existing_ssid_updates_in_place);
  RUN_TEST(test_save_replaces_least_recently_used_when_full);
  RUN_TEST(test_forget_clears_slot);
  RUN_TEST(test_list_skips_empty_slots);
  RUN_TEST(test_migrate_legacy_into_slot_and_clears_legacy_keys);
  RUN_TEST(test_migrate_legacy_empty_ssid_just_clears);
  RUN_TEST(test_migrate_legacy_no_keys_is_noop);
  RUN_TEST(test_pick_order_visible_strongest_first);
  RUN_TEST(test_pick_order_unseen_networks_come_after_visible);
  RUN_TEST(test_pick_order_uses_strongest_rssi_for_duplicate_ssid);
  RUN_TEST(test_pick_order_empty_when_no_slots);
  return UNITY_END();
}
