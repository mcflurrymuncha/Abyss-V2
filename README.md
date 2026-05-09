# Abyss.

> *Stare into the void for 20 minutes.*  
> *The void stares back.*

A minimalist C++ game. You look at it. It looks at you. Don't blink.

---

## Requirements

- **Linux / macOS / Windows** (with minor path tweaks)
- **g++** (C++17)
- **SDL2** + **SDL2_ttf**
-  **Discord Game SDK** (for Rich Presence) (Optional)

### Install SDL2

**Ubuntu/Debian:**
```bash
sudo apt install libsdl2-dev libsdl2-ttf-dev
```

**macOS (Homebrew):**
```bash
brew install sdl2 sdl2_ttf
```

**Windows (vcpkg):**
```bash
vcpkg install sdl2 sdl2-ttf
```

---

## Build

### Option A — Makefile (quickest)

```bash
# Without Discord RPC
make

# With Discord RPC
make DISCORD=1
```

### Option B — CMake

```bash
mkdir build && cd build

# Without Discord RPC
cmake .. && make

# With Discord RPC
cmake .. -DDISCORD_RPC=ON && make
```

---

## Discord Rich Presence Setup

Discord RPC shows **"Staring into the Abyss."** with a live countdown timer on your Discord profile.

1. **Download the Discord Game SDK**  
   → https://discord.com/developers/docs/game-sdk/sdk-starter-guide  
   Download `discord_game_sdk.zip`

2. **Extract and copy files:**
   ```
   discord_game_sdk/c/discord_rpc.h          →  Abyss./lib/discord_rpc.h
   discord_game_sdk/lib/x86_64/libdiscord_game_sdk.so  →  Abyss./lib/
   ```

3. **(Optional) Register your own App ID**  
   Go to https://discord.com/developers/applications, create a new app,  
   copy the **Application ID**, and replace `DISCORD_APP_ID` in `src/main.cpp`.

4. **Build with Discord enabled:**
   ```bash
   make DISCORD=1
   # or
   cmake .. -DDISCORD_RPC=ON && make
   ```

5. **Run** — Discord must be open. Your status will show automatically.

---

## Controls

| Key | Action |
|-----|--------|
| `ESC` | Give up (you coward) |
| `ENTER` / `ESC` | Exit after win/loss screen |

---

## What happens

- A great eye opens in the void and stares at you
- Spectral wisps orbit it, drifting and pulsing
- As time passes the screen destabilises — subtle shakes, deeper vignette, whispered text
- After **20 minutes** you win. The void blinked first.
- If you press ESC: *"You looked away. The Abyss is patient."*

---

## Project structure

```
Abyss./
├── src/
│   └── main.cpp          — entire game (~530 lines)
├── lib/                  — place Discord SDK files here
├── CMakeLists.txt
├── Makefile
└── README.md
```

---

*Built with SDL2. No dependencies beyond that (Discord RPC optional).*  
*Single source file. Zero assets. Pure void.*
