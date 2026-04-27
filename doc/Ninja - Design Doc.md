# VR Hand Tracking Ninja Combat - Design Document

## 1. Game Overview

### 1.1 Core Concept
A VR hand tracking demo game where players perform ninja hand seals (mudras) to cast jutsu and defeat a single boss enemy. The game showcases hand tracking capabilities and cannot be played with controllers.

### 1.2 Core Loop
1. Boss telegraphs attack
2. Player performs activation gesture → enters casting window
3. Player performs specific jutsu seal within time window
4. Cast jutsu to attack/defend
5. Repeat until boss is defeated

### 1.3 Demo Goals
- Demonstrate hand tracking accuracy
- Showcase unique VR interaction
- Completable in 5-10 minutes
- Teach players 4 distinct hand seals

---

## 2. Hand Seal System

### 2.0 Two-Step Casting System

**All jutsu follow this sequence:**

#### Step 1: Activation Gesture
- **Gesture**: Right hand with index and middle fingers extended vertically, other fingers curled
- **Duration Required**: Hold for 0.4s
- **Effect**: Opens 2.5-second casting window
- **Feedback**: 
  - Haptic pulse in right hand (if supported)
  - Right hand glows bright white
  - Particle burst effect from right hand
  - Audio: Sharp "activation" chime
  - UI: Circular timer appears (2.5s countdown)
  - Text prompt: "Casting Window Active!"

#### Step 2: Jutsu Seal (within 2.5s window)
- Player can now move/lower right hand freely
- Perform specific jutsu gesture with either hand
- Recognition triggers jutsu cast immediately
- If window expires without jutsu: plays error sound, must reactivate

**Example Flow:**
```
Player raises right hand (index + middle fingers up)
  → Hold for 0.4s
  → ACTIVATION: Haptic pulse + white glow + particle burst
  → Casting window opens (2.5s timer starts)
  → Player lowers right hand
  → Player forms Tiger Seal with left hand
  → Fire Jutsu launches immediately
```

### 2.1 Four Core Jutsu

#### 1. Tiger Seal - Fire Jutsu
- **Gesture**: Left hand - thumb pointing up, other four fingers curled into fist
- **Effect**: Shoot fireball projectile
- **Chakra Cost**: 20
- **Cooldown**: 2s
- **Damage**: 25
- **Visual Cue**: Left hand glows orange/red when recognized

#### 2. Dragon Seal - Lightning Jutsu  
- **Gesture**: Right hand - index finger and thumb form circle (OK sign), other fingers extended
- **Effect**: Lightning strike from above
- **Chakra Cost**: 30
- **Cooldown**: 3s
- **Damage**: 40
- **Visual Cue**: Right hand glows blue/white when recognized

#### 3. Horse Seal - Shield
- **Gesture**: Both hands - palms facing forward, all fingers spread wide
- **Effect**: Protective barrier (absorbs 50 damage)
- **Chakra Cost**: 25
- **Cooldown**: 5s
- **Duration**: 3s
- **Visual Cue**: Both hands glow cyan when recognized

#### 4. Boar Seal - Heal
- **Gesture**: Both hands - pressed together in prayer position, fingers interlaced
- **Effect**: Restore health
- **Chakra Cost**: 35
- **Cooldown**: 8s
- **Healing**: 30 HP
- **Visual Cue**: Both hands glow green when recognized

### 2.2 Hand Tracking Requirements

**Must Detect:**
- Individual finger curl/extension
- Thumb position and orientation
- Hand rotation/orientation
- Sequential gesture recognition (activation → jutsu)

**Recognition Tolerance:**
- Activation gesture: ±15° angle deviation, must hold 0.4s
- Jutsu gestures: ±15° angle deviation, instant recognition (no hold required)
- Casting window: 2.5 seconds from activation

**Casting Window States:**
1. **Idle**: No active window
2. **Activation Charging**: Right hand two-finger gesture detected, charging (0-0.4s)
3. **Window Active**: 2.5s timer running, waiting for jutsu seal
4. **Jutsu Recognized**: Specific seal detected, casting
5. **Window Expired**: 2.5s elapsed without jutsu, returns to Idle
6. **Cooldown**: Jutsu on cooldown, activation gesture ignored

---

## 3. Combat System

### 3.1 Player Stats
- **HP**: 100 (starts at full)
- **Chakra**: 100 (regenerates 10/second)

### 3.2 Boss Design - Shadow Ninja

**Phase 1 (HP: 100-60)**
- **Shuriken Throw**: Throws 3 projectiles in sequence
  - Telegraph: Raises arm, 1.5s warning
  - Counter: Use Shield or dodge
  
- **Dash Strike**: Rushes toward player
  - Telegraph: Crouches, 1.2s warning  
  - Counter: Use Lightning Jutsu to interrupt

**Phase 2 (HP: 60-0)**
- Same attacks but faster
- **New: Shadow Clone**: Spawns 2 clones
  - Telegraph: Hand seal animation, 2s warning
  - Counter: Use Fire Jutsu on all three (only one is real, others disappear after 1 hit)

**Total HP**: 100

