#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "Trie.h"
#include "WordStore.h"

namespace {

constexpr const char* kDefaultDataFile = "data/words.txt";
constexpr std::size_t kDefaultLimit = 5;
constexpr std::size_t kMaxLimit = 50;

std::string trim(const std::string& s) {
    std::size_t first = 0;
    std::size_t last = s.size();
    while (first < last && std::isspace(static_cast<unsigned char>(s[first]))) ++first;
    while (last > first && std::isspace(static_cast<unsigned char>(s[last - 1]))) --last;
    return s.substr(first, last - first);
}

bool parsePositive(const std::string& text, std::size_t& out) {
    if (text.empty() || text.size() > 6) return false;
    for (char c : text) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    out = static_cast<std::size_t>(std::stoul(text));
    return out > 0;
}

const char* kInvalidWordMessage =
    "Invalid input. Use letters, digits, spaces, hyphens or apostrophes "
    "(max 64 characters).";

/** Interactive command-line front end around a Trie. */
class AutocompleteApp {
public:
    explicit AutocompleteApp(std::string dataFile)
        : dataFile_(std::move(dataFile)), limit_(kDefaultLimit) {}

    int runStdio() {
        WordStore::load(dataFile_, trie_);

        std::string prefix;
        while (std::getline(std::cin, prefix)) {
            const auto normalized = Trie::normalize(prefix);
            if (!normalized) {
                std::cout << "{\"error\":\"invalid_prefix\"}\n" << std::flush;
                continue;
            }

            const SuggestionResult result = trie_.suggest(*normalized, 8);
            std::cout << "{\"totalMatches\":" << result.totalMatches
                      << ",\"items\":[";
            for (std::size_t i = 0; i < result.items.size(); ++i) {
                if (i > 0) std::cout << ',';
                std::cout << "{\"word\":\"" << result.items[i].word
                          << "\",\"frequency\":" << result.items[i].frequency << '}';
            }
            std::cout << "]}\n" << std::flush;
        }
        return 0;
    }

    int run() {
        loadData();
        printHelp();

        std::string line;
        while (true) {
            std::cout << "\n> " << std::flush;
            if (!std::getline(std::cin, line)) {  // EOF (Ctrl+D / piped input)
                std::cout << "\n";
                break;
            }
            line = trim(line);
            if (line.empty()) {
                std::cout << "Please type a prefix to search, or /help for commands.\n";
                continue;
            }
            if (line[0] == '/') {
                if (!handleCommand(line)) break;
            } else {
                handleSearch(line);
            }
        }
        std::cout << "Goodbye!\n";
        return 0;
    }

private:
    void loadData() {
        const LoadReport report = WordStore::load(dataFile_, trie_);
        if (!report.fileFound) {
            std::cout << "Note: '" << dataFile_
                      << "' not found. Starting with an empty dictionary; it "
                         "will be created when you add a word.\n";
            return;
        }
        std::cout << "Loaded " << report.loaded << " words from '" << dataFile_ << "'";
        if (report.duplicates > 0) std::cout << " (" << report.duplicates << " duplicates skipped)";
        if (report.invalid > 0) std::cout << " (" << report.invalid << " invalid lines skipped)";
        std::cout << ".\n";
    }

    void printHelp() const {
        std::cout <<
            "\nAutocomplete Search Engine (Trie)\n"
            "---------------------------------\n"
            "  <prefix>        show suggestions for a prefix (case-insensitive)\n"
            "  /add <word>     add a new word (saved to disk)\n"
            "  /del <word>     delete a word (saved to disk)\n"
            "  /pick <n>       choose suggestion n from the last search; it ranks higher next time\n"
            "  /limit <n>      number of suggestions to show (1-" << kMaxLimit << ", now " << limit_ << ")\n"
            "  /stats          dictionary statistics\n"
            "  /help           show this help\n"
            "  /quit           exit\n";
    }

    void handleSearch(const std::string& rawPrefix) {
        const auto prefix = Trie::normalize(rawPrefix);
        if (!prefix) {
            std::cout << kInvalidWordMessage << "\n";
            return;
        }

        const SuggestionResult result = trie_.suggest(*prefix, limit_);
        lastSuggestions_.clear();
        if (result.items.empty()) {
            std::cout << "No suggestions found for \"" << *prefix << "\".\n"
                      << "Tip: /add " << *prefix << "  to add it to the dictionary.\n";
            return;
        }

        std::cout << "Suggestions for \"" << *prefix << "\" (showing "
                  << result.items.size() << " of " << result.totalMatches << "):\n";
        for (std::size_t i = 0; i < result.items.size(); ++i) {
            std::cout << "  " << (i + 1) << ". " << result.items[i].word
                      << "   [used " << result.items[i].frequency << "x]\n";
            lastSuggestions_.push_back(result.items[i].word);
        }
    }

