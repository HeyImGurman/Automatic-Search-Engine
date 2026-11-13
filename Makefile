CXX       ?= g++
CPPFLAGS  += -I.
CXXFLAGS  ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
BUILD     := build
SRCS      := main.cpp Trie.cpp WordStore.cpp

.PHONY: all run test clean

all: $(BUILD)/autocomplete

$(BUILD)/autocomplete: $(SRCS) Trie.h WordStore.h
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS)

$(BUILD)/test_trie: tests/test_trie.cpp Trie.cpp WordStore.cpp Trie.h WordStore.h
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ tests/test_trie.cpp Trie.cpp WordStore.cpp

run: all
	./$(BUILD)/autocomplete

test: $(BUILD)/test_trie
	./$(BUILD)/test_trie

clean:
	rm -rf $(BUILD)