---

## 4. Game Flow

### 4.1 Tutorial Phase (2-3 minutes)

**Scene**: Training Dojo

1. **Introduction** (15s)
   - Text: "Welcome, ninja apprentice. Master the ancient hand seals."
   - Training dummy appears

2. **Activation Gesture** (45s)
   - Text: "All jutsu begin with the Activation Seal"
   - Ghosted right hand shows two fingers up
   - Player must hold correctly for 0.4s
   - **Trigger effects**: Haptic pulse + white glow + particles + timer UI
   - Text: "Good! You have 2.5 seconds to cast a jutsu."
   - Window expires (show expiration effect)
   - Text: "Let's learn the jutsu seals. Activate again."

3. **Seal 1 - Tiger Seal** (60s)
   - Text: "First, activate the casting window"
   - Player activates (two fingers → haptic + glow)
   - Text: "Now form the Tiger Seal with your left hand"
   - Show ghosted left hand with Tiger seal
   - Player forms seal → Fire Jutsu casts automatically
   - Training dummy is hit
   - Repeat 2x for practice
   - Text: "Notice: you can lower your right hand after activation"

4. **Seal 2 - Horse Seal** (60s)
   - Text: "Activate, then form Horse Seal for defense"
   - Show combined sequence
   - Player activates → forms shield with both hands
   - Training dummy attacks → shield blocks
   - Repeat 1x

5. **Seal 3 - Dragon Seal** (60s)
   - Text: "Activate, then Dragon Seal for lightning"
   - Player activates → forms OK sign with right hand
   - Lightning strikes dummy
   - Repeat 1x

6. **Seal 4 - Boar Seal** (45s)
   - Text: "Activate, then Boar Seal to heal"
   - Player activates → presses hands together
   - Heal effect plays
   - Practice 1x

7. **Speed Test** (30s)
   - Text: "Quick! Cast any 3 jutsu in 15 seconds"
   - Timer challenge to practice flow
   - Must successfully activate → cast 3 times

### 4.2 Minion Combat Section

1. Minion Design - Shuriken Throwers

**Purpose**: 
- Bridge tutorial and boss fight
- Let players practice casting under light pressure
- Introduce combat pacing and chakra management

**Visual Design**:
- Smaller than boss (70% scale)
- Generic ninja silhouette (black with white mask)
- Less detailed than boss (2-3 materials max)
- Spawn with smoke poof effect

2. Minion Behavior

#### Basic Stats
- **HP**: 30 (dies in 1-2 jutsu hits)
- **Spawn Pattern**: Waves of 2-4 minions
- **Movement**: Stationary at spawn points around arena perimeter
- **Attack**: Shuriken throw only

### 4.3 Boss Fight (3-8 minutes)

**Scene**: Moonlit Rooftop

1. Boss enters with dramatic animation
2. Combat begins
3. Phase transition at 60 HP (visual effect + dialogue)
4. Boss defeated → Victory screen

### 4.4 Victory Condition
- Boss HP reaches 0

### 4.5 Defeat Condition  
- Player HP reaches 0
- Option to retry from boss fight start

---

## 5. UI/UX Design

### 5.1 HUD Elements

**Top Left:**
- Player HP bar (green)
- Chakra bar (blue)

**Center:**
- **Activation Status Indicator**:
  - Small right hand icon in corner
  - Glows white when charging activation (0-0.4s progress bar)
  - Disappears when window opens
  
- **Casting Window Timer** (only visible when active):
  - Large circular progress ring (2.5s countdown)
  - Starts full, depletes clockwise
  - Glows white, pulses
  - Number in center: "2.4... 2.3... 2.2..."
  - Turns orange when <1s remaining
  - Turns red when <0.5s remaining

**Right Side:**
- 4 jutsu icons with cooldown timers
- Dimmed when on cooldown or insufficient chakra
- Highlighted when casting window is active

**Bottom Center:**
- Current hand gesture recognition indicator
  - Shows hand outline(s) needed for selected jutsu
  - Green when correct, red when incorrect
  - Only visible during active casting window

**Boss Health:**
- Red bar at top center
- Phase indicator

### 5.2 Tutorial Visuals

- Semi-transparent 3D hand model(s) float in front of player
  - Shows correct gesture for current instruction
- Can rotate view 360° around the model
- Step-by-step breakdown:
  - First shows activation gesture alone
  - Then shows jutsu gesture alone
  - Then shows full sequence with timing
- **Success effects**:
  - "Perfect!" text when gesture recognized
  - Confetti particles
  - Positive audio cue

### 5.3 Visual Feedback

**Activation Gesture Charging (0-0.4s):**
- Right hand outline appears faintly
- Fills with white light as held
- Progress bar below hand

**Activation Successful:**
- **Haptic**: Strong pulse (0.2s duration)
- **Visual**: Bright white flash + particle burst from right hand
- **Audio**: Sharp ascending chime
- **UI**: Casting window timer appears with whoosh animation
- **Text**: "Window Active!" (fades after 0.5s)
- Right hand continues glowing softly white for 0.5s, then fades

