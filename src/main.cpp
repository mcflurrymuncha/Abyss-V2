/*
 *   A b y s s .
 *
 *   Stare into the void for 20 minutes.
 *   The void stares back.
 *
 *   Built with SDL2 + Discord RPC
 *
 *   By vexi
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <algorithm>

// ── Discord RPC (header-only stub if SDK not present) ──────────────────────
#ifdef DISCORD_RPC
#include "discord_rpc.h"
#endif

// ── Constants ──────────────────────────────────────────────────────────────
static constexpr int   WINDOW_W        = 1280;
static constexpr int   WINDOW_H        = 720;
static constexpr float GOAL_SECONDS    = 20.0f * 60.0f;   // 20 minutes
static constexpr int   MAX_PARTICLES   = 512;
static constexpr int   MAX_WISPS       = 24;
static constexpr float TWO_PI          = 6.28318530718f;

// ── Discord RPC ────────────────────────────────────────────────────────────
static const char* DISCORD_APP_ID = "1234567890123456789";

// ── Colour palette ─────────────────────────────────────────────────────────
struct Col { Uint8 r, g, b, a; };
static constexpr Col C_BG       = {  2,  2,  6, 255 };
static constexpr Col C_VOID     = {  8,  4, 16, 255 };
static constexpr Col C_RING     = { 30, 10, 60, 200 };
static constexpr Col C_WISP     = {120, 60,200, 180 };
static constexpr Col C_TEXT     = {200,180,255, 255 };
static constexpr Col C_DIM      = { 80, 60,120, 180 };
static constexpr Col C_WARN     = {255,100, 60, 255 };
static constexpr Col C_WIN      = {220,200,255, 255 };

// ── Helpers ────────────────────────────────────────────────────────────────
static inline float randf(float lo, float hi) {
    return lo + (hi - lo) * (float)rand() / (float)RAND_MAX;
}
static inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : v > hi ? hi : v;
}
static void setColor(SDL_Renderer* r, Col c, Uint8 alphaOverride = 255) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, (Uint8)((c.a * alphaOverride) / 255));
}

// ── Particle ───────────────────────────────────────────────────────────────
struct Particle {
    float x, y, vx, vy;
    float life, maxLife;
    float size;
    Col   col;
    bool  active;
};

// ── Wisp (drifting light orb) ──────────────────────────────────────────────
struct Wisp {
    float x, y;
    float angle, radius;   // orbit params
    float speed;
    float phase;
    float alpha;           // 0-1 pulse
    float pulseSpeed;
};

// ── Eye blink state ────────────────────────────────────────────────────────
struct EyeState {
    float blinkT    = 1.0f;   // 1 = fully open, 0 = closed
    float nextBlink = 4.0f;
    bool  closing   = false;
    float timer     = 0.0f;
};

// ── GameState ──────────────────────────────────────────────────────────────
enum class Phase { WATCHING, WON, GAVE_UP };

struct GameState {
    float  elapsed    = 0.0f;
    Phase  phase      = Phase::WATCHING;

    // void-eye animation
    float  voidPulse  = 0.0f;
    float  voidRot    = 0.0f;
    float  pupilX     = 0.0f;
    float  pupilY     = 0.0f;
    float  targetPX   = 0.0f;
    float  targetPY   = 0.0f;
    float  gazeTimer  = 0.0f;

    EyeState eye;

    Particle particles[MAX_PARTICLES];
    Wisp     wisps[MAX_WISPS];

    // "sanity" – drops slowly, affects visuals
    float sanity   = 1.0f;
    float shakeX   = 0.0f;
    float shakeY   = 0.0f;
    float shakeAmt = 0.0f;

    // vignette pulse
    float vignPulse = 0.0f;
};

// ── Discord RPC helpers ────────────────────────────────────────────────────
static void discordInit(int64_t startTime) {
#ifdef DISCORD_RPC
    DiscordEventHandlers handlers{};
    Discord_Initialize(DISCORD_APP_ID, &handlers, 1, nullptr);

    DiscordRichPresence rp{};
    rp.state        = "Staring into the Abyss.";
    rp.details      = "20:00 remaining";
    rp.startTimestamp = startTime;
    rp.largeImageKey  = "void";
    rp.largeImageText = "Abyss.";
    Discord_UpdatePresence(&rp);
#else
    (void)startTime;
#endif
}

static void discordUpdate(float remaining, Phase phase) {
#ifdef DISCORD_RPC
    DiscordRichPresence rp{};
    if (phase == Phase::WON) {
        rp.state   = "Emerged from the Abyss.";
        rp.details = "Stared for 20 minutes. You won.";
    } else {
        int  mins = (int)remaining / 60;
        int  secs = (int)remaining % 60;
        char buf[64];
        snprintf(buf, sizeof(buf), "%02d:%02d remaining", mins, secs);
        rp.state   = "Staring into the Abyss.";
        rp.details = buf;
    }
    rp.largeImageKey  = "void";
    rp.largeImageText = "Abyss.";
    Discord_UpdatePresence(&rp);
    Discord_RunCallbacks();
#else
    (void)remaining; (void)phase;
#endif
}

static void discordShutdown() {
#ifdef DISCORD_RPC
    Discord_Shutdown();
#endif
}

// ── Draw filled circle ─────────────────────────────────────────────────────
static void fillCircle(SDL_Renderer* r, int cx, int cy, int radius) {
    for (int dy = -radius; dy <= radius; dy++) {
        int dx = (int)sqrtf((float)(radius * radius - dy * dy));
        SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

// ── Draw ring (thick outline) ──────────────────────────────────────────────
static void drawRing(SDL_Renderer* r, int cx, int cy, int rad, int thickness) {
    for (int t = 0; t < thickness; t++) {
        int rr = rad - t;
        if (rr <= 0) break;
        for (int i = 0; i < 360; i++) {
            float a = (float)i * TWO_PI / 360.0f;
            int x = cx + (int)(cosf(a) * rr);
            int y = cy + (int)(sinf(a) * rr);
            SDL_RenderDrawPoint(r, x, y);
        }
    }
}

// ── Initialise wisps ───────────────────────────────────────────────────────
static void initWisps(GameState& gs) {
    float cx = WINDOW_W / 2.0f;
    float cy = WINDOW_H / 2.0f;
    for (int i = 0; i < MAX_WISPS; i++) {
        Wisp& w   = gs.wisps[i];
        w.angle   = randf(0, TWO_PI);
        w.radius  = randf(120.0f, 320.0f);
        w.speed   = randf(0.08f, 0.25f) * (rand() % 2 ? 1.f : -1.f);
        w.phase   = randf(0, TWO_PI);
        w.pulseSpeed = randf(0.5f, 2.0f);
        w.alpha   = randf(0.3f, 1.0f);
        w.x = cx + cosf(w.angle) * w.radius;
        w.y = cy + sinf(w.angle) * w.radius;
    }
}

// ── Spawn a particle ───────────────────────────────────────────────────────
static void spawnParticle(GameState& gs, float x, float y) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle& p = gs.particles[i];
        if (p.active) continue;
        p.x       = x + randf(-4, 4);
        p.y       = y + randf(-4, 4);
        p.vx      = randf(-30.f, 30.f);
        p.vy      = randf(-60.f, -10.f);
        p.life    = randf(0.8f, 2.5f);
        p.maxLife = p.life;
        p.size    = randf(1.f, 3.f);
        p.col     = { (Uint8)randf(80,180), (Uint8)randf(30,80), (Uint8)randf(180,255), 200 };
        p.active  = true;
        break;
    }
}

// ── Update ─────────────────────────────────────────────────────────────────
static void update(GameState& gs, float dt) {
    if (gs.phase != Phase::WATCHING) return;

    gs.elapsed    += dt;
    gs.voidPulse  += dt * 0.7f;
    gs.voidRot    += dt * 0.03f;
    gs.sanity      = clampf(1.0f - gs.elapsed / GOAL_SECONDS, 0.0f, 1.0f);

    // screen shake ramps up as sanity drops
    gs.shakeAmt    = (1.0f - gs.sanity) * 4.0f;
    gs.shakeX      = randf(-gs.shakeAmt, gs.shakeAmt);
    gs.shakeY      = randf(-gs.shakeAmt, gs.shakeAmt);

    // vignette
    gs.vignPulse  += dt * lerp(0.4f, 1.8f, 1.0f - gs.sanity);

    // pupil drift
    gs.gazeTimer  -= dt;
    if (gs.gazeTimer <= 0.0f) {
        gs.targetPX  = randf(-40.f, 40.f);
        gs.targetPY  = randf(-20.f, 20.f);
        gs.gazeTimer = randf(1.5f, 4.0f);
    }
    float pupilSpeed = 3.0f * dt;
    gs.pupilX = lerp(gs.pupilX, gs.targetPX, pupilSpeed);
    gs.pupilY = lerp(gs.pupilY, gs.targetPY, pupilSpeed);

    // blink
    EyeState& e = gs.eye;
    e.timer    += dt;
    if (!e.closing && e.timer >= e.nextBlink) {
        e.closing  = true;
        e.timer    = 0.f;
        e.nextBlink = randf(3.f, 8.f);
    }
    if (e.closing) {
        float blinkDur = 0.12f;
        float halfDur  = blinkDur * 0.5f;
        if (e.timer < halfDur)
            e.blinkT = 1.0f - (e.timer / halfDur);
        else if (e.timer < blinkDur)
            e.blinkT = (e.timer - halfDur) / halfDur;
        else {
            e.blinkT  = 1.0f;
            e.closing = false;
            e.timer   = 0.f;
        }
    }

    // wisps
    float cx = WINDOW_W / 2.0f, cy = WINDOW_H / 2.0f;
    for (int i = 0; i < MAX_WISPS; i++) {
        Wisp& w  = gs.wisps[i];
        w.angle += w.speed * dt;
        float drift = sinf(gs.elapsed * 0.3f + w.phase) * 20.0f;
        w.x = cx + cosf(w.angle) * (w.radius + drift);
        w.y = cy + sinf(w.angle) * (w.radius * 0.55f + drift * 0.4f);
        w.alpha = 0.4f + 0.6f * (sinf(gs.elapsed * w.pulseSpeed + w.phase) * 0.5f + 0.5f);

        // wisp particles
        if (rand() % 6 == 0)
            spawnParticle(gs, w.x, w.y);
    }

    // particles
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle& p = gs.particles[i];
        if (!p.active) continue;
        p.life -= dt;
        if (p.life <= 0.f) { p.active = false; continue; }
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.vy -= 10.f * dt;  // float upward
    }

    // win condition
    if (gs.elapsed >= GOAL_SECONDS)
        gs.phase = Phase::WON;
}

// ── Render vignette ────────────────────────────────────────────────────────
static void renderVignette(SDL_Renderer* r, float sanity, float pulse) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    int   steps   = 80;
    float maxAlpha = lerp(120.f, 240.f, 1.0f - sanity);
    // pulsing extra
    maxAlpha += 20.f * sinf(pulse);

    for (int i = steps; i >= 0; i--) {
        float t   = (float)i / steps;           // 1 at edge, 0 at centre
        float a   = maxAlpha * t * t * t;
        SDL_SetRenderDrawColor(r, 0, 0, 0, (Uint8)clampf(a, 0, 255));
        SDL_Rect rect = { (int)(WINDOW_W * (1.f - t) * 0.5f),
                          (int)(WINDOW_H * (1.f - t) * 0.5f),
                          (int)(WINDOW_W * t),
                          (int)(WINDOW_H * t) };
        SDL_RenderDrawRect(r, &rect);
    }
}

// ── Render void eye ────────────────────────────────────────────────────────
static void renderEye(SDL_Renderer* r, const GameState& gs) {
    int   cx   = WINDOW_W / 2 + (int)gs.shakeX;
    int   cy   = WINDOW_H / 2 + (int)gs.shakeY;
    float blinkT = gs.eye.blinkT;

    float pulseScale = 1.0f + 0.04f * sinf(gs.voidPulse);
    int   eyeW = (int)(220 * pulseScale);
    int   eyeH = (int)(110 * blinkT * pulseScale);

    // -- sclera (dark) --
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    setColor(r, C_VOID, 255);
    // draw as ellipse by scaling circles
    for (int dy = -eyeH; dy <= eyeH; dy++) {
        float frac = (float)dy / (eyeH > 0 ? eyeH : 1);
        int   dx   = (int)(eyeW * sqrtf(clampf(1.f - frac * frac, 0.f, 1.f)));
        SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }

    // -- iris rings --
    for (int ri = 4; ri >= 0; ri--) {
        float  t    = (float)ri / 4.f;
        int    irad = (int)(lerp(30.f, 80.f, 1.f - t) * blinkT);
        Uint8  aa   = (Uint8)(80 + 120 * t);
        SDL_SetRenderDrawColor(r, C_RING.r, C_RING.g, C_RING.b, aa);
        // clip to eye ellipse
        for (int dy = -irad; dy <= irad; dy++) {
            float eyFrac  = (float)dy / (eyeH > 0 ? eyeH : 1);
            if (fabsf(eyFrac) > 1.f) continue;
            int   eyMaxDx = (int)(eyeW * sqrtf(clampf(1.f - eyFrac * eyFrac, 0.f, 1.f)));
            float irFrac  = (float)dy / (irad > 0 ? irad : 1);
            int   irDx    = (int)(irad * sqrtf(clampf(1.f - irFrac * irFrac, 0.f, 1.f)));
            int   x0      = std::max(cx - irDx, cx - eyMaxDx);
            int   x1      = std::min(cx + irDx, cx + eyMaxDx);
            if (x0 < x1) SDL_RenderDrawLine(r, x0, cy + dy, x1, cy + dy);
        }
    }

    // -- spinning lines in iris --
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 60, 20, 120, 80);
    int irisR = (int)(70 * blinkT);
    for (int li = 0; li < 12; li++) {
        float a  = gs.voidRot + (float)li * TWO_PI / 12.f;
        int   x0 = cx + (int)(cosf(a) * 18 * blinkT);
        int   y0 = cy + (int)(sinf(a) * 10 * blinkT);
        int   x1 = cx + (int)(cosf(a) * irisR);
        int   y1 = cy + (int)(sinf(a) * irisR * 0.5f);
        SDL_RenderDrawLine(r, x0, y0, x1, y1);
    }

    // -- pupil --
    int   px   = cx + (int)(gs.pupilX * blinkT);
    int   py   = cy + (int)(gs.pupilY * blinkT);
    int   prad = (int)(32 * blinkT * pulseScale);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    fillCircle(r, px, py, prad);

    // inner void shine
    SDL_SetRenderDrawColor(r, 40, 10, 80, 160);
    fillCircle(r, px, py, prad / 2);

    // tiny specular
    SDL_SetRenderDrawColor(r, 180, 160, 255, 120);
    fillCircle(r, px - prad / 4, py - prad / 4, 4);

    // -- eyelid shadow overlay --
    if (blinkT < 1.0f) {
        int lidH = (int)((1.0f - blinkT) * eyeH + eyeH);
        SDL_SetRenderDrawColor(r, C_BG.r, C_BG.g, C_BG.b, 255);
        // top lid
        for (int dy = -lidH; dy <= -eyeH - 1; dy++) {
            float frac = (float)dy / lidH;
            int   dx   = (int)(eyeW * sqrtf(clampf(1.f - frac * frac, 0.f, 1.f)));
            SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
        }
        // bottom lid
        for (int dy = eyeH + 1; dy <= lidH; dy++) {
            float frac = (float)dy / lidH;
            int   dx   = (int)(eyeW * sqrtf(clampf(1.f - frac * frac, 0.f, 1.f)));
            SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
        }
    }

    // -- outer glow rings --
    for (int gi = 1; gi <= 5; gi++) {
        float gp  = (float)gi / 5.f;
        Uint8 ga  = (Uint8)(40 * (1.f - gp) * (0.6f + 0.4f * sinf(gs.voidPulse + gp)));
        SDL_SetRenderDrawColor(r, 60, 20, 120, ga);
        int   gr  = eyeW + gi * 18;
        drawRing(r, cx, cy, gr, 2);
    }
}

// ── Render wisps ───────────────────────────────────────────────────────────
static void renderWisps(SDL_Renderer* r, const GameState& gs) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < MAX_WISPS; i++) {
        const Wisp& w   = gs.wisps[i];
        Uint8       a   = (Uint8)(w.alpha * 160);
        for (int s = 6; s >= 1; s--) {
            Uint8 sa = a / (Uint8)(s * 2);
            SDL_SetRenderDrawColor(r, C_WISP.r, C_WISP.g, C_WISP.b, sa);
            fillCircle(r, (int)w.x, (int)w.y, s * 2);
        }
    }
}

// ── Render particles ───────────────────────────────────────────────────────
static void renderParticles(SDL_Renderer* r, const GameState& gs) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < MAX_PARTICLES; i++) {
        const Particle& p = gs.particles[i];
        if (!p.active) continue;
        float t   = p.life / p.maxLife;
        Uint8 a   = (Uint8)(t * p.col.a);
        SDL_SetRenderDrawColor(r, p.col.r, p.col.g, p.col.b, a);
        int   s   = (int)(p.size * t + 0.5f);
        SDL_Rect rect = { (int)p.x - s, (int)p.y - s, s * 2, s * 2 };
        SDL_RenderFillRect(r, &rect);
    }
}

// ── Render HUD text ────────────────────────────────────────────────────────
static std::string formatTime(float secs) {
    int m = (int)secs / 60;
    int s = (int)secs % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", m, s);
    return std::string(buf);
}

static void renderText(SDL_Renderer* r, TTF_Font* font, const std::string& text,
                       int x, int y, Col col, bool centered = false) {
    SDL_Color sc = { col.r, col.g, col.b, col.a };
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text.c_str(), sc);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_Rect dst = { x, y, surf->w, surf->h };
    if (centered) dst.x -= surf->w / 2;
    SDL_FreeSurface(surf);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_RenderCopy(r, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

static void renderHUD(SDL_Renderer* r, TTF_Font* fontLarge, TTF_Font* fontSmall,
                      const GameState& gs) {
    float remaining = clampf(GOAL_SECONDS - gs.elapsed, 0.f, GOAL_SECONDS);

    if (gs.phase == Phase::WON) {
        renderText(r, fontLarge, "You endured the Abyss.",
                   WINDOW_W / 2, WINDOW_H / 2 - 60, C_WIN, true);
        renderText(r, fontSmall, "20 minutes. The void blinked first.",
                   WINDOW_W / 2, WINDOW_H / 2 + 10, C_DIM, true);
        renderText(r, fontSmall, "Press ESC to leave.",
                   WINDOW_W / 2, WINDOW_H / 2 + 50, C_DIM, true);
        return;
    }

    // title
    renderText(r, fontSmall, "Abyss.", 30, 24, C_DIM);

    // timer (top centre)
    Col timerCol = remaining < 60.f ? C_WARN : C_TEXT;
    renderText(r, fontLarge, formatTime(remaining),
               WINDOW_W / 2, 20, timerCol, true);

    // progress bar
    int   barW = 300, barH = 3;
    int   barX = WINDOW_W / 2 - barW / 2, barY = 70;
    float prog = gs.elapsed / GOAL_SECONDS;

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 40, 20, 70, 180);
    SDL_Rect bgBar = { barX, barY, barW, barH };
    SDL_RenderFillRect(r, &bgBar);

    SDL_SetRenderDrawColor(r, C_WISP.r, C_WISP.g, C_WISP.b, 200);
    SDL_Rect fgBar = { barX, barY, (int)(barW * prog), barH };
    SDL_RenderFillRect(r, &fgBar);

    // sanity hint (fades in as sanity drops)
    float insanity = 1.0f - gs.sanity;
    if (insanity > 0.2f) {
        Uint8 hintA = (Uint8)(clampf((insanity - 0.2f) / 0.8f, 0.f, 1.f) * 160.f);
        Col   hintC = { 180, 120, 255, hintA };
        const char* hints[] = {
            "it is looking at you",
            "don't look away",
            "you are still here",
            "the void remembers",
            "almost",
        };
        int hi = (int)(gs.elapsed / (GOAL_SECONDS / 5)) % 5;
        renderText(r, fontSmall, hints[hi], WINDOW_W / 2, WINDOW_H - 60, hintC, true);
    }

    // ESC hint
    renderText(r, fontSmall, "ESC — give up", 30, WINDOW_H - 40, C_DIM);
}

// ── Main ───────────────────────────────────────────────────────────────────
int main(int /*argc*/, char** /*argv*/) {
    srand((unsigned)time(nullptr));

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() != 0) {
        fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Abyss.",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_W, WINDOW_H,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!window) { fprintf(stderr, "Window: %s\n", SDL_GetError()); return 1; }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer) { fprintf(stderr, "Renderer: %s\n", SDL_GetError()); return 1; }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Load system monospace font (fallback to any available)
    TTF_Font* fontLarge = nullptr;
    TTF_Font* fontSmall = nullptr;
    const char* fontPaths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        nullptr
    };
    for (int fi = 0; fontPaths[fi] && !fontLarge; fi++) {
        fontLarge = TTF_OpenFont(fontPaths[fi], 42);
        fontSmall = TTF_OpenFont(fontPaths[fi], 18);
    }
    if (!fontLarge) {
        fprintf(stderr, "Could not load any font: %s\n", TTF_GetError());
        return 1;
    }

    // Discord
    int64_t startTime = (int64_t)time(nullptr);
    discordInit(startTime);

    // Game state
    GameState gs{};
    initWisps(gs);

    float discordTimer = 0.f;

    Uint32 prevTick = SDL_GetTicks();
    bool   running  = true;

    while (running) {
        // ── Timing ──
        Uint32 now = SDL_GetTicks();
        float  dt  = clampf((now - prevTick) / 1000.f, 0.f, 0.05f);
        prevTick   = now;

        // ── Events ──
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) { running = false; }
            if (ev.type == SDL_KEYDOWN) {
                if (ev.key.keysym.sym == SDLK_ESCAPE) {
                    if (gs.phase == Phase::WATCHING)
                        gs.phase = Phase::GAVE_UP;
                    else
                        running = false;
                }
                if (ev.key.keysym.sym == SDLK_RETURN && gs.phase != Phase::WATCHING)
                    running = false;
            }
        }

        // ── Update ──
        update(gs, dt);

        discordTimer += dt;
        if (discordTimer >= 5.0f) {
            discordUpdate(clampf(GOAL_SECONDS - gs.elapsed, 0.f, GOAL_SECONDS), gs.phase);
            discordTimer = 0.f;
        }

        // ── Render ──
        setColor(renderer, C_BG, 255);
        SDL_RenderClear(renderer);

        if (gs.phase == Phase::WATCHING) {
            // subtle void backdrop gradient (drawn as fading concentric rects)
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            for (int i = 20; i >= 0; i--) {
                float t  = (float)i / 20.f;
                Uint8 a  = (Uint8)(30 * t);
                SDL_SetRenderDrawColor(renderer, 20, 5, 50, a);
                int pad  = (int)(t * 200.f);
                SDL_Rect rr = { pad, pad, WINDOW_W - pad * 2, WINDOW_H - pad * 2 };
                SDL_RenderFillRect(renderer, &rr);
            }

            renderWisps(renderer, gs);
            renderEye(renderer, gs);
            renderParticles(renderer, gs);
            renderVignette(renderer, gs.sanity, gs.vignPulse);
        } else if (gs.phase == Phase::WON) {
            // gentle bright void
            SDL_SetRenderDrawColor(renderer, 10, 4, 24, 255);
            SDL_RenderClear(renderer);
            renderWisps(renderer, gs);
            renderVignette(renderer, 0.6f, gs.vignPulse);
        } else { // GAVE_UP
            SDL_SetRenderDrawColor(renderer, 2, 2, 6, 255);
            SDL_RenderClear(renderer);
            renderText(renderer, fontLarge, "You looked away.",
                       WINDOW_W / 2, WINDOW_H / 2 - 40, C_WARN, true);
            renderText(renderer, fontSmall, "The Abyss is patient.",
                       WINDOW_W / 2, WINDOW_H / 2 + 20, C_DIM, true);
            renderText(renderer, fontSmall, "Press ENTER or ESC to exit.",
                       WINDOW_W / 2, WINDOW_H / 2 + 60, C_DIM, true);
        }

        renderHUD(renderer, fontLarge, fontSmall, gs);

        SDL_RenderPresent(renderer);
    }

    discordShutdown();
    TTF_CloseFont(fontSmall);
    TTF_CloseFont(fontLarge);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
