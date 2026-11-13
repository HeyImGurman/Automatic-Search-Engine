#include "Trie.h"

#include <algorithm>
#include <cctype>

Trie::Trie() : root_(std::make_unique<TrieNode>()) {}

std::optional<std::string> Trie::normalize(const std::string& raw) {
    std::size_t first = 0;
    std::size_t last = raw.size();
    while (first < last && std::isspace(static_cast<unsigned char>(raw[first]))) ++first;
    while (last > first && std::isspace(static_cast<unsigned char>(raw[last - 1]))) --last;
    if (first == last) return std::nullopt;

    std::string out;
    out.reserve(last - first);
    bool previousWasSpace = false;
    for (std::size_t i = first; i < last; ++i) {
        const unsigned char c = static_cast<unsigned char>(raw[i]);
        if (c == ' ') {
            if (!previousWasSpace) out.push_back(' ');  // collapse runs of spaces
            previousWasSpace = true;
            continue;
        }
        previousWasSpace = false;
        if (std::isalnum(c) || c == '-' || c == '\'') {
            out.push_back(static_cast<char>(std::tolower(c)));
        } else {
            return std::nullopt;  // tabs, punctuation, non-ASCII bytes, ...
        }
    }
    if (out.size() > kMaxWordLength) return std::nullopt;
    return out;
}

Trie::InsertResult Trie::insert(const std::string& word, std::size_t frequency) {
    const auto key = normalize(word);
    if (!key) return InsertResult::Invalid;

    TrieNode* node = root_.get();
    for (char c : *key) {
        auto& child = node->children[c];
        if (!child) {
            child = std::make_unique<TrieNode>();
            ++nodeCount_;
        }
        node = child.get();
    }
    if (node->isEndOfWord) return InsertResult::AlreadyExists;

    node->isEndOfWord = true;
    node->frequency = std::max<std::size_t>(1, frequency);
    ++wordCount_;
    return InsertResult::Inserted;
}

bool Trie::removeHelper(TrieNode* node, const std::string& key,
                        std::size_t depth, bool& removed) {
    // Returns true if `node` is now useless and its parent should erase it.
    if (depth == key.size()) {
        if (!node->isEndOfWord) return false;
        node->isEndOfWord = false;
        node->frequency = 0;
        removed = true;
        return node->children.empty();
    }
    const auto it = node->children.find(key[depth]);
    if (it == node->children.end()) return false;

    if (removeHelper(it->second.get(), key, depth + 1, removed)) {
        node->children.erase(it);
        --nodeCount_;
    }
    return removed && !node->isEndOfWord && node->children.empty();
}

bool Trie::remove(const std::string& word) {
    const auto key = normalize(word);
    if (!key) return false;
    bool removed = false;
    removeHelper(root_.get(), *key, 0, removed);
    if (removed) --wordCount_;
    return removed;
}

const TrieNode* Trie::findNode(const std::string& key) const {
    const TrieNode* node = root_.get();
    for (char c : key) {
        const auto it = node->children.find(c);
        if (it == node->children.end()) return nullptr;
        node = it->second.get();
    }
    return node;
}

TrieNode* Trie::findNode(const std::string& key) {
    return const_cast<TrieNode*>(static_cast<const Trie*>(this)->findNode(key));
}

bool Trie::contains(const std::string& word) const {
    const auto key = normalize(word);
    if (!key) return false;
    const TrieNode* node = findNode(*key);
    return node != nullptr && node->isEndOfWord;
}

std::size_t Trie::frequencyOf(const std::string& word) const {
    const auto key = normalize(word);
    if (!key) return 0;
    const TrieNode* node = findNode(*key);
    return (node != nullptr && node->isEndOfWord) ? node->frequency : 0;
}

bool Trie::incrementFrequency(const std::string& word) {
    const auto key = normalize(word);
    if (!key) return false;
    TrieNode* node = findNode(*key);
    if (node == nullptr || !node->isEndOfWord) return false;
    ++node->frequency;
    return true;
}

void Trie::collect(const TrieNode* node, std::string& buffer,
                   std::vector<Suggestion>& out) {
    if (node->isEndOfWord) out.push_back({buffer, node->frequency});
    for (const auto& entry : node->children) {
        const char ch = entry.first;
        const auto& child = entry.second;
        buffer.push_back(ch);
        collect(child.get(), buffer, out);
        buffer.pop_back();
    }
}

SuggestionResult Trie::suggest(const std::string& prefix, std::size_t limit) const {
    SuggestionResult result;
    const auto key = normalize(prefix);
    if (!key) return result;

    const TrieNode* start = findNode(*key);
    if (start == nullptr) return result;

    std::vector<Suggestion> candidates;
    std::string buffer = *key;
    collect(start, buffer, candidates);
    result.totalMatches = candidates.size();

    const auto better = [](const Suggestion& a, const Suggestion& b) {
        if (a.frequency != b.frequency) return a.frequency > b.frequency;
        return a.word < b.word;
    };
    const std::size_t k = std::min(limit, candidates.size());
    std::partial_sort(candidates.begin(),
                      candidates.begin() + static_cast<std::ptrdiff_t>(k),
                      candidates.end(), better);
    candidates.resize(k);
    result.items = std::move(candidates);
    return result;
}

std::vector<Suggestion> Trie::allWords() const {
    std::vector<Suggestion> words;
    words.reserve(wordCount_);
    std::string buffer;
    collect(root_.get(), buffer, words);
    return words;
}
