/**
 * Cube Slice VR - A Fruit Ninja-inspired VR Arcade Game
 *
 * Players slice floating Rubik's Cube-style objects with virtual blades.
 * Gently tapping a cube flips it upward, increasing score multiplier.
 * Slice with a fast swing to earn points!
 *
 * Controls:
 *   - Swing controllers to slice cubes (fast swing = slice)
 *   - Gently tap cubes to flip/juggle them (slow tap = flip, +1x multiplier)
 *   - Press A button to restart after game over
 *
 * Scoring:
 *   - Base score per slice = 100
 *   - Each flip before slicing adds +1x multiplier
 *   - Consecutive slices build a combo (resets after 3 seconds)
 */

#include "realitylib_vr.h"
#include "realitylib_text.h"
#include <android/log.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define PI M_PI

#define LOG_TAG "CubeSliceVR"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// =============================================================================
// Game Constants
// =============================================================================

#define MAX_CUBES           12
#define MAX_FRAGMENTS       200
#define BLADE_LENGTH        0.8f
#define BLADE_TRAIL_SIZE    12

// Rubik's cube appearance
#define CUBE_BLOCK_SIZE     0.065f
#define CUBE_GAP            0.008f
#define CUBE_GRID_STEP      (CUBE_BLOCK_SIZE + CUBE_GAP)
#define CUBE_TOTAL_SIZE     (CUBE_GRID_STEP * 3.0f)

// Spawning
#define SPAWN_RADIUS        1.4f
#define SPAWN_HEIGHT       -0.3f
#define LAUNCH_SPEED_MIN    3.0f
#define LAUNCH_SPEED_MAX    4.8f

// Physics
#define MISS_HEIGHT        -1.0f
#define GAME_GRAVITY       -3.0f

// Collision
#define SLICE_SPEED_THRESH  1.5f
#define FLIP_SPEED_MIN      0.3f
#define FLIP_SPEED_MAX      1.5f
#define HIT_DISTANCE        0.30f
#define FLIP_COOLDOWN       0.25f

// Scoring & game flow
#define BASE_SCORE          100
#define MAX_LIVES           3
#define FRAGMENT_LIFETIME   2.0f
#define COMBO_TIMEOUT       3.0f
#define SPAWN_INTERVAL_INIT 2.0f
#define SPAWN_INTERVAL_MIN  0.5f
#define DIFFICULTY_RAMP_SEC 120.0f
#define RESTART_DELAY       2.0f

// =============================================================================
// Data Structures
// =============================================================================

typedef enum {
    STATE_PLAYING,
    STATE_GAME_OVER
} GamePhase;

typedef enum {
    CUBE_INACTIVE = 0,
    CUBE_FLYING
} CubeState;

typedef struct {
    Vector3   position;
    Vector3   velocity;
    float     rotationY;        // Y-axis rotation (radians)
    float     rotationX;        // X-axis rotation (radians)
    float     rotationSpeedY;
    float     rotationSpeedX;
    int       flipCount;
    CubeState state;
    float     lifetime;
    float     flashTimer;       // White flash after flip
    float     hitCooldown;      // Prevents re-hit right after flip
    Color     color;
    bool      active;
} SliceCube;

typedef struct {
    Vector3 position;
    Vector3 velocity;
    float   size;
    Color   color;
    float   lifetime;
    bool    active;
} Fragment;

typedef struct {
    Vector3 position;
    bool    valid;
} TrailPoint;

typedef struct {
    Vector3    tipPosition;
    Vector3    prevTipPosition;
    Vector3    tipVelocity;
    float      speed;
    TrailPoint trail[BLADE_TRAIL_SIZE];
    int        trailIndex;
    bool       tracking;
    bool       hasPrevTip;
} BladeState;

typedef struct {
    SliceCube  cubes[MAX_CUBES];
    Fragment   fragments[MAX_FRAGMENTS];
    BladeState blades[2];           // 0 = left, 1 = right

    GamePhase  phase;
    int        score;
    int        lives;
    int        totalSliced;
    int        totalMissed;
    int        bestCombo;
    int        currentCombo;
    float      comboTimer;

    float      spawnTimer;
    float      gameTime;
    float      deltaTime;
    float      gameOverTimer;

    Vector3    gameCenter;          // Player's XZ position at game start (stage space)
    float      gameFacing;          // Player's yaw at game start (radians, 0 = facing -Z)
    bool       gameCenterValid;     // True once gameCenter has been captured from a valid headset pose

    bool       initialized;
    bool       handTrackingEnabled;
} GameState;

// =============================================================================
// Globals
// =============================================================================

static GameState game = {0};

