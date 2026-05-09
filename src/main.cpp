/*
 *   A b y s s .
 *
 *   Stare into the void for 20 minutes.
 *   The void stares back.
 *
 *   Built with SDL2 + Discord Game SDK
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

// ── Discord Game SDK ───────────────────────────────────────────────────────
// Setup:
//   1. Copy discord_game_sdk/cpp/* into lib/discord/
//   2. Copy discord_game_sdk/lib/x86_64/discord_game_sdk.dll (+ .so/.dylib) into lib/
//   3. Build with -DDISCORD_RPC (handled by Makefile/CMake automatically)
#ifdef DISCORD_RPC
#include "discord/discord.h"
static discord::Core* g_discord = nullptr;
#endif

// ── Constants ──────────────────────────────────────────────────────────────
static constexpr int   WINDOW_W        = 1280;
static constexpr int   WINDOW_H        = 720;
static constexpr float GOAL_SECONDS    = 20.0f * 60.0f;
static constexpr int   MAX_PARTICLES   = 512;
static constexpr int   MAX_WISPS       = 24;
static constexpr float TWO_PI          = 6.28318530718f;

static constexpr int64_t DISCORD_APP_ID = 1502615601888231424LL;

// ── Colour palette ─────────────────────────────────────────────────────────
struct Col { Uint8 r, g, b, a; };
static constexpr Col C_BG   = {  2,  2,  6, 255 };
static constexpr Col C_VOID = {  8,  4, 16, 255 };
static constexpr Col C_RING = { 30, 10, 60, 200 };
static constexpr Col C_WISP = {120, 60,200, 180 };
static constexpr Col C_TEXT = {200,180,255, 255 };
static constexpr Col C_DIM  = { 80, 60,120, 180 };
static constexpr Col C_WARN = {255,100, 60, 255 };
static constexpr Col C_WIN  = {220,200,255, 255 };

// ── Helpers ────────────────────────────────────────────────────────────────
static inline float randf(float lo, float hi) {
    return lo + (hi - lo) * (float)rand() / (float)RAND_MAX;
}
static inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : v > hi ? hi : v;
}
static void setColor(SDL_Renderer* r, Col c, Uint8 ao = 255) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, (Uint8)((c.a * ao) / 255));
}

// ── Types ──────────────────────────────────────────────────────────────────
struct Particle {
    float x, y, vx, vy, life, maxLife, size;
    Col   col;
    bool  active;
};

struct Wisp {
    float x, y, angle, radius, speed, phase, alpha, pulseSpeed;
};

struct EyeState {
    float blinkT    = 1.0f;
    float nextBlink = 4.0f;
    bool  closing   = false;
    float timer     = 0.0f;
};

enum class Phase { WATCHING, WON, GAVE_UP };

struct GameState {
    float    elapsed = 0.0f;
    Phase    phase   = Phase::WATCHING;
    float    voidPulse = 0.0f, voidRot = 0.0f;
    float    pupilX = 0.0f, pupilY = 0.0f;
    float    targetPX = 0.0f, targetPY = 0.0f, gazeTimer = 0.0f;
    EyeState eye;
    Particle particles[MAX_PARTICLES];
    Wisp     wisps[MAX_WISPS];
    float    sanity = 1.0f, shakeX = 0.0f, shakeY = 0.0f, shakeAmt = 0.0f;
    float    vignPulse = 0.0f;
};

// ── Discord helpers ────────────────────────────────────────────────────────
static void discordInit() {
#ifdef DISCORD_RPC
    auto result = discord::Core::Create(
        DISCORD_APP_ID,
        DiscordCreateFlags_NoRequireDiscord,
        &g_discord
    );
    if (result != discord::Result::Ok) {
        fprintf(stderr, "Discord: init failed (code %d) — is Discord running?\n",
                (int)result);
        g_discord = nullptr;
        return;
    }
    discord::Activity act{};
    act.SetDetails("Staring into the void");
    act.SetState("20:00 remaining");
    act.GetTimestamps().SetStart((int64_t)time(nullptr));
    act.GetAssets().SetLargeImage("abyss");
    act.GetAssets().SetLargeText("Abyss.");
    g_discord->ActivityManager().UpdateActivity(act, [](discord::Result) {});
    printf("Discord: Rich Presence active\n");
#endif
}

static void discordUpdate(float remaining, Phase phase) {
#ifdef DISCORD_RPC
    if (!g_discord) return;
    discord::Activity act{};
    if (phase == Phase::WON) {
        act.SetDetails("Emerged from the Abyss.");
        act.SetState("Survived 20 minutes. The void blinked first.");
    } else {
        int  m = (int)remaining / 60, s = (int)remaining % 60;
        char buf[32];
        snprintf(buf, sizeof(buf), "%02d:%02d remaining", m, s);
        act.SetDetails("Staring into the void");
        act.SetState(buf);
        act.GetTimestamps().SetEnd(
            (int64_t)time(nullptr) + (int64_t)remaining
        );
    }
    act.GetAssets().SetLargeImage("abyss");
    act.GetAssets().SetLargeText("Abyss.");
    g_discord->ActivityManager().UpdateActivity(act, [](discord::Result) {});
    g_discord->RunCallbacks();
#else
    (void)remaining; (void)phase;
#endif
}

static void discordShutdown() {
#ifdef DISCORD_RPC
    delete g_discord;
    g_discord = nullptr;
#endif
}

// ── Drawing ────────────────────────────────────────────────────────────────
static void fillCircle(SDL_Renderer* r, int cx, int cy, int radius) {
    for (int dy = -radius; dy <= radius; dy++) {
        int dx = (int)sqrtf((float)(radius * radius - dy * dy));
        SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

static void drawRing(SDL_Renderer* r, int cx, int cy, int rad, int thickness) {
    for (int t = 0; t < thickness; t++) {
        int rr = rad - t;
        if (rr <= 0) break;
        for (int i = 0; i < 360; i++) {
            float a = (float)i * TWO_PI / 360.f;
            SDL_RenderDrawPoint(r, cx + (int)(cosf(a)*rr), cy + (int)(sinf(a)*rr));
        }
    }
}

// ── Init ───────────────────────────────────────────────────────────────────
static void initWisps(GameState& gs) {
    float cx = WINDOW_W / 2.f, cy = WINDOW_H / 2.f;
    for (int i = 0; i < MAX_WISPS; i++) {
        Wisp& w      = gs.wisps[i];
        w.angle      = randf(0, TWO_PI);
        w.radius     = randf(120.f, 320.f);
        w.speed      = randf(0.08f, 0.25f) * (rand()%2 ? 1.f : -1.f);
        w.phase      = randf(0, TWO_PI);
        w.pulseSpeed = randf(0.5f, 2.f);
        w.alpha      = randf(0.3f, 1.f);
        w.x = cx + cosf(w.angle) * w.radius;
        w.y = cy + sinf(w.angle) * w.radius;
    }
}

static void spawnParticle(GameState& gs, float x, float y) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle& p = gs.particles[i];
        if (p.active) continue;
        p.x = x + randf(-4,4); p.y = y + randf(-4,4);
        p.vx = randf(-30.f,30.f); p.vy = randf(-60.f,-10.f);
        p.life = p.maxLife = randf(0.8f, 2.5f);
        p.size = randf(1.f, 3.f);
        p.col  = { (Uint8)randf(80,180),(Uint8)randf(30,80),(Uint8)randf(180,255),200 };
        p.active = true;
        break;
    }
}

// ── Update ─────────────────────────────────────────────────────────────────
static void update(GameState& gs, float dt) {
    if (gs.phase != Phase::WATCHING) return;

    gs.elapsed   += dt;
    gs.voidPulse += dt * 0.7f;
    gs.voidRot   += dt * 0.03f;
    gs.sanity     = clampf(1.f - gs.elapsed / GOAL_SECONDS, 0.f, 1.f);
    gs.shakeAmt   = (1.f - gs.sanity) * 4.f;
    gs.shakeX     = randf(-gs.shakeAmt, gs.shakeAmt);
    gs.shakeY     = randf(-gs.shakeAmt, gs.shakeAmt);
    gs.vignPulse += dt * lerp(0.4f, 1.8f, 1.f - gs.sanity);

    gs.gazeTimer -= dt;
    if (gs.gazeTimer <= 0.f) {
        gs.targetPX = randf(-40.f, 40.f);
        gs.targetPY = randf(-20.f, 20.f);
        gs.gazeTimer = randf(1.5f, 4.f);
    }
    gs.pupilX = lerp(gs.pupilX, gs.targetPX, 3.f * dt);
    gs.pupilY = lerp(gs.pupilY, gs.targetPY, 3.f * dt);

    EyeState& e = gs.eye;
    e.timer += dt;
    if (!e.closing && e.timer >= e.nextBlink) {
        e.closing = true; e.timer = 0.f; e.nextBlink = randf(3.f, 8.f);
    }
    if (e.closing) {
        float half = 0.06f;
        if      (e.timer < half)      e.blinkT = 1.f - e.timer / half;
        else if (e.timer < half*2.f)  e.blinkT = (e.timer - half) / half;
        else { e.blinkT = 1.f; e.closing = false; e.timer = 0.f; }
    }

    float cx = WINDOW_W/2.f, cy = WINDOW_H/2.f;
    for (int i = 0; i < MAX_WISPS; i++) {
        Wisp& w  = gs.wisps[i];
        w.angle += w.speed * dt;
        float drift = sinf(gs.elapsed * 0.3f + w.phase) * 20.f;
        w.x = cx + cosf(w.angle) * (w.radius + drift);
        w.y = cy + sinf(w.angle) * (w.radius * 0.55f + drift * 0.4f);
        w.alpha = 0.4f + 0.6f*(sinf(gs.elapsed*w.pulseSpeed+w.phase)*0.5f+0.5f);
        if (rand() % 6 == 0) spawnParticle(gs, w.x, w.y);
    }

    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle& p = gs.particles[i];
        if (!p.active) continue;
        p.life -= dt;
        if (p.life <= 0.f) { p.active = false; continue; }
        p.x += p.vx * dt; p.y += p.vy * dt; p.vy -= 10.f * dt;
    }

    if (gs.elapsed >= GOAL_SECONDS) gs.phase = Phase::WON;
}

// ── Render ─────────────────────────────────────────────────────────────────
static void renderVignette(SDL_Renderer* r, float sanity, float pulse) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    float maxA = lerp(120.f, 240.f, 1.f - sanity) + 20.f * sinf(pulse);
    for (int i = 80; i >= 0; i--) {
        float t = (float)i / 80.f;
        SDL_SetRenderDrawColor(r, 0, 0, 0, (Uint8)clampf(maxA*t*t*t, 0, 255));
        SDL_Rect rc = { (int)(WINDOW_W*(1.f-t)*0.5f), (int)(WINDOW_H*(1.f-t)*0.5f),
                        (int)(WINDOW_W*t), (int)(WINDOW_H*t) };
        SDL_RenderDrawRect(r, &rc);
    }
}

static void renderEye(SDL_Renderer* r, const GameState& gs) {
    int   cx = WINDOW_W/2 + (int)gs.shakeX;
    int   cy = WINDOW_H/2 + (int)gs.shakeY;
    float bt = gs.eye.blinkT;
    float ps = 1.f + 0.04f * sinf(gs.voidPulse);
    int   eW = (int)(220*ps), eH = (int)(110*bt*ps);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    setColor(r, C_VOID);
    for (int dy = -eH; dy <= eH; dy++) {
        float f = (float)dy / (eH>0?eH:1);
        int   dx = (int)(eW * sqrtf(clampf(1.f-f*f,0.f,1.f)));
        SDL_RenderDrawLine(r, cx-dx, cy+dy, cx+dx, cy+dy);
    }
    for (int ri = 4; ri >= 0; ri--) {
        float t = (float)ri/4.f;
        int   ir = (int)(lerp(30.f,80.f,1.f-t)*bt);
        SDL_SetRenderDrawColor(r, C_RING.r, C_RING.g, C_RING.b, (Uint8)(80+120*t));
        for (int dy = -ir; dy <= ir; dy++) {
            float ef = (float)dy/(eH>0?eH:1); if(fabsf(ef)>1.f) continue;
            int   emx = (int)(eW*sqrtf(clampf(1.f-ef*ef,0.f,1.f)));
            float irf = (float)dy/(ir>0?ir:1);
            int   idx = (int)(ir*sqrtf(clampf(1.f-irf*irf,0.f,1.f)));
            int x0=std::max(cx-idx,cx-emx), x1=std::min(cx+idx,cx+emx);
            if(x0<x1) SDL_RenderDrawLine(r,x0,cy+dy,x1,cy+dy);
        }
    }
    SDL_SetRenderDrawColor(r, 60,20,120,80);
    int iR = (int)(70*bt);
    for (int li = 0; li < 12; li++) {
        float a = gs.voidRot + (float)li*TWO_PI/12.f;
        SDL_RenderDrawLine(r,
            cx+(int)(cosf(a)*18*bt), cy+(int)(sinf(a)*10*bt),
            cx+(int)(cosf(a)*iR),    cy+(int)(sinf(a)*iR*0.5f));
    }
    int px=(int)(cx+gs.pupilX*bt), py=(int)(cy+gs.pupilY*bt);
    int pr=(int)(32*bt*ps);
    SDL_SetRenderDrawColor(r,0,0,0,255);       fillCircle(r,px,py,pr);
    SDL_SetRenderDrawColor(r,40,10,80,160);    fillCircle(r,px,py,pr/2);
    SDL_SetRenderDrawColor(r,180,160,255,120); fillCircle(r,px-pr/4,py-pr/4,4);

    if (bt < 1.f) {
        int lidH = (int)((1.f-bt)*eH+eH);
        SDL_SetRenderDrawColor(r, C_BG.r, C_BG.g, C_BG.b, 255);
        for (int dy=-lidH; dy<=-eH-1; dy++) {
            float f=(float)dy/lidH;
            int dx=(int)(eW*sqrtf(clampf(1.f-f*f,0.f,1.f)));
            SDL_RenderDrawLine(r,cx-dx,cy+dy,cx+dx,cy+dy);
        }
        for (int dy=eH+1; dy<=lidH; dy++) {
            float f=(float)dy/lidH;
            int dx=(int)(eW*sqrtf(clampf(1.f-f*f,0.f,1.f)));
            SDL_RenderDrawLine(r,cx-dx,cy+dy,cx+dx,cy+dy);
        }
    }
    for (int gi=1; gi<=5; gi++) {
        float gp=(float)gi/5.f;
        SDL_SetRenderDrawColor(r,60,20,120,
            (Uint8)(40*(1.f-gp)*(0.6f+0.4f*sinf(gs.voidPulse+gp))));
        drawRing(r, cx, cy, eW+gi*18, 2);
    }
}

static void renderWisps(SDL_Renderer* r, const GameState& gs) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int i=0; i<MAX_WISPS; i++) {
        const Wisp& w = gs.wisps[i];
        Uint8 a = (Uint8)(w.alpha*160);
        for (int s=6; s>=1; s--) {
            SDL_SetRenderDrawColor(r,C_WISP.r,C_WISP.g,C_WISP.b,a/(Uint8)(s*2));
            fillCircle(r,(int)w.x,(int)w.y,s*2);
        }
    }
}

static void renderParticles(SDL_Renderer* r, const GameState& gs) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int i=0; i<MAX_PARTICLES; i++) {
        const Particle& p = gs.particles[i];
        if (!p.active) continue;
        float t = p.life/p.maxLife;
        SDL_SetRenderDrawColor(r,p.col.r,p.col.g,p.col.b,(Uint8)(t*p.col.a));
        int s=(int)(p.size*t+0.5f);
        SDL_Rect rc={(int)p.x-s,(int)p.y-s,s*2,s*2};
        SDL_RenderFillRect(r,&rc);
    }
}

static void renderText(SDL_Renderer* r, TTF_Font* font, const std::string& text,
                       int x, int y, Col col, bool centered=false) {
    SDL_Color sc={col.r,col.g,col.b,col.a};
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font,text.c_str(),sc);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r,surf);
    SDL_Rect dst={x,y,surf->w,surf->h};
    if (centered) dst.x -= surf->w/2;
    SDL_FreeSurface(surf);
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
    SDL_RenderCopy(r,tex,nullptr,&dst);
    SDL_DestroyTexture(tex);
}

static void renderHUD(SDL_Renderer* r, TTF_Font* fL, TTF_Font* fS, const GameState& gs) {
    float rem = clampf(GOAL_SECONDS-gs.elapsed, 0.f, GOAL_SECONDS);

    if (gs.phase == Phase::WON) {
        renderText(r,fL,"You endured the Abyss.",WINDOW_W/2,WINDOW_H/2-60,C_WIN,true);
        renderText(r,fS,"20 minutes. The void blinked first.",WINDOW_W/2,WINDOW_H/2+10,C_DIM,true);
        renderText(r,fS,"Press ESC to leave.",WINDOW_W/2,WINDOW_H/2+50,C_DIM,true);
        return;
    }

    renderText(r,fS,"Abyss.",30,24,C_DIM);

    char tbuf[16]; snprintf(tbuf,sizeof(tbuf),"%02d:%02d",(int)rem/60,(int)rem%60);
    renderText(r,fL,tbuf,WINDOW_W/2,20,rem<60.f?C_WARN:C_TEXT,true);

    int bW=300,bH=3,bX=WINDOW_W/2-150,bY=70;
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r,40,20,70,180);
    SDL_Rect bg={bX,bY,bW,bH}; SDL_RenderFillRect(r,&bg);
    SDL_SetRenderDrawColor(r,C_WISP.r,C_WISP.g,C_WISP.b,200);
    SDL_Rect fg={bX,bY,(int)(bW*gs.elapsed/GOAL_SECONDS),bH}; SDL_RenderFillRect(r,&fg);

    float ins = 1.f - gs.sanity;
    if (ins > 0.2f) {
        Uint8 ha=(Uint8)(clampf((ins-0.2f)/0.8f,0.f,1.f)*160.f);
        Col hc={180,120,255,ha};
        const char* hints[]={"it is looking at you","don't look away",
                              "you are still here","the void remembers","almost"};
        renderText(r,fS,hints[(int)(gs.elapsed/(GOAL_SECONDS/5))%5],
                   WINDOW_W/2,WINDOW_H-60,hc,true);
    }
    renderText(r,fS,"ESC - give up",30,WINDOW_H-40,C_DIM);
}

// ── Entry point ────────────────────────────────────────────────────────────
int main(int, char**) {
    srand((unsigned)time(nullptr));

    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)!=0) {
        fprintf(stderr,"SDL_Init: %s\n",SDL_GetError()); return 1;
    }
    if (TTF_Init()!=0) {
        fprintf(stderr,"TTF_Init: %s\n",TTF_GetError()); return 1;
    }

    SDL_Window* win = SDL_CreateWindow("Abyss.",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
        WINDOW_W,WINDOW_H, SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
    if (!win) { fprintf(stderr,"Window: %s\n",SDL_GetError()); return 1; }

    SDL_Renderer* ren = SDL_CreateRenderer(win,-1,
        SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if (!ren) { fprintf(stderr,"Renderer: %s\n",SDL_GetError()); return 1; }
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);

    // Font loading — Windows paths included for cross-platform convenience
    TTF_Font* fL=nullptr, *fS=nullptr;
    const char* fonts[]={
        // Linux
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf",
        // macOS
        "/Library/Fonts/Arial Bold.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        // Windows (relative — works when run from game dir)
        "C:/Windows/Fonts/arialbd.ttf",
        "C:/Windows/Fonts/calibrib.ttf",
        nullptr
    };
    for (int fi=0; fonts[fi]&&!fL; fi++) {
        fL = TTF_OpenFont(fonts[fi],42);
        fS = TTF_OpenFont(fonts[fi],18);
    }
    if (!fL) { fprintf(stderr,"No font found: %s\n",TTF_GetError()); return 1; }

    discordInit();

    GameState gs{};
    initWisps(gs);
    float discordTimer=0.f;
    Uint32 prev=SDL_GetTicks();
    bool running=true;

    while (running) {
        Uint32 now=SDL_GetTicks();
        float dt=clampf((now-prev)/1000.f,0.f,0.05f);
        prev=now;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type==SDL_QUIT) running=false;
            if (ev.type==SDL_KEYDOWN) {
                if (ev.key.keysym.sym==SDLK_ESCAPE) {
                    if (gs.phase==Phase::WATCHING) gs.phase=Phase::GAVE_UP;
                    else running=false;
                }
                if (ev.key.keysym.sym==SDLK_RETURN && gs.phase!=Phase::WATCHING)
                    running=false;
            }
        }

        update(gs,dt);

        discordTimer+=dt;
        if (discordTimer>=5.f) {
            discordUpdate(clampf(GOAL_SECONDS-gs.elapsed,0.f,GOAL_SECONDS),gs.phase);
            discordTimer=0.f;
        }

        setColor(ren,C_BG); SDL_RenderClear(ren);

        if (gs.phase==Phase::WATCHING) {
            SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
            for (int i=20;i>=0;i--) {
                float t=(float)i/20.f;
                SDL_SetRenderDrawColor(ren,20,5,50,(Uint8)(30*t));
                int p2=(int)(t*200.f);
                SDL_Rect rr={p2,p2,WINDOW_W-p2*2,WINDOW_H-p2*2};
                SDL_RenderFillRect(ren,&rr);
            }
            renderWisps(ren,gs);
            renderEye(ren,gs);
            renderParticles(ren,gs);
            renderVignette(ren,gs.sanity,gs.vignPulse);
        } else if (gs.phase==Phase::WON) {
            SDL_SetRenderDrawColor(ren,10,4,24,255); SDL_RenderClear(ren);
            renderWisps(ren,gs);
            renderVignette(ren,0.6f,gs.vignPulse);
        } else {
            SDL_SetRenderDrawColor(ren,2,2,6,255); SDL_RenderClear(ren);
            renderText(ren,fL,"You looked away.",WINDOW_W/2,WINDOW_H/2-40,C_WARN,true);
            renderText(ren,fS,"The Abyss is patient.",WINDOW_W/2,WINDOW_H/2+20,C_DIM,true);
            renderText(ren,fS,"Press ENTER or ESC to exit.",WINDOW_W/2,WINDOW_H/2+60,C_DIM,true);
        }

        renderHUD(ren,fL,fS,gs);
        SDL_RenderPresent(ren);
    }

    discordShutdown();
    TTF_CloseFont(fS); TTF_CloseFont(fL);
    TTF_Quit();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
