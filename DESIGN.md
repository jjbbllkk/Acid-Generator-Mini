# Acid Generator Mini - Design Requirements

Comprehensive design specification for reproducing the Acid Generator Mini sequencer module. This document covers panel layout, visual design, widget placement, color palette, typography, pattern generation algorithm, and signal behavior.

## Module Identity

- **Name**: Acid Generator Mini
- **Slug**: AcidGenMini
- **Brand**: Vulpes79
- **Type**: TB-303-style generative pattern sequencer
- **Panel Size**: 12HP (60.96mm wide x 128.5mm tall)
- **Format**: VCV Rack 2 module

## Panel Layout Overview

The panel flows top-to-bottom through five distinct visual zones:

```
+---------------------------+  y=0
|        [screws]           |
|   <<  ACID GEN MINI  >>  |  Title + chevrons
|                           |
| [DENSITY] [SPREAD] [LEN] |  Row 1: Main knobs (y=20)
|                           |
| [ACC] [SLD] [ROOT] [SCL] |  Row 2: Secondary knobs (y=38)
|                           |
| +=======================+ |
| | Pattern Display       | |  16-step bar visualization (y=46)
| +=======================+ |
| [C MIN  ] [GEN][LED]     |  Info display + Generate (y=70)
| [-][+] o o o o o  OCT    |  Octave controls (y=82)
|===========================|  <-- yellow divider line (y=90)
| [CLK]  [RST]  [GEN]      |  CV Inputs (y=100, dark bg)
|===========================|
| [V/OCT] [GATE] [ACC][SLD]|  Outputs (y=117, darkest bg)
|  vulpes79                 |  Brand label
|        [screws]           |
+---------------------------+  y=128.5
```

## Color Palette

### Backgrounds (top to bottom gradient: light to dark)

| Element | Color | Notes |
|---------|-------|-------|
| Main panel background | `#e6e6e6` | Light warm gray, full panel |
| Section grouping rects | `#dcdcdc` | Subtle darker gray, `rx=2` rounded corners |
| CV input band | `#222222` | Dark band, y=90 to y=110 |
| Output band | `#1a1a1a` | Darkest band, y=107.22 to y=128.5 |

### Accent Color

| Element | Color | Notes |
|---------|-------|-------|
| Divider line | `#ffff00` | Full-width horizontal line at y=90, stroke-width 0.5 |

Yellow is used extremely sparingly. In this module it appears only as the divider line between the controls section and CV inputs. The vulpes79 design language also uses yellow for hero knob halo rings and VU meter dots on other modules.

### Display Colors

| Element | Color |
|---------|-------|
| Display background | `#0a0a0a` (near-black) |
| Display border | `#333333` stroke, 1px, rounded 3px |
| Active step bars | `#79d8b9` (bright cyan/teal) |
| Inactive step bars | `#509080` base, modulated by octave brightness |
| Accent indicator (current) | `#ff8040` (orange) |
| Accent indicator (inactive) | `#aa5522` (dark orange/brown) |
| Slide indicator (current) | `#4080ff` (blue) |
| Slide indicator (inactive) | `#2255aa` (dark blue) |
| Current step marker | `#ffffff` (white, 2px horizontal line) |
| Info display text (scale/root) | `#79d8b9` (cyan, matches bars) |
| Info display text (current note) | `#ffffff` (white) |

### UI Element Colors

| Element | Color |
|---------|-------|
| Knob outline circles | `#bbbbbb` stroke, 0.3 width (light section) |
| Jack outline circles | `#444444` stroke, 0.3 width (dark sections) |
| Title text | `#1a1a1a` |
| Knob labels (light bg) | `#1a1a1a` |
| Input/Output labels (dark bg) | `#b3b3b3` |
| Octave LED labels | `#888888` |
| Brand text | `#b3b3b3` |
| Chevrons | `#221f1f` |

## Typography

**Font**: JetBrains Mono (exclusively, no other fonts)