**Jutsu Seal Recognized (within window):**
- Casting hand(s) glow with element color
- Element particles swirl around hand(s)
- Audio: Element-specific sound
- Jutsu effect triggers immediately
- Window closes

**Window Expired (no jutsu cast):**
- Timer ring shatters with breaking glass effect
- Haptic: Short double-pulse (error pattern)
- Audio: Descending error tone
- Text: "Window Expired"
- All effects fade

**Jutsu Cast Effects:**
- **Fire**: Flame projectile with trail launches from left hand
- **Lightning**: Bolt descends from sky at targeting point
- **Shield**: Blue hexagonal barrier materializes around player
- **Heal**: Green aura pulses from pressed hands, spreads to body

**Boss Telegraph:**
- Boss glows red before attacking
- Timer indicator above boss head

---

## 6. Technical Specifications

### 6.1 Hand Tracking Requirements

**Minimum:**
- Track 21 keypoints per hand
- 60 FPS tracking update rate
- Finger curl detection (0-1 value per finger)
- Independent hand recognition

**Ideal:**
- 90 FPS tracking
- Finger contact detection (for Boar seal)
- Confidence score per hand
- Per-finger curl precision

### 6.2 Performance Targets

- Frame Rate: 90 FPS
- Activation Gesture Recognition Latency: <100ms  
- Jutsu Seal Recognition Latency: <150ms
- Haptic Feedback Latency: <50ms from recognition

### 6.3 Casting Window Timing
```
Activation Gesture:
- Detection threshold: 0.4s hold
- Tolerance: ±0.1s (0.3-0.5s acceptable)

Casting Window:
- Duration: 2.5s
- Warning threshold: 1.0s remaining (UI turns orange)
- Critical threshold: 0.5s remaining (UI turns red + audio beep)

Jutsu Recognition (within window):
- Instant cast (no hold required)
- Recognition threshold: 0.2s hold for gesture stability
```

### 6.4 Scene Complexity

- Single arena environment
- Max 4 active particle systems
- Boss + max 2 clones (3 characters total)

---

## 7. Level Design

### 7.1 Training Dojo
- Indoor Japanese dojo
- Wooden floor, paper walls
- Minimal distractions
- Good lighting for hand visibility
- Tatami mats on floor
- Paper lanterns providing ambient light

### 7.2 Rooftop Arena
- Circular platform (8m diameter)
- Night scene with full moon
- Cherry blossom petals floating
- City skyline in background
- Clear boundaries to prevent player from falling off
- Wooden railings at edges

---

## 8. Audio Design

### 8.1 Sound Effects

**Activation Gesture:**
- Charging (0-0.4s): Subtle ascending hum (pitch rises)
- Success: Sharp "CHING!" (like unsheathing sword)
- Haptic sync: Pulse matches audio peak

**Casting Window:**
- Opens: Whoosh + wind chime
- Running: Subtle ticking (quiet, not annoying)
- <1s warning: Single beep
- <0.5s critical: Faster beeping
- Expires: Descending "wah wah" + glass shatter

**Jutsu Recognition:**
- Tiger/Fire: Ignition "fwoosh"
- Dragon/Lightning: Electric crackle "bzzt"
- Horse/Shield: Crystal resonance "shiing"
- Boar/Heal: Soft bell "ding" with reverb

**Jutsu Cast:**
- Fire: Fireball launch whoosh + crackling
- Lightning: Thunder crack boom
- Shield: Sustained crystal hum (3s)
- Heal: Gentle pulse waves

**Boss:**
- Footsteps when dashing
- Shuriken throw: Cutting air whoosh
- Takes damage: Grunt
- Clone spawn: Smoke poof

### 8.2 Music
- Tutorial: Calm Japanese flute with koto
- Boss Fight: Taiko drums, building intensity in Phase 2
- Victory: Triumphant shakuhachi melody

---

## 9. Art Style

### 9.1 Visual Direction
- Stylized low-poly aesthetic  
- High contrast colors for readability
- Toon shading for jutsu effects
- Clean, simple geometry

### 9.2 Color Palette
- **Activation**: White/Silver (bright, attention-grabbing)
- **Fire Jutsu**: Orange/Red gradient
- **Lightning Jutsu**: Blue/White electric
- **Shield**: Cyan/Teal
- **Heal**: Emerald Green
- **Boss**: Dark purple/black with red accents
- **UI Timer**: White → Orange (1s) → Red (0.5s)

### 9.3 Player Hands
- Semi-transparent ghosted hands
- During activation charge: Right hand fills with white light
- During window: Hands normal visibility
- During jutsu recognition: Glows with element color
- Particle trails follow hand movements

### 9.4 Casting Window Visual

**Window Active State:**
- Circular timer ring floats at eye level, slightly below center
- 30cm diameter, 2cm thick ring
- Glows white with inner pulse animation
- Countdown numbers in center (large, readable)
- Ring depletes clockwise (starts at 12 o'clock)
- Color shifts: White → Orange (1s) → Red (0.5s)

**Window Expiration:**
- Ring shatters into fragments
- Fragments fade and fall
- Brief red flash

