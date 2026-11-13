#ifndef WORD_STORE_H
#define WORD_STORE_H

#include <cstddef>
#include <string>

#include "Trie.h"

/** Summary of a load operation. */
struct LoadReport {
    bool fileFound = false;
    std::size_t loaded = 0;      // new words inserted
    std::size_t duplicates = 0;  // words skipped because they already existed
    std::size_t invalid = 0;     // lines that could not be parsed / validated
};

/**
 * Reads and writes the word list on disk.
 *
 * File format (UTF-8/ASCII text, one entry per line):
 *     word            -> frequency 1
 *     word,42         -> frequency 42
 *     # comment       -> ignored, as are blank lines
 */
class WordStore {
public:
    /** Load `path` into `trie`. Time O(total chars in file). */
    static LoadReport load(const std::string& path, Trie& trie);

    /**
     * Save every word as "word,frequency", alphabetically. The data is written
     * to a temporary file first and then renamed over the target, so a crash
     * mid-write cannot corrupt the existing list. Time O(total chars).
     */
    static bool save(const std::string& path, const Trie& trie,
                     std::string* error = nullptr);
};

#endif  // WORD_STORE_H