All text in the SVG panel is converted to `<path>` elements for portability. An editable text layer (display:none) is maintained in Inkscape for future editing.

| Element | Size | Weight | Color | Position |
|---------|------|--------|-------|----------|
| Module title "ACID GEN MINI" | 3.5px | Bold | `#1a1a1a` | Centered, y~7 |
| Main knob labels (DENSITY, SPREAD, LENGTH) | 2px | Regular | `#1a1a1a` | Centered below knobs, y~13 |
| Small knob labels (ACC, SLD, ROOT, SCALE) | 2px | Regular | `#1a1a1a` | Centered below knobs, y~31 |
| Octave labels (OCT, -, +) | 1.8px | Regular | `#1a1a1a` | Near octave controls |
| Octave LED labels (-2, -1, 0, +1, +2) | 1.5px | Regular | `#888888` | Below octave LEDs, y~87 |
| CV input labels (CLK, RST, GEN) | 2px | Regular | `#b3b3b3` | Above jacks in dark section |
| Output labels (V/OCT, GATE, ACC, SLIDE) | 2px | Regular | `#b3b3b3` | Above jacks in darkest section |
| Brand "vulpes79" | ~2.1px (8px font, scaled) | Regular | `#b3b3b3` | Bottom center, output section, y~127 |

## Section Backgrounds

Three rounded rectangles group the controls on the light portion of the panel:

| Section | Position | Size | Color |
|---------|----------|------|-------|
| Main knobs | (3, 10) | 54.96 x 18mm | `#dcdcdc`, rx=2 |
| Small knobs | (3, 28) | 54.96 x 17mm | `#dcdcdc`, rx=2 |
| Display area | (3, 44) | 54.96 x 44mm | `#dcdcdc`, rx=2 |

## Decorative Elements

### Title Chevrons

Two pairs of mirrored arrow shapes flank the title at y=5-9:

- **Left pair**: Two right-pointing triangles at x=3 and x=6
- **Right pair**: Two left-pointing triangles at x=57.96 and x=54.96
- **Fill**: `#221f1f`
- **Shape**: Polygon, e.g. `points="3,5 7,7 3,9"` (left) and `points="57.96,5 53.96,7 57.96,9"` (right)

### Acid Smiley Face

A signature brand element - a melting acid house smiley face:

- **Position**: Bottom-right of panel, inside the CV input dark band area
- **Transform**: `matrix(0.00333856,0,0,0.00333856,44.699277,91.823424)` (very small)
- **Colors**: Yellow face `#f7e71d`, dark features/outline `#292a2c`
- **Components**: Head circle, two oval eyes, wide smile, melting hair/drip detail
- **Layer**: Separate Inkscape layer labeled "acid smiley"

### Yellow Divider Line

Full-width horizontal accent at y=90, separating control section from CV inputs:
- `stroke="#ffff00"`, `stroke-width="0.5"`
- Spans x=0 to x=60.96

## Widget Placement (all positions in mm, centered)

### Column Layout

| Column | X Position | Used By |
|--------|-----------|---------|
| COL1 | 12mm | DENSITY, ACC, CLK, V/OCT |
| COL2 | 30.48mm | SPREAD (center of module) |
| COL3 | 49mm | LENGTH, SCALE |
| Extra | 24mm | SLD, RST, GATE |
| Extra | 37mm | ROOT |
| Extra | 38mm | GEN input, ACC output |
| Extra | 51mm | SLIDE output |

### Row 1: Main Knobs (y=20mm)

| Control | Position | Widget | Parameter |
|---------|----------|--------|-----------|
| DENSITY | (12, 20) | Rogan1PWhite | 0-100%, default 50% |
| SPREAD | (30.48, 20) | Rogan1PWhite | 0-100%, default 50% |
| LENGTH | (49, 20) | Rogan1PWhite | 1-64 steps, snap, default 16 |

Knob outline circles: r=4.5, `#bbbbbb` stroke, 0.3 width

### Row 2: Secondary Knobs (y=38mm)

