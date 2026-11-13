#include "WordStore.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace {

std::string trim(const std::string& s) {
    std::size_t first = 0;
    std::size_t last = s.size();
    while (first < last && std::isspace(static_cast<unsigned char>(s[first]))) ++first;
    while (last > first && std::isspace(static_cast<unsigned char>(s[last - 1]))) --last;
    return s.substr(first, last - first);
}

bool parseFrequency(const std::string& text, std::size_t& out) {
    if (text.empty() || text.size() > 18) return false;
    for (char c : text) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    out = static_cast<std::size_t>(std::stoull(text));
    return true;
}

}  // namespace

LoadReport WordStore::load(const std::string& path, Trie& trie) {
    LoadReport report;
    std::ifstream in(path);
    if (!in) return report;
    report.fileFound = true;

    std::string line;
    bool firstLine = true;
    while (std::getline(in, line)) {
        if (firstLine) {
            firstLine = false;
            if (line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);  // UTF-8 BOM
        }
        line = trim(line);  // also strips a trailing '\r' from Windows files
        if (line.empty() || line[0] == '#') continue;

        std::string word = line;
        std::size_t frequency = 1;
        const std::size_t comma = line.rfind(',');
        if (comma != std::string::npos) {
            if (!parseFrequency(trim(line.substr(comma + 1)), frequency)) {
                ++report.invalid;
                continue;
            }
            word = line.substr(0, comma);
        }

        switch (trie.insert(word, frequency)) {
            case Trie::InsertResult::Inserted:      ++report.loaded;     break;
            case Trie::InsertResult::AlreadyExists: ++report.duplicates; break;
            case Trie::InsertResult::Invalid:       ++report.invalid;    break;
        }
    }
    return report;
}

bool WordStore::save(const std::string& path, const Trie& trie, std::string* error) {
    namespace fs = std::filesystem;
    const auto fail = [&](const std::string& message) {
        if (error != nullptr) *error = message;
        return false;
    };

    std::error_code ec;
    const fs::path target(path);
    if (target.has_parent_path()) {
        fs::create_directories(target.parent_path(), ec);
        if (ec) return fail("cannot create directory: " + ec.message());
    }

    fs::path temp = target;
    temp += ".tmp";
    const std::string tempPath = temp.string();
    {
        std::ofstream out(tempPath, std::ios::trunc);
        if (!out) return fail("cannot open " + tempPath + " for writing");
        for (const Suggestion& entry : trie.allWords()) {
            out << entry.word << ',' << entry.frequency << '\n';
        }
        out.flush();
        if (!out) return fail("write failed for " + tempPath);
    }

    fs::rename(temp, target, ec);
    if (ec) {
        fs::remove(temp, ec);
        return fail("cannot replace " + path);
    }
    return true;
}
