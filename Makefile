# Abyss. — Simple Makefile (alternative to CMake)
# Usage:
#   make              — build without Discord RPC
#   make DISCORD=1    — build with Discord RPC (requires SDK in ./lib/)
#   make clean

CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra
LDFLAGS  :=

# SDL2 flags via pkg-config
SDL_CFLAGS  := $(shell pkg-config --cflags sdl2 SDL2_ttf)
SDL_LDFLAGS := $(shell pkg-config --libs   sdl2 SDL2_ttf) -lm

SRC    := src/main.cpp
TARGET := abyss

ifeq ($(DISCORD),1)
CXXFLAGS += -DDISCORD_RPC -Ilib
LDFLAGS  += -Llib -ldiscord_game_sdk -Wl,-rpath,'$$ORIGIN/lib'
$(info Discord RPC: ENABLED)
else
$(info Discord RPC: disabled  [use: make DISCORD=1])
endif

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) $< -o $@ $(SDL_LDFLAGS) $(LDFLAGS)
	@echo ""
	@echo "  Built: ./$(TARGET)"
	@echo "  Run:   ./$(TARGET)"
	@echo ""

clean:
	rm -f $(TARGET)

.PHONY: all clean