| Control | Position | Widget | Parameter |
|---------|----------|--------|-----------|
| ACC (Accent) | (12, 38) | Rogan1PWhite | 0-100%, default 25% |
| SLD (Slide) | (24, 38) | Rogan1PWhite | 0-100%, default 15% |
| ROOT | (37, 38) | Rogan1PWhite | 0-11, snap (C through B) |
| SCALE | (49, 38) | Rogan1PWhite | 0-23, snap (24 scales) |

Knob outline circles: r=4.5, `#bbbbbb` stroke, 0.3 width

### Pattern Display (y=46mm)

- **Position**: (4, 46)
- **Size**: 52.96 x 22mm
- **Widget type**: Custom `PatternDisplay` (OpaqueWidget)
- **Background**: `#0a0a0a` rounded rect (r=3), `#333333` border stroke
- See [Pattern Display Rendering](#pattern-display-rendering) for drawing details

### Info Display (y=70mm)

- **Position**: (4, 70)
- **Size**: 36 x 7mm
- **Widget type**: Custom `InfoDisplay` (OpaqueWidget)
- **Background**: `#0a0a0a` rounded rect (r=2)
- **Left text**: Root + Scale abbreviation (e.g. "C MIN") in cyan `#79d8b9`, 10pt
- **Right text**: Current note playing (e.g. "E4") in white `#ffffff`, 10pt

### Generate Button + LED (y=73.5mm)

| Element | Position | Widget |
|---------|----------|--------|
| Generate button | (46, 73.5) | VCVButton |
| Generate LED | (54, 73.5) | SmallLight\<GreenLight\> |

LED brightness fades from 1.0 with decay: `brightness *= 1 - sampleTime * 4`

### Octave Controls (y=82mm)

| Element | Position | Widget |
|---------|----------|--------|
| Octave Down (-) | (8, 82) | TL1105 |
| Octave Up (+) | (20, 82) | TL1105 |
| Octave LED 0 (-2) | (30, 82) | SmallLight\<GreenLight\> |
| Octave LED 1 (-1) | (35.5, 82) | SmallLight\<GreenLight\> |
| Octave LED 2 (0) | (41, 82) | SmallLight\<GreenLight\> |
| Octave LED 3 (+1) | (46.5, 82) | SmallLight\<GreenLight\> |
| Octave LED 4 (+2) | (52, 82) | SmallLight\<GreenLight\> |

LED spacing: 5.5mm apart, starting at x=30
Active octave: brightness 1.0, inactive: brightness 0.1

### CV Inputs (y=100mm, dark background)

| Port | Position | Widget | Label |
|------|----------|--------|-------|
| CLK (Clock) | (10, 100) | PJ301MPort | CLK |
| RST (Reset) | (24, 100) | PJ301MPort | RST |
| GEN (Generate) | (38, 100) | PJ301MPort | GEN |

Jack outline circles: r=4.3, `#444444` stroke, 0.3 width

### Outputs (y=117mm, darkest background)

| Port | Position | Widget | Label |
|------|----------|--------|-------|
| PITCH | (10, 117) | PJ301MPort | V/OCT |
| GATE | (24, 117) | PJ301MPort | GATE |
| ACCENT | (38, 117) | PJ301MPort | ACC |
| SLIDE | (51, 117) | PJ301MPort | SLIDE |

Jack outline circles: r=4.3, `#444444` stroke, 0.3 width

### Screws

Standard ScrewSilver at four corners:
- Top-left: (RACK_GRID_WIDTH, 0)
- Top-right: (box.size.x - 2 * RACK_GRID_WIDTH, 0)
- Bottom-left: (RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)
- Bottom-right: (box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)

## Pattern Display Rendering

The custom PatternDisplay widget draws a 16-step bar visualization with auto-paging for patterns up to 64 steps.

### Layout

- **Padding**: 3px all sides
- **Bar area width**: widget width - (padding * 2)
- **Bar width**: barAreaWidth / 16 - 1
- **Bar max height**: widget height - (padding * 2) - 16px (room for indicators + page)
- **Indicator Y**: widget height - padding - 12px

### Page Indicator

When pattern length > 16, show "page/total" (e.g. "2/4") at top-right:
- Font: 8pt UI font
- Color: `#606060`
- Alignment: right, top
- Auto-follows the current step position

### Per-Step Drawing

For each of the 16 visible steps:

1. **Background bar** (always drawn):
   - Full height of bar area
   - Outside pattern length: `#151515`
   - Inside pattern length: `#1a1a1a`

2. **Note bar** (active notes only):
   - Height: `(note + 1) / 7.0` of max bar height, clamped to 0.15-1.0
   - Drawn from bottom up
   - Current step color: `#79d8b9` (bright cyan)
   - Inactive step color: base RGB `(0x50, 0x90, 0x80)` scaled by octave brightness
   - Octave brightness: `0.6 + octave * 0.2`, clamped 0.4-1.0

3. **Current step indicator**:
   - White horizontal line (`#ffffff`), 2px tall, below bar area

4. **Accent indicator** (dot):
   - Small filled circle, r=2
   - At indicator Y position, centered on bar
   - Current step: `#ff8040`, inactive: `#aa5522`

5. **Slide indicator** (line/chevron):
   - Small angled stroke below accent position (indicatorY + 4)
   - Three-point path: left(-2), right(+2), extended right(+4, +2)
   - Stroke width: 1.5
   - Current step: `#4080ff`, inactive: `#2255aa`

## Scale Abbreviations (for info display)

| Scale | Abbreviation |
|-------|-------------|
| Major | MAJ |
| Minor | MIN |
| Dorian | DOR |
| Mixolydian | MIX |
| Lydian | LYD |
| Phrygian | PHR |
| Locrian | LOC |
| Harmonic Minor | H-m |
| Harmonic Major | H-M |
| Dorian #4 | D#4 |
| Phrygian Dominant | PhD |
| Melodic Minor | Mm |
| Lydian Augmented | L+ |
| Lydian Dominant | LD |
| Hungarian Minor | HUN |
| Super Locrian | SuL |
| Spanish | SPA |
| Bhairav | BHV |
| Pentatonic Minor | Pm |
| Pentatonic Major | PM |
| Blues Minor | BLU |
| Whole Tone | WHL |
| Chromatic | CHR |
| Japanese In-Sen | INS |

## Parameter Specifications

| Parameter | Type | Min | Default | Max | Unit | Snap |
|-----------|------|-----|---------|-----|------|------|
| DENSITY | Continuous | 0 | 50 | 100 | % | No |
| SPREAD | Continuous | 0 | 50 | 100 | % | No |
| PATTERN LENGTH | Discrete | 1 | 16 | 64 | steps | Yes |
| ACCENT DENSITY | Continuous | 0 | 25 | 100 | % | No |
| SLIDE DENSITY | Continuous | 0 | 15 | 100 | % | No |
| ROOT NOTE | Discrete | 0 (C) | 0 | 11 (B) | semitone | Yes |
| SCALE | Discrete | 0 | 0 | 23 | index | Yes |
| OCTAVE | Discrete | -2 | 0 | +2 | octaves | Yes |
| OCTAVE UP | Momentary button | - | - | - | - | - |
| OCTAVE DOWN | Momentary button | - | - | - | - | - |
| GENERATE | Momentary button | - | - | - | - | - |

## Pattern Generation Algorithm

### Overview

The generator creates a "master pattern" containing all note data. Density and spread are NOT baked in -- they are applied in real-time during playback. This allows the user to sweep density/spread knobs and hear immediate changes without regenerating.

### PRNG

Uses SFC32 (Small Fast Chaotic) 32-bit PRNG, seeded from system time XORed with a linear congruential update of the previous seed. Produces deterministic sequences for a given seed.

### Generation Steps

#### 1. Musical Spread Logic (Scale Priority Order)

Weight each of the 7 scale degrees:
- Degree 0 (root): weight += 999.0 (always highest priority)
- Degree 4 (typically the 5th): weight += 0.5 (often second priority)
- All others: random weight from PRNG

Sort by weight descending to create `scalePriorityOrder[]`. This determines which notes appear first as the SPREAD knob increases from 0% (root only) to 100% (all 7 degrees).

#### 2. Density Mask Order (Bar Activation)

Weight each of the 16 bar positions:
- Every 4th position (downbeats 0, 4, 8, 12): weight += 0.5
- Position 0 (the "One"): additional weight += 0.5
- All positions: random base weight from PRNG

Sort by weight descending to create `barActivationOrder[]`. This determines which beat positions activate first as the DENSITY knob increases from 0% to 100%.

#### 3. Step Content Generation

For each of the 64 steps:
- **Note pool index**: If downbeat (i%4==0) and random > 0.3, use index 0 (root). Otherwise, random 0-6.
- **Octave**: Random from {-1, 0, +1}
- **Accent probability**: Random float 0-1 (compared against accent density at playback)
- **Slide probability**: Random float 0-1 (compared against slide density at playback)

### Real-Time Application

During playback, for each step:

1. **Mute check**: User-toggled mutes force rest regardless of other settings.
2. **Density check**: Is this step's bar position (step % 16) among the first N activated positions, where N = round(16 * density / 100)? If not, the step is a rest.
3. **Spread check**: Is this step's notePoolIndex less than M, where M = max(1, round(7 * spread / 100))? If not, quantize to root (pool index 0).
4. **Accent check**: Is accentProb < (accentDensity / 100)? If so, accent is active.
5. **Slide check**: Is slideProb < (slideDensity / 100)? If so, slide is active.

### Scale Definitions

24 scales, each defined as an array of semitone intervals from root:

| Index | Scale | Intervals | Length |
|-------|-------|-----------|--------|
| 0 | Major | 0,2,4,5,7,9,11 | 7 |
| 1 | Minor | 0,2,3,5,7,8,10 | 7 |
| 2 | Dorian | 0,2,3,5,7,9,10 | 7 |
| 3 | Mixolydian | 0,2,4,5,7,9,10 | 7 |
| 4 | Lydian | 0,2,4,6,7,9,11 | 7 |
| 5 | Phrygian | 0,1,3,5,7,8,10 | 7 |
| 6 | Locrian | 0,1,3,5,6,8,10 | 7 |
| 7 | Harmonic Minor | 0,2,3,5,7,8,11 | 7 |
| 8 | Harmonic Major | 0,2,4,5,7,8,11 | 7 |
| 9 | Dorian #4 | 0,2,3,6,7,9,10 | 7 |
| 10 | Phrygian Dominant | 0,1,4,5,7,8,10 | 7 |
| 11 | Melodic Minor | 0,2,3,5,7,9,11 | 7 |
| 12 | Lydian Augmented | 0,2,4,6,8,9,11 | 7 |
| 13 | Lydian Dominant | 0,2,4,6,7,9,10 | 7 |
| 14 | Hungarian Minor | 0,2,3,6,7,8,11 | 7 |
| 15 | Super Locrian | 0,1,3,4,6,8,10 | 7 |
| 16 | Spanish | 0,1,4,5,7,9,10 | 7 |
| 17 | Bhairav | 0,1,4,5,7,8,11 | 7 |
| 18 | Pentatonic Minor | 0,3,5,7,10 | 5 |
| 19 | Pentatonic Major | 0,2,4,7,9 | 5 |
| 20 | Blues Minor | 0,3,5,6,7,10 | 6 |
| 21 | Whole Tone | 0,2,4,6,8,10 | 6 |
| 22 | Chromatic | 0,1,2,3,4,5,6,7,8,9,10,11 | 12 |
| 23 | Japanese In-Sen | 0,1,5,7,10 | 5 |

### Note-to-Pitch Conversion

```
midiNote = scaleIntervals[noteDegree % scaleLength] + rootNote + 12 * (octave + noteDegree / scaleLength)
pitchVoltage = midiNote / 12.0  (1V/oct, 0V = C0)
```

## Output Voltage Specifications

| Output | Voltage | Behavior |
|--------|---------|----------|
| PITCH (V/OCT) | 1V/oct | 0V = C0. Continuous during slide (portamento). |
| GATE | 0V / 10V | Short pulse (20ms) for normal notes. Extended (~110% of clock period) for slide notes. 1ms retrigger gap when notes don't slide. |
| ACCENT | 0V / 10V | Pulse matching gate length on accented notes. |
| SLIDE | 0V / 10V | High when current step has slide flag active. |

### Slide/Portamento Behavior

- When a note has slide enabled, the gate extends to tie into the next step
- The next note's pitch glides over ~50ms (303-style portamento)
- During slide, no gate retrigger occurs (legato)
- When gate IS retriggered (no slide), a 1ms gap forces the gate low to ensure envelope retrigger

### Clock Period Measurement

The module measures the time between clock rising edges to determine tempo:
- Used to calculate slide gate extension (110% of clock period)
- Bounded to 10ms-2s range for sanity
- Default: 125ms (~120 BPM 16th notes)

## Input Behavior

| Input | Behavior |
|-------|----------|
| CLK | Rising edge advances to next step. Schmitt trigger detection. |
| RST | Rising edge resets step to -1 (next clock goes to step 0). Clears slide state. |
| GEN | Rising edge generates a new pattern (same as pressing Generate button). |

## Context Menu

Right-click menu provides a "Scale" submenu with all 24 scales as checkable items, allowing scale selection without using the knob.

## State Serialization (JSON)

Saved state includes:
- **version**: Schema version (currently 3)
- **seed**: PRNG seed for pattern regeneration
- **currentStep**: Playback position
- **masterPattern**: Full backup of barActivationOrder, scalePriorityOrder, per-step data (notePoolIndex, octave, accentProb, slideProb, muted)
- **Slide state**: currentSlideActive, currentPitch, slideTargetPitch, slideRate

On load: restores master pattern from JSON (v2+) or regenerates from seed (v1 fallback).

## Design Principles (Vulpes79 Design Language)

1. **Monochrome base with yellow accents**: Panel is grayscale `#e6e6e6` to `#1a1a1a`. Yellow `#ffff00` used only for divider lines and hero knob halos (on other modules).

2. **Light-to-dark vertical gradient**: Controls section is light, CV/output sections progressively darker. Creates natural visual hierarchy and functional separation.

3. **Section grouping**: Subtle rounded rects (`#dcdcdc`, rx=2) group related controls without harsh borders.

4. **Typography hierarchy through size only**: JetBrains Mono everywhere. Size and weight vary for hierarchy; font never changes.

5. **Consistent column alignment**: Widget positions snap to a repeating column grid.

6. **Knob hierarchy through size**: Larger Rogan knobs for primary parameters, smaller for secondary. All white variant (Rogan1PWhite).

7. **Personality through subtle graphics**: Acid smiley, chevrons add character while remaining small and unobtrusive.

8. **Display accent color**: `#79d8b9` cyan/teal used consistently across pattern bars, info text, and active indicators. This is the module's signature display color.

## SVG Layer Structure

- **Root level**: Background rects, section rects, decorative elements, text paths
- **layer1-0** ("acid smiley"): Smiley face paths, heavily scaled down via matrix transform
- **Text**: All converted to `<path>` elements with `aria-label` for accessibility. Source text preserved in hidden Inkscape layer for editing.

## File Structure

```
src/
  plugin.cpp          Plugin initialization
  plugin.hpp          Header declarations
  AcidSeq.cpp         Module + widgets (PatternDisplay, InfoDisplay, AcidSeqWidget)
  Generator.hpp       Pattern generation engine, PRNG, scale data, voltage helpers
res/
  AcidGenMini.svg     Panel SVG (60.96mm x 128.5mm)
```