// 3x3x3 block offsets, center (0,0,0) removed -> 26 blocks
#define RUBIK_COUNT 26
static const int rubikOff[RUBIK_COUNT][3] = {
    {-1,-1,-1},{-1,-1, 0},{-1,-1, 1},
    {-1, 0,-1},{-1, 0, 0},{-1, 0, 1},
    {-1, 1,-1},{-1, 1, 0},{-1, 1, 1},
    { 0,-1,-1},{ 0,-1, 0},{ 0,-1, 1},
    { 0, 0,-1},           { 0, 0, 1},
    { 0, 1,-1},{ 0, 1, 0},{ 0, 1, 1},
    { 1,-1,-1},{ 1,-1, 0},{ 1,-1, 1},
    { 1, 0,-1},{ 1, 0, 0},{ 1, 0, 1},
    { 1, 1,-1},{ 1, 1, 0},{ 1, 1, 1},
};

// =============================================================================
// Math Helpers
// =============================================================================

static int ClampI(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static float Clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Calculate closest distance from point to line segment
static float DistancePointToSegment(Vector3 point, Vector3 segA, Vector3 segB) {
    Vector3 ab = Vector3Subtract(segB, segA);
    Vector3 ap = Vector3Subtract(point, segA);
    
    float abLen2 = ab.x * ab.x + ab.y * ab.y + ab.z * ab.z;
    if (abLen2 < 0.0001f) {
        // Segment is essentially a point
        return Vector3Distance(point, segA);
    }
    
    float t = (ap.x * ab.x + ap.y * ab.y + ap.z * ab.z) / abLen2;
    t = Clampf(t, 0.0f, 1.0f);
    
    Vector3 closest = Vector3Create(
        segA.x + t * ab.x,
        segA.y + t * ab.y,
        segA.z + t * ab.z
    );
    
    return Vector3Distance(point, closest);
}

// =============================================================================
// Random Number Generator
// =============================================================================

static unsigned int rngSeed = 42;

static float RandFloat(void) {
    rngSeed = rngSeed * 1103515245 + 12345;
    return (float)(rngSeed % 10000) / 10000.0f;
}

static float RandRange(float lo, float hi) {
    return lo + RandFloat() * (hi - lo);
}

static Color RandBrightColor(void) {
    static const Color palette[] = {
        {255,  50,  50, 255},   // Red
        { 50, 150, 255, 255},   // Blue
        { 50, 255,  50, 255},   // Green
        {255, 200,   0, 255},   // Gold
        {255, 100,   0, 255},   // Orange
        {200,  50, 255, 255},   // Purple
        {  0, 255, 200, 255},   // Cyan
        {255,  50, 200, 255},   // Pink
    };
    int idx = ClampI((int)(RandFloat() * 8.0f), 0, 7);
    return palette[idx];
}

// =============================================================================
// Initialization
// =============================================================================

static void InitGame(void) {
    bool ht = game.handTrackingEnabled;
    memset(&game, 0, sizeof(GameState));
    game.handTrackingEnabled = ht;

    game.phase       = STATE_PLAYING;
    game.lives       = MAX_LIVES;
    game.spawnTimer  = 1.0f;
    game.deltaTime   = 1.0f / 72.0f;
    game.initialized     = true;
    game.gameCenterValid = false;  // Will be captured once headset tracking is valid

    // Player stands still - all gameplay in stage space
    SetPlayerPosition(Vector3Create(0, 0, 0));
    SetPlayerYaw(0);

    LOGI("=== CUBE SLICE VR - Game started! ===");
}

// =============================================================================
// Cube Spawning
// =============================================================================

static void SpawnCube(void) {
    int slot = -1;
    for (int i = 0; i < MAX_CUBES; i++) {
        if (!game.cubes[i].active) { slot = i; break; }
    }
    if (slot < 0) return;

    SliceCube* c = &game.cubes[slot];
    memset(c, 0, sizeof(SliceCube));

    // Spawn in an arc in front of the player (~120° cone)
    float angle  = RandRange(-PI * 0.33f, PI * 0.33f) + game.gameFacing;
    float radius = RandRange(1.0f, SPAWN_RADIUS);

    c->position = Vector3Create(
        game.gameCenter.x + sinf(angle) * radius,
        SPAWN_HEIGHT,
        game.gameCenter.z - cosf(angle) * radius
    );

    // Calculate lateral velocity based on spawn offset from center
    // Cubes spawn farther from center get more lateral velocity toward center
    float offsetX = c->position.x - game.gameCenter.x;
    float offsetZ = c->position.z - game.gameCenter.z;
    float offsetDist = sqrtf(offsetX * offsetX + offsetZ * offsetZ);
    
    // Lateral speed proportional to distance from center
    float lateralSpeed = offsetDist * 0.9f;
    
    // Direction points toward center (-offset direction)
    float lateralDirX = 0.0f;
    float lateralDirZ = 0.0f;
    if (offsetDist > 0.001f) {
        lateralDirX = -offsetX / offsetDist;
        lateralDirZ = -offsetZ / offsetDist;
    }
    
    // Main upward velocity
    float speed = RandRange(LAUNCH_SPEED_MIN, LAUNCH_SPEED_MAX);
    
    c->velocity = Vector3Create(
        lateralDirX * lateralSpeed,
        speed,
        lateralDirZ * lateralSpeed
    );

    c->rotationY      = RandFloat() * 2.0f * PI;
    c->rotationX      = RandFloat() * 2.0f * PI;
    c->rotationSpeedY = RandRange(2.0f, 6.0f) * (RandFloat() > 0.5f ? 1.0f : -1.0f);
    c->rotationSpeedX = RandRange(1.5f, 4.0f) * (RandFloat() > 0.5f ? 1.0f : -1.0f);
    c->color         = RandBrightColor();
    c->state         = CUBE_FLYING;
    c->active        = true;
}

// =============================================================================
// Fragment Explosion on Slice
// =============================================================================

static void SpawnFragments(SliceCube* cube, Vector3 bladeVelocity) {
    for (int i = 0; i < RUBIK_COUNT; i++) {
        int slot = -1;
        for (int j = 0; j < MAX_FRAGMENTS; j++) {
            if (!game.fragments[j].active) { slot = j; break; }
        }
        if (slot < 0) break;

        Fragment* f = &game.fragments[slot];

        // Block offset, rotated by cube's current Y rotation
        float ox = rubikOff[i][0] * CUBE_GRID_STEP;
        float oy = rubikOff[i][1] * CUBE_GRID_STEP;
        float oz = rubikOff[i][2] * CUBE_GRID_STEP;

        float cosA = cosf(cube->rotationY);
        float sinA = sinf(cube->rotationY);
        float rx =  ox * cosA + oz * sinA;
        float rz = -ox * sinA + oz * cosA;

        f->position = Vector3Add(cube->position, Vector3Create(rx, oy, rz));

        // Velocity: cube momentum + blade influence + outward scatter
        Vector3 outward = Vector3Normalize(Vector3Create(rx, oy, rz));
        f->velocity = Vector3Add(
            Vector3Add(cube->velocity, Vector3Scale(bladeVelocity, 0.3f)),
            Vector3Scale(outward, RandRange(1.0f, 3.0f))
        );
        f->velocity.x += RandRange(-1.0f, 1.0f);
        f->velocity.y += RandRange(-0.5f, 1.5f);
        f->velocity.z += RandRange(-1.0f, 1.0f);

        f->size = CUBE_BLOCK_SIZE * RandRange(0.6f, 1.0f);

        // Color with slight variation
        int cr = cube->color.r + (int)RandRange(-30, 30);
        int cg = cube->color.g + (int)RandRange(-30, 30);
        int cb = cube->color.b + (int)RandRange(-30, 30);
        f->color = (Color){
            (unsigned char)ClampI(cr, 0, 255),
            (unsigned char)ClampI(cg, 0, 255),
            (unsigned char)ClampI(cb, 0, 255),
            255
        };

        f->lifetime = FRAGMENT_LIFETIME;
        f->active   = true;
    }
}

// Spawn golden score particles floating upward
static void SpawnScoreEffect(Vector3 pos, int count) {
    for (int i = 0; i < count; i++) {
        int slot = -1;
        for (int j = 0; j < MAX_FRAGMENTS; j++) {
            if (!game.fragments[j].active) { slot = j; break; }
        }
        if (slot < 0) break;

        Fragment* f = &game.fragments[slot];
        f->position = Vector3Add(pos, Vector3Create(
            RandRange(-0.05f, 0.05f),
            RandRange(0, 0.05f),
            RandRange(-0.05f, 0.05f)
        ));
        f->velocity = Vector3Create(
            RandRange(-0.3f, 0.3f),
            RandRange(1.0f, 2.5f),
            RandRange(-0.3f, 0.3f)
        );
        f->size     = 0.02f;
        f->color    = GOLD;
        f->lifetime = 1.5f;
        f->active   = true;
    }
}

// =============================================================================
// Drawing - Rubik's Cube
// =============================================================================

static void DrawRubikCube(Vector3 center, float rotY, float rotX, Color color, float flash) {
    float step = CUBE_GRID_STEP;
    float bs   = CUBE_BLOCK_SIZE;

    float cosY = cosf(rotY), sinY = sinf(rotY);
    float cosX = cosf(rotX), sinX = sinf(rotX);

    for (int i = 0; i < RUBIK_COUNT; i++) {
        float ox = rubikOff[i][0] * step;
        float oy = rubikOff[i][1] * step;
        float oz = rubikOff[i][2] * step;

        // Rotate around X axis first
        float y1 =  oy * cosX - oz * sinX;
        float z1 =  oy * sinX + oz * cosX;

        // Then rotate around Y axis
        float rx =  ox * cosY + z1 * sinY;
        float ry =  y1;
        float rz = -ox * sinY + z1 * cosY;

        Vector3 blockPos = Vector3Add(center, Vector3Create(rx, ry, rz));

        Color bc = color;
        if (flash > 0.0f) {
            bc.r = (unsigned char)ClampI((int)(bc.r + 200.0f * flash), 0, 255);
            bc.g = (unsigned char)ClampI((int)(bc.g + 200.0f * flash), 0, 255);
            bc.b = (unsigned char)ClampI((int)(bc.b + 200.0f * flash), 0, 255);
        }
        DrawVRCube(blockPos, bs, bc);
    }
}

// =============================================================================
// Drawing - Blades & Trails
// =============================================================================

static Color GetBladeColor(int hand) {
    if (game.currentCombo >= 5)  return MAGENTA;
    if (game.currentCombo >= 3)  return ORANGE;
    if (game.currentCombo >= 1)  return YELLOW;
    return (hand == 0) ? SKYBLUE : LIME;
}

static void DrawBlade(int hand, VRController ctrl) {
    if (!ctrl.isTracking) return;

    Vector3 forward  = QuaternionForward(ctrl.orientation);
    Vector3 bladeDir = Vector3Scale(forward, -1.0f);
    Vector3 bladeEnd = Vector3Add(ctrl.position, Vector3Scale(bladeDir, BLADE_LENGTH));

    Color col = GetBladeColor(hand);

    // Handle
    DrawVRSphere(ctrl.position, 0.02f, GRAY);

    // Blade line
    DrawVRLine3D(ctrl.position, bladeEnd, col);

    // Trail
    BladeState* b = &game.blades[hand];
    for (int i = 0; i < BLADE_TRAIL_SIZE - 1; i++) {
        int idx  = (b->trailIndex - i - 1 + BLADE_TRAIL_SIZE) % BLADE_TRAIL_SIZE;
        int next = (idx - 1 + BLADE_TRAIL_SIZE) % BLADE_TRAIL_SIZE;
        if (!b->trail[idx].valid || !b->trail[next].valid) continue;

        float alpha = 1.0f - (float)(i + 1) / BLADE_TRAIL_SIZE;
        Color tc = {
            (unsigned char)(col.r * alpha),
            (unsigned char)(col.g * alpha),
            (unsigned char)(col.b * alpha),
            255
        };
        DrawVRLine3D(b->trail[idx].position, b->trail[next].position, tc);
    }
}

// =============================================================================
// Drawing - Fragments
// =============================================================================

static void DrawFragments(void) {
    for (int i = 0; i < MAX_FRAGMENTS; i++) {
        Fragment* f = &game.fragments[i];
        if (!f->active) continue;

        float fade = Clampf(f->lifetime / (FRAGMENT_LIFETIME * 0.3f), 0.0f, 1.0f);
        Color c = {
            (unsigned char)(f->color.r * fade),
            (unsigned char)(f->color.g * fade),
            (unsigned char)(f->color.b * fade),
            255
        };
        DrawVRCube(f->position, f->size, c);
    }
}

// =============================================================================
// Drawing - Environment
// =============================================================================

static void DrawEnvironment(void) {
    float gcx = game.gameCenter.x;
    float gcz = game.gameCenter.z;

    // Reference grid centered on player
    DrawVRGrid(16, 0.5f);

    // Ground plane at miss-line height
    DrawVRPlane(
        Vector3Create(gcx, MISS_HEIGHT - 0.01f, gcz),
        Vector3Create(10, 0, 10),
        (Color){20, 10, 10, 255}
    );

    // Danger-line ring at miss height, centered on player
    int segments = 24;
    float ringR  = SPAWN_RADIUS + 0.5f;
    for (int i = 0; i < segments; i++) {
        float a0 = (float)i       / segments * 2.0f * PI;
        float a1 = (float)(i + 1) / segments * 2.0f * PI;
        Vector3 p0 = Vector3Create(gcx + cosf(a0) * ringR, MISS_HEIGHT, gcz + sinf(a0) * ringR);
        Vector3 p1 = Vector3Create(gcx + cosf(a1) * ringR, MISS_HEIGHT, gcz + sinf(a1) * ringR);
        DrawVRLine3D(p0, p1, (Color){100, 30, 30, 255});
    }

    // Ambient pillars for spatial reference, around player
    for (int i = 0; i < 6; i++) {
        float a   = (float)i / 6.0f * 2.0f * PI;
        float r   = SPAWN_RADIUS + 1.5f;
        Vector3 p = Vector3Create(gcx + cosf(a) * r, 1.0f, gcz + sinf(a) * r);
        DrawVRCuboid(p, Vector3Create(0.08f, 2.5f, 0.08f),
                     Vector3Create(0.15f, 0.15f, 0.25f));

        // Small pulsing light on top
        Vector3 lp = p;
        lp.y = 2.3f;
        float pulse = (sinf(game.gameTime * 2.0f + a) + 1.0f) * 0.5f;
        DrawVRCuboid(lp, Vector3Create(0.06f, 0.06f, 0.06f),
                     Vector3Create(0.2f + pulse * 0.3f, 0.1f, 0.4f));
    }
}

// =============================================================================
// Drawing - HUD (Score, Lives, Combo)
// =============================================================================

static void DrawHUD(void) {
    // Place HUD 1m in front of the player, facing them
    float hudDist = 1.0f;
    float sinF = sinf(game.gameFacing);
    float cosF = cosf(game.gameFacing);
    float hudCx = game.gameCenter.x + sinF * hudDist;
    float hudCz = game.gameCenter.z - cosF * hudDist;

    // "right" direction on the HUD plane
    float rX = cosF;
    float rZ = sinF;

    float labelY = 2.15f;
    float valueY = 2.02f;
    float lpix   = 0.010f;   // label pixel size
    float vpix   = 0.015f;   // value pixel size

    // --- SCORE (center) ---
    DrawTextCentered("SCORE", hudCx, labelY, hudCz, lpix, GRAY, game.gameFacing);
    DrawNumberCentered(game.score, hudCx, valueY, hudCz, vpix, GOLD, game.gameFacing);

    // --- LIVES (left of center) ---
    float lOff = -0.50f;
    float lx = hudCx + lOff * rX;
    float lz = hudCz + lOff * rZ;
    DrawPixelText("LIVES", Vector3Create(lx, labelY, lz), lpix, GRAY, game.gameFacing);
    DrawNumberAt(game.lives, Vector3Create(lx + 0.02f * rX, valueY, lz + 0.02f * rZ),
                 vpix, RED, game.gameFacing);

    // --- COMBO (right of center, when active) ---
    if (game.currentCombo > 0 && game.comboTimer > 0) {
        float fade = Clampf(game.comboTimer / COMBO_TIMEOUT, 0.2f, 1.0f);
        Color cc = ORANGE;
        cc.r = (unsigned char)(cc.r * fade);
        cc.g = (unsigned char)(cc.g * fade);
        cc.b = (unsigned char)(cc.b * fade);
        float cOff = 0.32f;
        float cx = hudCx + cOff * rX;
        float cz = hudCz + cOff * rZ;
        DrawPixelText("COMBO", Vector3Create(cx, labelY, cz), lpix, cc, game.gameFacing);
        DrawPixelText("X", Vector3Create(cx, valueY, cz), vpix, cc, game.gameFacing);
        float xw = 4.0f * vpix * 1.25f;
        DrawNumberAt(game.currentCombo,
                     Vector3Create(cx + xw * rX, valueY, cz + xw * rZ),
                     vpix, cc, game.gameFacing);
    }
}

// =============================================================================
// Drawing - Game Over Screen
// =============================================================================

static void DrawGameOverScreen(void) {
    float t   = Clampf(game.gameOverTimer, 0.0f, 1.0f);

    // Place game over screen 1.1m in front of the player
    float goDist = 1.1f;
    float sinF = sinf(game.gameFacing);
    float cosF = cosf(game.gameFacing);
    float goCx = game.gameCenter.x + sinF * goDist;
    float goCz = game.gameCenter.z - cosF * goDist;
    // Slightly farther for background decorations
    float bgCx = game.gameCenter.x + sinF * (goDist + 0.15f);
    float bgCz = game.gameCenter.z - cosF * (goDist + 0.15f);

    // Background sphere
    float pulse = 0.10f + sinf(game.gameTime * 3.0f) * 0.03f;
    DrawVRSphere(Vector3Create(bgCx, 1.5f, bgCz), pulse * t,
                 (Color){60, 10, 10, 255});

    // Decorative ring
    for (int i = 0; i < 12; i++) {
        float a  = (float)i / 12.0f * 2.0f * PI + game.gameTime;
        float rv = 0.5f * t;
        Vector3 p = Vector3Create(
            goCx + cosf(a) * rv,
            1.5f + sinf(a * 2.0f) * 0.1f,
            goCz + sinf(a) * rv * 0.3f);
        DrawVRCube(p, 0.025f * t, RED);
    }

    float tp = 0.020f * t;   // title pixel size
    float dp = 0.013f * t;   // detail pixel size
    float np = 0.016f * t;   // number pixel size

    // "GAME OVER"
    DrawTextCentered("GAME OVER", goCx, 1.82f, goCz, tp, RED, game.gameFacing);

    // Final score
    DrawTextCentered("SCORE", goCx, 1.60f, goCz, dp, GRAY, game.gameFacing);
    DrawNumberCentered(game.score, goCx, 1.48f, goCz, np, GOLD, game.gameFacing);

    // Best combo
    DrawTextCentered("BEST COMBO", goCx, 1.32f, goCz, dp, GRAY, game.gameFacing);
    DrawNumberCentered(game.bestCombo, goCx, 1.20f, goCz, np, ORANGE, game.gameFacing);

    // Total sliced
    DrawTextCentered("SLICED", goCx, 1.06f, goCz, dp, GRAY, game.gameFacing);
    DrawNumberCentered(game.totalSliced, goCx, 0.94f, goCz, np, SKYBLUE, game.gameFacing);

    // Restart prompt (after delay)
    if (game.gameOverTimer > RESTART_DELAY) {
        float blink = (sinf(game.gameTime * 5.0f) + 1.0f) * 0.5f;
        Color pr = {(unsigned char)(255 * blink), (unsigned char)(255 * blink),
                    (unsigned char)(255 * blink), 255};
        DrawTextCentered("PRESS A", goCx, 0.74f, goCz, dp, pr, game.gameFacing);
    }
}

// =============================================================================
// Physics Update
// =============================================================================

static void UpdatePhysics(void) {
    float dt = game.deltaTime;

    // Cubes
    for (int i = 0; i < MAX_CUBES; i++) {
        SliceCube* c = &game.cubes[i];
        if (!c->active || c->state != CUBE_FLYING) continue;

        c->velocity.y += GAME_GRAVITY * dt;
        c->position = Vector3Add(c->position, Vector3Scale(c->velocity, dt));
        c->rotationY += c->rotationSpeedY * dt;
        c->rotationX += c->rotationSpeedX * dt;
        c->lifetime += dt;
        if (c->flashTimer > 0) c->flashTimer -= dt;
        if (c->hitCooldown > 0) c->hitCooldown -= dt;

        // Missed - fell below threshold
        if (c->position.y < MISS_HEIGHT) {
            c->active = false;
            c->state  = CUBE_INACTIVE;

            if (game.phase == STATE_PLAYING) {
                game.lives--;
                game.totalMissed++;
                game.currentCombo = 0;
                game.comboTimer   = 0;

                TriggerVRHaptic(CONTROLLER_LEFT,  0.3f, 0.2f);
                TriggerVRHaptic(CONTROLLER_RIGHT, 0.3f, 0.2f);

                LOGI("MISS! Lives remaining: %d", game.lives);

                if (game.lives <= 0) {
                    game.phase         = STATE_GAME_OVER;
                    game.gameOverTimer = 0;
                    LOGI("GAME OVER! Score:%d  Sliced:%d  BestCombo:%d",
                         game.score, game.totalSliced, game.bestCombo);
                }
            }
        }
    }

    // Fragments
    for (int i = 0; i < MAX_FRAGMENTS; i++) {
        Fragment* f = &game.fragments[i];
        if (!f->active) continue;

        f->velocity.y += GAME_GRAVITY * 1.5f * dt;
        f->position = Vector3Add(f->position, Vector3Scale(f->velocity, dt));
        f->lifetime -= dt;
        f->size     *= 0.997f;

        if (f->lifetime <= 0 || f->position.y < -3.0f) {
            f->active = false;
        }
    }
}

// =============================================================================
// Blade State Update
// =============================================================================

static void UpdateBlades(void) {
    float dt = game.deltaTime;

    for (int hand = 0; hand < 2; hand++) {
        VRController ctrl = GetController(hand);
        BladeState*  b    = &game.blades[hand];

        b->tracking = ctrl.isTracking;
        if (!ctrl.isTracking) {
            b->hasPrevTip = false;
            continue;
        }

        Vector3 forward  = QuaternionForward(ctrl.orientation);
        Vector3 bladeDir = Vector3Scale(forward, -1.0f);
        b->tipPosition   = Vector3Add(ctrl.position, Vector3Scale(bladeDir, BLADE_LENGTH));

        if (b->hasPrevTip) {
            Vector3 delta  = Vector3Subtract(b->tipPosition, b->prevTipPosition);
            b->tipVelocity = Vector3Scale(delta, 1.0f / dt);
            b->speed       = Vector3Length(b->tipVelocity);
        } else {
            b->tipVelocity = Vector3Create(0, 0, 0);
            b->speed       = 0;
        }

        b->prevTipPosition = b->tipPosition;
        b->hasPrevTip      = true;

        // Trail
        b->trail[b->trailIndex].position = b->tipPosition;
        b->trail[b->trailIndex].valid    = true;
        b->trailIndex = (b->trailIndex + 1) % BLADE_TRAIL_SIZE;
    }
}

// =============================================================================
// Collision Detection - Slice & Flip
// =============================================================================

static void CheckCollisions(void) {
    if (game.phase != STATE_PLAYING) return;

    for (int hand = 0; hand < 2; hand++) {
        BladeState* b = &game.blades[hand];
        if (!b->tracking || !b->hasPrevTip) continue;
        
        VRController ctrl = GetController(hand);
        Vector3 bladeStart = ctrl.position;
        Vector3 bladeEnd = b->tipPosition;

        for (int i = 0; i < MAX_CUBES; i++) {
            SliceCube* c = &game.cubes[i];
            if (!c->active || c->state != CUBE_FLYING) continue;
            if (c->hitCooldown > 0) continue;

            // Check distance from cube to entire blade line segment
            float dist = DistancePointToSegment(c->position, bladeStart, bladeEnd);
            if (dist > HIT_DISTANCE) continue;

            if (b->speed >= SLICE_SPEED_THRESH) {
                // ==================== SLICE ====================
                int multiplier = 1 + c->flipCount;
                int points     = BASE_SCORE * multiplier;
                game.score += points;
                game.totalSliced++;
                game.currentCombo++;
                game.comboTimer = COMBO_TIMEOUT;

                if (game.currentCombo > game.bestCombo) {
                    game.bestCombo = game.currentCombo;
                }

                SpawnFragments(c, b->tipVelocity);
                SpawnScoreEffect(c->position, 3 + c->flipCount * 2);

                c->active = false;
                c->state  = CUBE_INACTIVE;

                float haptic = Clampf(0.3f + game.currentCombo * 0.1f, 0, 1);
                TriggerVRHaptic(hand, haptic, 0.15f);

                LOGI("SLICE! Flips:%d  x%d  +%d  Total:%d  Combo:%d",
                     c->flipCount, multiplier, points,
                     game.score, game.currentCombo);

            } else if (b->speed >= FLIP_SPEED_MIN && b->speed < FLIP_SPEED_MAX) {
                // ==================== FLIP ====================
                c->flipCount++;
                
                // Calculate direction toward game center
                float offsetX = c->position.x - game.gameCenter.x;
                float offsetZ = c->position.z - game.gameCenter.z;
                float offsetDist = sqrtf(offsetX * offsetX + offsetZ * offsetZ);
                
                // Lateral velocity toward center, proportional to distance
                float lateralSpeed = offsetDist * 0.5f;
                float lateralDirX = 0.0f;
                float lateralDirZ = 0.0f;
                if (offsetDist > 0.001f) {
                    lateralDirX = -offsetX / offsetDist;
                    lateralDirZ = -offsetZ / offsetDist;
                }
                
                // Set velocity with small random variation
                c->velocity.y = 2.0f;
                c->velocity.x = lateralDirX * lateralSpeed + RandRange(-0.2f, 0.2f);
                c->velocity.z = lateralDirZ * lateralSpeed + RandRange(-0.2f, 0.2f);
                c->flashTimer    = 0.3f;
                c->hitCooldown   = FLIP_COOLDOWN;
                c->rotationSpeedY *= 1.4f;
                c->rotationSpeedX *= 1.4f;
                c->color         = RandBrightColor();

                TriggerVRHaptic(hand, 0.15f, 0.08f);

                LOGI("FLIP! Cube %d now at x%d", i, 1 + c->flipCount);
            }
        }
    }
}

// =============================================================================
// Spawn Logic & Difficulty Ramp
// =============================================================================

static void UpdateSpawning(void) {
    if (game.phase != STATE_PLAYING) return;

    game.spawnTimer -= game.deltaTime;
    if (game.spawnTimer > 0) return;

    SpawnCube();

    // Ramp difficulty: faster spawning over time
    float progress = Clampf(game.gameTime / DIFFICULTY_RAMP_SEC, 0, 1);
    float interval = SPAWN_INTERVAL_INIT +
                     (SPAWN_INTERVAL_MIN - SPAWN_INTERVAL_INIT) * progress;
    game.spawnTimer = interval;

    // Chance for double spawn at higher difficulty
    if (progress > 0.3f && RandFloat() < progress * 0.3f) {
        SpawnCube();
    }
}

// =============================================================================
// Combo Timer
// =============================================================================

static void UpdateCombo(void) {
    if (game.comboTimer > 0) {
        game.comboTimer -= game.deltaTime;
        if (game.comboTimer <= 0) {
            game.currentCombo = 0;
        }
    }
}

// =============================================================================
// Game Over Handling
// =============================================================================

static void HandleGameOver(void) {
    game.gameOverTimer += game.deltaTime;

    if (game.gameOverTimer > RESTART_DELAY) {
        VRController right = GetController(CONTROLLER_RIGHT);
        VRController left  = GetController(CONTROLLER_LEFT);
        if (right.buttonA || left.buttonA) {
            LOGI("Restarting game...");
            InitGame();
        }
    }
}

// =============================================================================
// Main Loop - Called Every Frame
// =============================================================================

void inLoop(struct android_app* app) {
    if (!game.initialized) {
        InitGame();
    }

    // Delta time from display refresh rate
    VRHeadset headset = GetHeadset();
    game.deltaTime = (headset.displayRefreshRate > 0)
                   ? 1.0f / headset.displayRefreshRate
                   : 1.0f / 72.0f;
    game.gameTime += game.deltaTime;

    // Deferred capture of player center: wait until headset reports a valid
    // non-zero position (OpenXR tracking may not be ready on the very first frame).
    if (!game.gameCenterValid) {
        float hx = headset.position.x;
        float hy = headset.position.y;
        float hz = headset.position.z;
        // Headset at exactly (0,0,0) usually means tracking isn't ready yet;
        // a real head position always has y > 0 (you're not lying on the floor).
        if (hy > 0.1f || (hx * hx + hz * hz) > 0.01f) {
            game.gameCenter = Vector3Create(hx, 0, hz);
            Vector3 fwd = QuaternionForward(headset.orientation);
            game.gameFacing = atan2f(-fwd.x, fwd.z);
            game.gameCenterValid = true;
            LOGI("Player center captured: (%.2f, %.2f)  facing: %.1f deg",
                 game.gameCenter.x, game.gameCenter.z,
                 game.gameFacing * 180.0f / PI);
        }
    }

    // Hand tracking update (if available)
    if (game.handTrackingEnabled) {
        UpdateHandTracking();
    }

    // -- Game Logic --
    UpdateBlades();

    if (game.phase == STATE_PLAYING) {
        UpdateSpawning();
        CheckCollisions();
        UpdateCombo();
    } else {
        HandleGameOver();
    }

    UpdatePhysics();

    // -- Rendering --
    DrawEnvironment();

    // Game cubes (Rubik's style)
    for (int i = 0; i < MAX_CUBES; i++) {
        SliceCube* c = &game.cubes[i];
        if (!c->active || c->state != CUBE_FLYING) continue;

        float flash = (c->flashTimer > 0) ? c->flashTimer / 0.3f : 0;
        DrawRubikCube(c->position, c->rotationY, c->rotationX, c->color, flash);

        // Flip-count golden orbs orbiting the cube
        for (int f = 0; f < c->flipCount && f < 5; f++) {
            int total = c->flipCount > 0 ? c->flipCount : 1;
            float ang = game.gameTime * 5.0f + (float)f / total * 2.0f * PI;
            float r   = CUBE_TOTAL_SIZE + 0.05f;
            Vector3 orbPos = Vector3Add(c->position,
                Vector3Create(cosf(ang) * r, 0, sinf(ang) * r));
            DrawVRCube(orbPos, 0.012f, GOLD);
        }
    }

    // Fragments
    DrawFragments();

    // Blades (both hands)
    DrawBlade(0, GetController(CONTROLLER_LEFT));
    DrawBlade(1, GetController(CONTROLLER_RIGHT));

    // HUD (score, lives, combo)
    DrawHUD();

    // Game over overlay
    if (game.phase == STATE_GAME_OVER) {
        DrawGameOverScreen();
    }

    // Periodic debug log
    static int frameCount = 0;
    if (++frameCount % 500 == 0) {
        LOGI("Score:%d  Lives:%d  Combo:%d  Sliced:%d  Missed:%d  Time:%.0fs",
             game.score, game.lives, game.currentCombo,
             game.totalSliced, game.totalMissed, game.gameTime);
    }
}

// =============================================================================
// Application Entry Point
// =============================================================================

void android_main(struct android_app* app) {
    LOGI("Cube Slice VR - Starting...");

    if (!InitApp(app)) {
        LOGE("Failed to initialize VR!");
        return;
    }
    LOGI("VR initialized");

    // Hand tracking (optional - graceful fallback to controllers)
    if (InitHandTracking()) {
        game.handTrackingEnabled = true;
        LOGI("Hand tracking enabled");
    } else {
        game.handTrackingEnabled = false;
        LOGI("Hand tracking unavailable - controllers only");
    }

    // Dark space-like background
    SetVRClearColor((Color){8, 8, 20, 255});

    // Main loop
    while (!AppShouldClose(app)) {
        BeginVRMode();
        SyncControllers();
        inLoop(app);
        EndVRMode();
    }

    // Cleanup
    if (game.handTrackingEnabled) {
        ShutdownHandTracking();
    }

    LOGI("Shutting down...");
    CloseApp(app);
    LOGI("Cube Slice VR - Done");
}
