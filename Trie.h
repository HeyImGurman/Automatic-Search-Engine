#ifndef TRIE_H
#define TRIE_H

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

/**
 * A single node of the prefix tree.
 *
 * Children are kept in an ordered std::map so that a depth-first traversal
 * visits words in alphabetical order and each node only pays for the
 * characters that actually occur (a fixed 26-slot array would waste memory
 * and could not hold digits, hyphens or apostrophes).
 */
struct TrieNode {
    std::map<char, std::unique_ptr<TrieNode>> children;
    bool isEndOfWord = false;   // true if a complete word ends at this node
    std::size_t frequency = 0;  // usage weight, used to rank suggestions
};

/** One autocomplete candidate. */
struct Suggestion {
    std::string word;
    std::size_t frequency = 0;
};

/** The top-N suggestions plus how many words matched in total. */
struct SuggestionResult {
    std::vector<Suggestion> items;
    std::size_t totalMatches = 0;
};

/**
 * Prefix tree (trie) supporting insertion, deletion, exact lookup and ranked
 * prefix completion. All input is normalised (trimmed + lower-cased), so every
 * operation is case-insensitive.
 *
 * Accepted characters: ASCII letters, digits, single spaces, '-' and '\''.
 *
 * Notation used in complexity comments:
 *   m = length of the word / prefix,  N = number of stored words,
 *   S = number of words under a prefix, L = suggestion limit.
 */
class Trie {
public:
    enum class InsertResult { Inserted, AlreadyExists, Invalid };

    static constexpr std::size_t kMaxWordLength = 64;

    Trie();

    /**
     * Trim, lower-case and validate a word or prefix.
     * Returns std::nullopt if it is empty, too long or has illegal characters.
     * Time O(m), space O(m).
     */
    static std::optional<std::string> normalize(const std::string& raw);

    /**
     * Insert a word. Duplicates are rejected (existing frequency is kept).
     * Time O(m log sigma) where sigma <= 39 is the alphabet size (effectively
     * O(m)). Space O(m) in the worst case (m new nodes).
     */
    InsertResult insert(const std::string& word, std::size_t frequency = 1);

    /**
     * Remove a word and prune nodes that are no longer needed, while keeping
     * other words that share the prefix (deleting "app" keeps "apple").
     * Returns false if the word was not stored. Time O(m log sigma), space
     * O(m) for the recursion stack.
     */
    bool remove(const std::string& word);

    /** Exact-match lookup. Time O(m log sigma), space O(1) extra. */
    bool contains(const std::string& word) const;

    /** Usage weight of a stored word, or 0 if it is not stored. O(m). */
    std::size_t frequencyOf(const std::string& word) const;

    /** Bump a stored word's weight by one (e.g. when a user picks it). O(m). */
    bool incrementFrequency(const std::string& word);

    /**
     * Up to `limit` completions of `prefix`, ordered by frequency (highest
     * first) and then alphabetically. The prefix itself is included if it is
     * a stored word.
     *
     * Time  O(m log sigma)  to reach the prefix node
     *     + O(total chars in the subtree) for the DFS
     *     + O(S log L)      for partial_sort of the S candidates.
     * Space O(S * avg word length) for the candidate list.
     */
    SuggestionResult suggest(const std::string& prefix, std::size_t limit) const;

    /** Every stored word in alphabetical order. O(total chars in the trie). */
    std::vector<Suggestion> allWords() const;

    std::size_t size() const { return wordCount_; }
    std::size_t nodeCount() const { return nodeCount_; }  // excludes the root
    bool empty() const { return wordCount_ == 0; }

private:
    const TrieNode* findNode(const std::string& key) const;
    TrieNode* findNode(const std::string& key);
    static void collect(const TrieNode* node, std::string& buffer,
                        std::vector<Suggestion>& out);
    bool removeHelper(TrieNode* node, const std::string& key,
                      std::size_t depth, bool& removed);

    std::unique_ptr<TrieNode> root_;
    std::size_t wordCount_ = 0;
    std::size_t nodeCount_ = 0;
};

#endif  // TRIE_H
