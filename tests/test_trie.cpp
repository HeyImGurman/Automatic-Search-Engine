#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "../Trie.h"
#include "../WordStore.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++checks;                                                            \
        if (!(cond)) {                                                       \
            ++failures;                                                      \
            std::cerr << "FAIL line " << __LINE__ << ": " #cond << "\n";     \
        }                                                                    \
    } while (0)

static void testInsertAndLookup() {
    Trie t;
    CHECK(t.insert("Apple") == Trie::InsertResult::Inserted);
    CHECK(t.contains("apple"));
    CHECK(t.contains("  APPLE "));
    CHECK(!t.contains("app"));              // prefix is not a word
    CHECK(t.insert("apple") == Trie::InsertResult::AlreadyExists);
    CHECK(t.size() == 1);
}

static void testInvalidInput() {
    Trie t;
    CHECK(t.insert("") == Trie::InsertResult::Invalid);
    CHECK(t.insert("   ") == Trie::InsertResult::Invalid);
    CHECK(t.insert("he@llo") == Trie::InsertResult::Invalid);
    CHECK(t.insert(std::string(Trie::kMaxWordLength + 1, 'a')) == Trie::InsertResult::Invalid);
    CHECK(t.insert(std::string(Trie::kMaxWordLength, 'a')) == Trie::InsertResult::Inserted);
    CHECK(t.size() == 1);
    CHECK(t.suggest("", 5).items.empty());
    CHECK(!Trie::normalize("a\tb").has_value());
    CHECK(Trie::normalize("  New   York ").value() == "new york");
}

static void testSuggestions() {
    Trie t;
    for (const char* w : {"apple", "application", "apply", "appreciate", "banana", "app"})
        t.insert(w);

    auto r = t.suggest("APP", 10);
    CHECK(r.totalMatches == 5);
    CHECK(r.items.size() == 5);
    CHECK(r.items[0].word == "app");        // equal frequency -> alphabetical
    CHECK(r.items[1].word == "apple");

    auto limited = t.suggest("app", 2);
    CHECK(limited.items.size() == 2);
    CHECK(limited.totalMatches == 5);

    CHECK(t.suggest("zzz", 5).items.empty());
    CHECK(t.suggest("zzz", 5).totalMatches == 0);
    CHECK(t.suggest("banana", 5).items.size() == 1);
}

static void testFrequencyRanking() {
    Trie t;
    t.insert("apply");
    t.insert("apple", 1);
    t.insert("application", 10);
    CHECK(t.suggest("app", 3).items[0].word == "application");
    CHECK(t.incrementFrequency("apply"));
    CHECK(t.incrementFrequency("apply"));
    CHECK(t.frequencyOf("apply") == 3);
    CHECK(t.suggest("app", 3).items[1].word == "apply");
    CHECK(!t.incrementFrequency("missing"));
}

static void testRemove() {
    Trie t;
    t.insert("app");
    t.insert("apple");
    const std::size_t nodesBoth = t.nodeCount();

    CHECK(t.remove("app"));                 // keeps "apple"
    CHECK(!t.contains("app"));
    CHECK(t.contains("apple"));
    CHECK(t.nodeCount() == nodesBoth);      // no node can be pruned yet

    CHECK(t.remove("APPLE"));
    CHECK(t.size() == 0);
    CHECK(t.nodeCount() == 0);              // everything pruned
    CHECK(!t.remove("apple"));
    CHECK(!t.remove(""));

    t.insert("apple");
    t.insert("apply");
    CHECK(t.remove("apple"));
    CHECK(t.contains("apply"));
    CHECK(t.nodeCount() == 5);              // a-p-p-l-y
}

static void testPersistence() {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "trie_test_data";
    fs::remove_all(dir);
    const std::string path = (dir / "words.txt").string();

    Trie t;
    t.insert("alpha", 3);
    t.insert("beta");
    CHECK(WordStore::save(path, t));

    Trie loaded;
    const LoadReport report = WordStore::load(path, loaded);
    CHECK(report.fileFound);
    CHECK(report.loaded == 2);
    CHECK(loaded.frequencyOf("alpha") == 3);
    CHECK(loaded.contains("beta"));

    {   // messy hand-edited file: BOM, comments, CRLF, duplicates, bad lines
        std::ofstream out(path, std::ios::trunc);
        out << "\xEF\xBB\xBF# comment\r\nHello\r\nhello,5\r\n\r\nbad!word\r\nworld,abc\r\nworld,7\r\n";
    }
    Trie messy;
    const LoadReport r2 = WordStore::load(path, messy);
    CHECK(r2.loaded == 2);
    CHECK(r2.duplicates == 1);
    CHECK(r2.invalid == 2);
    CHECK(messy.frequencyOf("world") == 7);

    Trie none;
    CHECK(!WordStore::load((dir / "missing.txt").string(), none).fileFound);
    fs::remove_all(dir);
}

int main() {
    testInsertAndLookup();
    testInvalidInput();
    testSuggestions();
    testFrequencyRanking();
    testRemove();
    testPersistence();
    std::cout << (checks - failures) << "/" << checks << " checks passed\n";
    return failures == 0 ? 0 : 1;
}
