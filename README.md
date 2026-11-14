# Autocomplete Search Engine (Trie / Prefix Tree) — C++17

A command-line autocomplete engine that returns ranked, real-time-style
suggestions for a typed prefix, like the search box on Google or YouTube.
Built around a hand-written **Trie** with clean OOP separation, STL containers,
file persistence and unit tests.

```
> app
Suggestions for "app" (showing 5 of 18):
  1. apple   [used 95x]
  2. application   [used 80x]
  3. apply   [used 60x]
  4. approach   [used 45x]
  5. app   [used 40x]
```

## Features

| Requirement | Implementation |
|---|---|
| Trie with `TrieNode`, insert, prefix search, DFS, end-of-word flag | `Trie.h` / `Trie.cpp` |
| Top-N suggestions | `Trie::suggest(prefix, limit)`; limit adjustable at runtime with `/limit` |
| Load words from `.txt` | `WordStore::load` reads `data/words.txt` |
| Add words, persisted across runs | `/add` inserts and immediately saves |
| Delete words | `/del` removes the word and prunes unused nodes |
| Case-insensitive | all input passes through `Trie::normalize` (trim + lower-case) |
| Invalid / empty input | rejected with clear messages (empty, illegal characters, > 64 chars) |
| No duplicates | `insert` returns `AlreadyExists`; duplicates in the file are skipped on load |
| No-match message | friendly message plus a hint to `/add` the word |

**Bonus:** suggestions are **ranked by usage frequency** (ties broken alphabetically).
`/pick <n>` records that you chose a suggestion, so it ranks higher next time, which is
the same idea real search engines use.

## Project structure

```
AutocompleteSearchEngine/
├── main.cpp            # interactive CLI (AutocompleteApp class)
├── Trie.h / Trie.cpp   # TrieNode + Trie data structure (no I/O)
├── WordStore.h / .cpp  # file loading / saving (no trie logic)
├── data/words.txt      # dictionary ("word" or "word,frequency" per line)
├── tests/test_trie.cpp # unit tests (52 checks, no external framework)
├── Makefile
└── README.md
```

`Trie` knows nothing about files or the console, and `WordStore` knows nothing about
trie internals. The CLI only talks to their public interfaces.

## Build and run

Requires a C++17 compiler (g++ / clang++) and `make`. Run from the project root so
the default `data/words.txt` path resolves.

```bash
make          # builds build/autocomplete
make run      # build + start the app
make test     # build and run the unit tests
make clean
```

Without `make`:
```bash
g++ -std=c++17 -O2 -Wall -Wextra -o autocomplete main.cpp Trie.cpp WordStore.cpp
./autocomplete                 # uses data/words.txt
./autocomplete path/to/my_words.txt
```

## Web app and deployment

The Vite web app uses the same C++ trie through a small Node.js WebSocket
service. Vercel can host the static Vite frontend, but it does not host a
persistent WebSocket server; deploy the service separately on a Docker-capable
host (for example, Railway, Render, or Fly.io).

### Run locally

Requires Node.js, npm, and g++.

```bash
npm install
npm run dev:server   # compiles the C++ engine and starts the WebSocket service
npm run dev          # in another terminal; opens the Vite app at localhost:5173
```

The UI connects to `ws://localhost:3001` in development. The service accepts
JSON requests such as `{"type":"search","id":1,"prefix":"app"}` and responds
with ranked suggestions and their usage frequencies.

### Deploy

1. Deploy the repository's `Dockerfile` to a host that supports persistent
   WebSocket connections. Configure `ALLOWED_ORIGINS` there with your Vercel
   site origin (for example, `https://your-app.vercel.app`). The service reads
   `data/words.txt` by default; configure `WORDS_FILE` if your dictionary is
   stored elsewhere.
2. Deploy the project to Vercel as a Vite app. Set `VITE_WS_URL` to the
   WebSocket service URL, using `wss://` for a secure deployment (for example,
   `wss://your-service.example.com`).
3. Redeploy the Vercel frontend after setting `VITE_WS_URL`; Vite embeds it at
   build time. The WebSocket service also exposes `/health` for host health
   checks.

The CLI and C++ tests remain available with `make run` and `make test`.

## Commands

| Input | Action |
|---|---|
| `app` (any text without `/`) | show suggestions for the prefix |
| `/add <word>` | add a word (saved immediately) |
| `/del <word>` | delete a word (saved immediately) |
| `/pick <n>` | choose result *n* from the last search; raises its rank |
| `/limit <n>` | show up to *n* suggestions (1–50, default 5) |
| `/stats` | word count, trie node count, data file |
| `/help`, `/quit` | help / exit (Ctrl+D also exits) |

Allowed characters: ASCII letters, digits, single spaces, `-` and `'`
(so phrases like `binary search` work).

## Data file format

```
# comments and blank lines are ignored
apple,95        # word,frequency
banana          # plain word -> frequency 1
```

Saves are written to a temporary file and then renamed over the original, so a crash
mid-write cannot corrupt your dictionary. Windows line endings and a UTF-8 BOM are
handled on load.

## Design notes

* **Children stored in `std::map<char, unique_ptr<TrieNode>>`**: memory is only spent
  on characters that exist (a 26-slot array wastes space and cannot hold digits or
  hyphens), DFS yields alphabetical order for free, and `unique_ptr` means no manual
  `delete` and no leaks (RAII).
* **Normalisation at the boundary**: every public method normalises its input, so
  `"APPLE "`, `"Apple"` and `"apple"` are the same word everywhere.
* **Deletion prunes**: removing `app` keeps `apple`; removing the last word on a branch
  frees the whole dead branch (`nodeCount()` is tracked and tested).
* **Bounded depth**: words are limited to 64 characters, which keeps the recursive DFS
  and destructor safely shallow.
* **Const-correct API** with `std::optional` for "valid or not" results.

## Complexity

Let **m** = length of the word/prefix, **N** = number of words, **σ** = alphabet size
(≤ 39 here), **S** = number of words that match a prefix, **L** = suggestion limit.

| Operation | Time | Space |
|---|---|---|
| `insert(word)` | O(m · log σ) ≈ **O(m)** | O(m) new nodes in the worst case |
| `contains(word)` | O(m · log σ) ≈ **O(m)** | O(1) |
| `remove(word)` | O(m · log σ) ≈ **O(m)** | O(m) recursion stack |
| `suggest(prefix, L)` | O(m · log σ) to find the prefix node + O(T) DFS over the subtree (T = chars in it) + O(S · log L) `partial_sort` | O(S · avg word length) for candidates |
| `allWords()` / save / load | O(total characters) | O(N · avg word length) |
| Whole trie | n/a | O(total characters) nodes worst case; shared prefixes are stored once |

Key point: lookup cost depends on the **length of the word, not on the number of
words** (a sorted vector needs O(log N) string comparisons, an unsorted one O(N)).
A trie also gives the prefix grouping for free.

## Ideas to extend it

* Early-terminating top-K (cache the best K completions in each node) so `suggest`
  doesn't visit the whole subtree for very short prefixes.
* Fuzzy matching / typo tolerance (edit-distance search over the trie).
* Compressed trie (radix tree) to cut node count.
* Interactive per-keystroke mode using a terminal library, or a small HTTP API on top.
* Unicode support (store `char32_t` or UTF-8 aware keys).
