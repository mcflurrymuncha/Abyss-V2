/*
 * A b y s s . 
 * Pure void | Discord Rich Presence | Terminal Logging
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <vector>
#include <string>
#include <ctime>
#include <cmath>

// Ensure you have the Discord Game SDK in your include path
#include "discord.h" 

// ── Constants ──────────────────────────────────────────────────────────────
static constexpr int   WINDOW_W        = 1280;
static constexpr int   WINDOW_H        = 720;
static constexpr float GOAL_SECONDS    = 20.0f * 60.0f;
static constexpr int64_t DISCORD_APP_ID = 1502615601888231424LL;

// ── Globals ────────────────────────────────────────────────────────────────
discord::Core* g_discord = nullptr;
enum class Phase { WATCHING, WON };

// ── Discord Integration ────────────────────────────────────────────────────
void InitDiscord() {
    auto result = discord::Core::Create(DISCORD_APP_ID, DiscordCreateFlags_NoRequireDiscord, &g_discord);
    if (result != discord::Result::Ok) {
        std::cout << "[DISCORD] Failed to connect (Is Discord open?)" << std::endl;
    } else {
        std::cout << "[DISCORD] Presence Link Active." << std::endl;
    }
}

void UpdateDiscord(float elapsed, Phase phase) {
    if (!g_discord) return;

    discord::Activity activity{};
    if (phase == Phase::WON) {
        activity.SetDetails("Emerged from the Abyss.");
        activity.SetState("The void blinked first.");
    } else {
        activity.SetDetails("Staring into the void...");
        char timeBuf[32];
        int rem = (int)(GOAL_SECONDS - elapsed);
        sprintf(timeBuf, "%02d:%02d remaining", rem / 60, rem % 60);
        activity.SetState(timeBuf);
    }

    activity.GetAssets().SetLargeImage("abyss_icon"); // Must match your Discord Dev Portal Asset name
    activity.GetAssets().SetLargeText("Abyss.");
    
    // This adds the "elapsed" timer to the Discord status
    activity.GetTimestamps().SetStart(time(nullptr) - (int)elapsed);

    g_discord->ActivityManager().UpdateActivity(activity, [](discord::Result result) {
        if (result != discord::Result::Ok) {
            // Silence is golden in the logs, but you can debug here
        }
    });
}

// ── Helper ─────────────────────────────────────────────────────────────────
void RenderVignette(SDL_Renderer* ren, float elapsed) {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    float pulse = sinf(elapsed * 0.5f) * 10.0f;
    for (int i = 0; i < 100; ++i) {
        float t = i / 100.0f;
        Uint8 alpha = (Uint8)(255 * (1.0f - t));
        SDL_SetRenderDrawColor(ren, 0, 0, 0, alpha);
        SDL_Rect r = { (int)(WINDOW_W * t / 2), (int)(WINDOW_H * t / 2), (int)(WINDOW_W * (1 - t)), (int)(WINDOW_H * (1 - t)) };
        SDL_RenderDrawRect(ren, &r);
    }
}

// ── Main ───────────────────────────────────────────────────────────────────
int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 1;
    if (TTF_Init() < 0) return 1;

    SDL_Window* win = SDL_CreateWindow("Abyss.", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_W, WINDOW_H, SDL_WINDOW_SHOWN);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    // Terminal Welcome
    std::cout << "========================================" << std::endl;
    std::cout << "             ABYSS TERMINAL             " << std::endl;
    std::cout << "========================================" << std::endl;

    InitDiscord();

    TTF_Font* font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 28); // Standard Windows path
    SDL_Color textColor = { 100, 100, 150, 255 };

    float elapsed = 0.0f;
    float discordTimer = 0.0f;
    bool running = true;
    Phase phase = Phase::WATCHING;
    Uint32 lastTicks = SDL_GetTicks();

    while (running) {
        Uint32 currentTicks = SDL_GetTicks();
        float dt = (currentTicks - lastTicks) / 1000.0f;
        lastTicks = currentTicks;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
        }

        if (phase == Phase::WATCHING) {
            elapsed += dt;
            discordTimer += dt;

            // Update Discord every 2 seconds to avoid rate limits
            if (discordTimer >= 2.0f) {
                UpdateDiscord(elapsed, phase);
                discordTimer = 0.0f;
                std::cout << "[VOID LOG] " << (int)(GOAL_SECONDS - elapsed) << "s remaining. Focus steady." << std::endl;
            }

            if (elapsed >= GOAL_SECONDS) {
                phase = Phase::WON;
                UpdateDiscord(elapsed, phase);
                std::cout << "[EVENT] Achievement Unlocked: The Void Blinked." << std::endl;
            }
        }

        if (g_discord) g_discord->RunCallbacks();

        // Rendering
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);

        RenderVignette(ren, elapsed);

        // Render Timer Text
        if (font) {
            char buf[32];
            int rem = (int)(GOAL_SECONDS - elapsed);
            if (phase == Phase::WON) sprintf(buf, "YOU SURVIVED.");
            else sprintf(buf, "%02d:%02d", rem / 60, rem % 60);

            SDL_Surface* surf = TTF_RenderText_Blended(font, buf, textColor);
            SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, surf);
            SDL_Rect textRect = { WINDOW_W/2 - surf->w/2, 50, surf->w, surf->h };
            SDL_RenderCopy(ren, tex, NULL, &textRect);
            SDL_FreeSurface(surf);
            SDL_DestroyTexture(tex);
        }

        SDL_RenderPresent(ren);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