    // Returns false when the program should exit.
    bool handleCommand(const std::string& line) {
        const std::size_t space = line.find_first_of(" \t");
        std::string command = line.substr(0, space);
        const std::string argument =
            space == std::string::npos ? "" : trim(line.substr(space + 1));
        std::transform(command.begin(), command.end(), command.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (command == "/quit" || command == "/exit" || command == "/q") return false;
        if (command == "/help" || command == "/h") { printHelp(); return true; }
        if (command == "/add")   { handleAdd(argument);    return true; }
        if (command == "/del" || command == "/delete") { handleDelete(argument); return true; }
        if (command == "/pick")  { handlePick(argument);   return true; }
        if (command == "/limit") { handleLimit(argument);  return true; }
        if (command == "/stats") { handleStats();          return true; }

        std::cout << "Unknown command '" << command << "'. Type /help for the list.\n";
        return true;
    }

    void handleAdd(const std::string& word) {
        if (word.empty()) { std::cout << "Usage: /add <word>\n"; return; }
        switch (trie_.insert(word)) {
            case Trie::InsertResult::Invalid:
                std::cout << kInvalidWordMessage << "\n";
                break;
            case Trie::InsertResult::AlreadyExists:
                std::cout << "\"" << *Trie::normalize(word) << "\" already exists. No duplicate added.\n";
                break;
            case Trie::InsertResult::Inserted:
                std::cout << "Added \"" << *Trie::normalize(word) << "\".\n";
                persist();
                break;
        }
    }

    void handleDelete(const std::string& word) {
        if (word.empty()) { std::cout << "Usage: /del <word>\n"; return; }
        if (!Trie::normalize(word)) { std::cout << kInvalidWordMessage << "\n"; return; }
        if (trie_.remove(word)) {
            std::cout << "Deleted \"" << *Trie::normalize(word) << "\".\n";
            persist();
        } else {
            std::cout << "\"" << *Trie::normalize(word) << "\" is not in the dictionary.\n";
        }
    }

    void handlePick(const std::string& argument) {
        std::size_t index = 0;
        if (!parsePositive(argument, index)) { std::cout << "Usage: /pick <number>\n"; return; }
        if (lastSuggestions_.empty()) { std::cout << "Run a search first, then pick from its results.\n"; return; }
        if (index > lastSuggestions_.size()) {
            std::cout << "Choose a number between 1 and " << lastSuggestions_.size() << ".\n";
            return;
        }
        const std::string& word = lastSuggestions_[index - 1];
        if (trie_.incrementFrequency(word)) {
            std::cout << "You picked \"" << word << "\" (now used " << trie_.frequencyOf(word) << "x).\n";
            persist();
        } else {
            std::cout << "\"" << word << "\" no longer exists.\n";
        }
    }

    void handleLimit(const std::string& argument) {
        std::size_t value = 0;
        if (!parsePositive(argument, value) || value > kMaxLimit) {
            std::cout << "Usage: /limit <1-" << kMaxLimit << ">\n";
            return;
        }
        limit_ = value;
        std::cout << "Showing up to " << limit_ << " suggestions.\n";
    }

    void handleStats() const {
        std::cout << "Words stored : " << trie_.size() << "\n"
                  << "Trie nodes   : " << trie_.nodeCount() << "\n"
                  << "Data file    : " << dataFile_ << "\n"
                  << "Result limit : " << limit_ << "\n";
    }

    void persist() const {
        std::string error;
        if (!WordStore::save(dataFile_, trie_, &error)) {
            std::cout << "Warning: could not save to disk (" << error << ").\n";
        }
    }

    Trie trie_;
    std::string dataFile_;
    std::size_t limit_;
    std::vector<std::string> lastSuggestions_;
};

}  // namespace

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "--stdio") {
        const std::string dataFile = (argc > 2) ? argv[2] : kDefaultDataFile;
        AutocompleteApp app(dataFile);
        return app.runStdio();
    }
    const std::string dataFile = (argc > 1) ? argv[1] : kDefaultDataFile;
    AutocompleteApp app(dataFile);
    return app.run();
}
