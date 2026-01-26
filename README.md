# Acid Generator Mini for VCV Rack

A compact and powerful acid-style sequence generator module for VCV Rack, offering dynamic pattern creation with intuitive controls. Designed to bring classic acid basslines and evolving melodies to your modular setup.

## Features

*   **DENSITY:** Controls the probability of notes or events being triggered in the sequence.
*   **SPREAD:** Adjusts the distribution and variation of generated parameters, influencing musical diversity.
*   **LENGTH:** Sets the active length of the sequence or the duration of generated notes.
*   **ACC (Accent):** Controls the dynamics of accented notes, adding punch and emphasis.
*   **SLD (Slide):** Manages portamento or glide between notes for fluid transitions.
*   **ROOT:** Selects the fundamental root note of the generated musical patterns.
*   **SCALE:** Chooses the musical scale for quantizing generated pitches, ensuring harmonic consistency.
*   **OCT (Octave):** Transposes the entire sequence up or down by octaves.

### Inputs

*   **CLK (Clock):** External clock input for synchronization.
*   **RST (Reset):** Resets the sequence to its starting position.
*   **GEN (Generate):** Triggers the generation or regeneration of a new musical pattern.

### Outputs

*   **V/OCT (Voltage per Octave):** Standard pitch control voltage output.
*   **GATE:** Gate signal output for triggering envelopes and other modules.
*   **ACC (Accent):** Trigger output for accented notes.
*   **SLIDE:** Control voltage or trigger output for slide/portamento events.

## Installation

### VCV Library (Recommended)

The easiest way to install Acid Generator Mini is directly through the VCV Rack application's plugin manager. Search for "Acid Generator Mini" and click "Install".

### Manual Installation

1.  Locate your VCV Rack user folder:
    *   **macOS:** `~/Documents/Rack2/`
    *   **Windows:** `My Documents\Rack2\`
    *   **Linux:** `~/.Rack2/`
2.  Navigate into the `plugins/` subdirectory within your VCV Rack user folder.
3.  Copy the `AcidGeneratorMini-<version>-<platform>.vcvplugin` file (e.g., `AcidGeneratorMini-2.0.0-mac-arm64.vcvplugin`) into this directory.
4.  Restart VCV Rack.

## Building from Source

If you prefer to build the plugin yourself, follow these steps:

### Prerequisites

*   VCV Rack SDK (available from the [VCV Rack website](https://vcvrack.com/downloads)). Ensure the `RACK_DIR` environment variable is set to your Rack SDK directory, or adjust the `Makefile` accordingly.
*   Standard build tools (g++, clang, make).

### Steps

1.  **Clone the Repository:**
    ```bash
    git clone https://github.com/Vulpes79/Acid-Generator-Mini.git
    cd Acid-Generator-Mini
    ```
2.  **Build and Install:**
    ```bash
    # Clean previous builds
    make clean
    # Build and install to your Rack user directory (adjust RACK_USER_DIR if needed)
    RACK_USER_DIR="/Users/foxparty/Library/Application Support/Rack2" make install
    ```
    Replace `/Users/foxparty/Library/Application Support/Rack2` with your actual VCV Rack user directory if it's different.

## Usage

Once installed, launch VCV Rack, right-click on an empty space in your patch, and select "Acid Generator Mini" from the module browser under the "Vulpes79" brand. Connect the inputs and outputs to other modules in your patch to start creating acid sequences!

## Credits

Developed by Vulpes79.
