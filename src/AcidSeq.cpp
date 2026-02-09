#include "plugin.hpp"
#include "Generator.hpp"
#include <ctime>

using namespace AcidGenerator;

struct AcidSeq : Module {
    enum ParamIds {
        PARAM_PATTERN_LENGTH,
        PARAM_DENSITY,
        PARAM_SPREAD,
        PARAM_ACCENT_DENSITY,
        PARAM_SLIDE_DENSITY,
        PARAM_GENERATE,
        PARAM_SCALE,
        PARAM_ROOT_NOTE,
        PARAM_OCTAVE,
        PARAM_OCTAVE_UP,
        PARAM_OCTAVE_DOWN,
        PARAMS_LEN
    };

    enum InputIds {
        INPUT_CLOCK,
        INPUT_RESET,
        INPUT_GENERATE,
        INPUTS_LEN
    };

    enum OutputIds {
        OUTPUT_PITCH,
        OUTPUT_GATE,
        OUTPUT_ACCENT,
        OUTPUT_SLIDE,
        OUTPUTS_LEN
    };

    enum LightIds {
        LIGHT_GENERATE,
        ENUMS(LIGHT_STEP, 16),  // Step indicator lights
        ENUMS(LIGHT_OCTAVE, 5), // Octave indicator lights (-2 to +2)
        LIGHTS_LEN
    };

    // Schmitt triggers for edge detection
    dsp::SchmittTrigger clockTrigger;
    dsp::SchmittTrigger resetTrigger;
    dsp::SchmittTrigger generateTrigger;
    dsp::SchmittTrigger generateButtonTrigger;
    dsp::SchmittTrigger octaveUpTrigger;
    dsp::SchmittTrigger octaveDownTrigger;

    // Gate pulse generator (for timed gate output)
    dsp::PulseGenerator gatePulse;
    dsp::PulseGenerator accentPulse;

    // Master pattern data (density/spread applied in real-time)
    MasterPattern masterPattern;

    // Cached pattern for display (recomputed when params change or edits occur)
    Pattern displayPattern;
    float cachedDensity = -1.f;
    float cachedSpread = -1.f;
    float cachedAccentDensity = -1.f;
    float cachedSlideDensity = -1.f;
    bool forceDisplayRefresh = false;  // Set by UI edits to trigger refresh

    int currentStep = -1;  // -1 means not started yet

    // Slide state
    bool currentSlideActive = false;  // Is current step sliding INTO next?
    float slideTargetPitch = 0.f;
    float currentPitch = 0.f;
    float slideRate = 0.f;

    // Clock period measurement (for tempo-aware slide gates)
    float timeSinceLastClock = 0.f;
    float measuredClockPeriod = 0.125f;  // Default ~120 BPM 16ths

    // Retrigger gap (forces gate low briefly when retriggering mid-slide)
    float retriggerGapRemaining = 0.f;
    static constexpr float RETRIGGER_GAP_TIME = 0.001f;  // 1ms gap

    // Light fade
    float generateLightBrightness = 0.f;

    // Seed for random generation
    uint32_t currentSeed = 12345;

    // Cached values for display access (updated each process cycle)
    int cachedPatternLength = 16;
    Scale cachedScale = Scale::MINOR;
    int cachedRootNote = 0;

    AcidSeq() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        // Pattern generation parameters
        configParam(PARAM_PATTERN_LENGTH, 1.f, 64.f, 16.f, "Pattern Length", " steps");
        paramQuantities[PARAM_PATTERN_LENGTH]->snapEnabled = true;

        configParam(PARAM_DENSITY, 0.f, 100.f, 50.f, "Density", "%");
        configParam(PARAM_SPREAD, 0.f, 100.f, 50.f, "Spread", "%");
        configParam(PARAM_ACCENT_DENSITY, 0.f, 100.f, 25.f, "Accent Density", "%");
        configParam(PARAM_SLIDE_DENSITY, 0.f, 100.f, 15.f, "Slide Density", "%");

        // Scale selection
        configParam(PARAM_SCALE, 0.f, (float)(Scale::NUM_SCALES) - 1.f, 0.f, "Scale");
        paramQuantities[PARAM_SCALE]->snapEnabled = true;

        // Root note (0-11 = C to B)
        configParam(PARAM_ROOT_NOTE, 0.f, 11.f, 0.f, "Root Note");
        paramQuantities[PARAM_ROOT_NOTE]->snapEnabled = true;

        // Base octave offset
        configParam(PARAM_OCTAVE, -2.f, 2.f, 0.f, "Octave");
        paramQuantities[PARAM_OCTAVE]->snapEnabled = true;

        // Octave buttons
        configButton(PARAM_OCTAVE_UP, "Octave Up");
        configButton(PARAM_OCTAVE_DOWN, "Octave Down");

        // Generate button
        configButton(PARAM_GENERATE, "Generate Pattern");

        // Inputs
        configInput(INPUT_CLOCK, "Clock");
        configInput(INPUT_RESET, "Reset");
        configInput(INPUT_GENERATE, "Generate Trigger");

        // Outputs
        configOutput(OUTPUT_PITCH, "Pitch (1V/oct)");
        configOutput(OUTPUT_GATE, "Gate");
        configOutput(OUTPUT_ACCENT, "Accent");
        configOutput(OUTPUT_SLIDE, "Slide");

        // Generate initial pattern
        generateNewPattern();
    }

    void generateNewPattern() {
        // Create new seed from system time
        currentSeed = static_cast<uint32_t>(std::time(nullptr)) ^
                      static_cast<uint32_t>(currentSeed * 1664525 + 1013904223);

        // Generate master pattern (density/spread will be applied in real-time)
        generateMaster(currentSeed, masterPattern);

        // Clear any user mutes from previous pattern
        masterPattern.clearMutes();

        // Force display pattern update
        cachedDensity = -1.f;

        // Visual feedback
        generateLightBrightness = 1.f;
    }

    // Update the display pattern from master pattern + current params
    void updateDisplayPattern() {
        float density = params[PARAM_DENSITY].getValue();
        float spread = params[PARAM_SPREAD].getValue();
        float accentDensity = params[PARAM_ACCENT_DENSITY].getValue();
        float slideDensity = params[PARAM_SLIDE_DENSITY].getValue();

        // Only update if params changed or UI edit forced refresh
        if (!forceDisplayRefresh &&
            density == cachedDensity && spread == cachedSpread &&
            accentDensity == cachedAccentDensity && slideDensity == cachedSlideDensity) {
            return;
        }

        forceDisplayRefresh = false;
        cachedDensity = density;
        cachedSpread = spread;
        cachedAccentDensity = accentDensity;
        cachedSlideDensity = slideDensity;

        // Recompute display pattern
        for (int i = 0; i < MAX_STEPS; i++) {
            displayPattern.steps[i] = masterPattern.getStep(i, density, spread, accentDensity, slideDensity);
        }
    }

    void process(const ProcessArgs& args) override {
        int patternLength = static_cast<int>(params[PARAM_PATTERN_LENGTH].getValue());
        Scale scale = static_cast<Scale>(static_cast<int>(params[PARAM_SCALE].getValue()));
        int rootNote = static_cast<int>(params[PARAM_ROOT_NOTE].getValue());
        int octaveOffset = static_cast<int>(params[PARAM_OCTAVE].getValue());

        // Real-time params for density/spread (applied every clock)
        float density = params[PARAM_DENSITY].getValue();
        float spread = params[PARAM_SPREAD].getValue();
        float accentDensity = params[PARAM_ACCENT_DENSITY].getValue();
        float slideDensity = params[PARAM_SLIDE_DENSITY].getValue();

        // Update cached values for display widget access
        cachedPatternLength = patternLength;
        cachedScale = scale;
        cachedRootNote = rootNote;

        // Update display pattern (checks internally if params changed)
        updateDisplayPattern();

        // --- Handle Generate Trigger ---
        bool generateTriggered = false;

        if (generateButtonTrigger.process(params[PARAM_GENERATE].getValue() > 0.f)) {
            generateTriggered = true;
        }
        if (generateTrigger.process(inputs[INPUT_GENERATE].getVoltage())) {
            generateTriggered = true;
        }

        if (generateTriggered) {
            generateNewPattern();
        }

        // --- Handle Octave Buttons ---
        if (octaveUpTrigger.process(params[PARAM_OCTAVE_UP].getValue() > 0.f)) {
            float currentOctave = params[PARAM_OCTAVE].getValue();
            if (currentOctave < 2.f) {
                params[PARAM_OCTAVE].setValue(currentOctave + 1.f);
            }
        }
        if (octaveDownTrigger.process(params[PARAM_OCTAVE_DOWN].getValue() > 0.f)) {
            float currentOctave = params[PARAM_OCTAVE].getValue();
            if (currentOctave > -2.f) {
                params[PARAM_OCTAVE].setValue(currentOctave - 1.f);
            }
        }

        // --- Handle Reset Trigger ---
        if (resetTrigger.process(inputs[INPUT_RESET].getVoltage())) {
            currentStep = -1;  // Will become 0 on next clock
            currentSlideActive = false;
            retriggerGapRemaining = 0.f;
        }

        // --- Accumulate time for clock period measurement ---
        timeSinceLastClock += args.sampleTime;

        // --- Handle Clock ---
        bool clockRising = clockTrigger.process(inputs[INPUT_CLOCK].getVoltage());

        if (clockRising) {
            // Measure clock period (with sanity bounds)
            if (timeSinceLastClock > 0.01f && timeSinceLastClock < 2.0f) {
                measuredClockPeriod = timeSinceLastClock;
            }
            timeSinceLastClock = 0.f;
            // Advance step
            currentStep++;
            if (currentStep >= patternLength) {
                currentStep = 0;
            }

            // Get current step data with real-time density/spread applied
            SequenceStep step = masterPattern.getStep(currentStep, density, spread, accentDensity, slideDensity);

            if (!step.isRest()) {
                // Calculate pitch voltage
                // getNoteInScale returns semitone offset from root
                // VCV standard: 0V = C4, 1V/octave
                int midiNote = getNoteInScale(step.note, scale, rootNote, step.octave + octaveOffset);
                float pitchVoltage = (midiNote) / 12.0f;  // 1V/oct, 0V = C0

                // Check if previous step had slide active (slide INTO this note)
                int prevStep = (currentStep - 1 + patternLength) % patternLength;
                SequenceStep prevStepData = masterPattern.getStep(prevStep, density, spread, accentDensity, slideDensity);
                bool slideFromPrev = !prevStepData.isRest() && prevStepData.slide;

                if (slideFromPrev) {
                    // Sliding into this note - set up portamento, no retrigger
                    slideTargetPitch = pitchVoltage;
                    // Slide over ~50ms (typical 303 glide time)
                    slideRate = (slideTargetPitch - currentPitch) / (0.05f * args.sampleRate);

                    // If this step also has slide, extend gate to tie into next step
                    if (step.slide) {
                        gatePulse.trigger(measuredClockPeriod * 1.1f);
                    }
                    // Otherwise let the previous gate naturally decay
                } else {
                    // Normal attack - set pitch immediately and retrigger gate
                    currentPitch = pitchVoltage;
                    slideTargetPitch = pitchVoltage;
                    slideRate = 0.f;

                    // If gate is currently high, force a brief gap for retrigger
                    if (gatePulse.remaining > 0.f) {
                        retriggerGapRemaining = RETRIGGER_GAP_TIME;
                    }

                    // Gate time: slides extend to next step, normal notes are short
                    float gateTime = step.slide ? (measuredClockPeriod * 1.1f) : 0.02f;
                    gatePulse.trigger(gateTime);

                    // Trigger accent pulse if accented
                    if (step.accent) {
                        accentPulse.trigger(gateTime);
                    }
                }

                // Store slide state for next step
                currentSlideActive = step.slide;

            } else {
                // Rest - no gate, reset slide
                currentSlideActive = false;
            }
        }

        // --- Process slide (portamento) ---
        if (slideRate != 0.f) {
            currentPitch += slideRate;
            // Check if we've reached target
            if ((slideRate > 0.f && currentPitch >= slideTargetPitch) ||
                (slideRate < 0.f && currentPitch <= slideTargetPitch)) {
                currentPitch = slideTargetPitch;
                slideRate = 0.f;
            }
        }

        // --- Set Outputs ---
        outputs[OUTPUT_PITCH].setVoltage(currentPitch);

        // Gate output (high while pulse is active, but forced low during retrigger gap)
        bool gateHigh = gatePulse.process(args.sampleTime);
        if (retriggerGapRemaining > 0.f) {
            retriggerGapRemaining -= args.sampleTime;
            outputs[OUTPUT_GATE].setVoltage(0.f);  // Force low for retrigger
        } else {
            outputs[OUTPUT_GATE].setVoltage(gateHigh ? 10.f : 0.f);
        }

        // Accent output
        bool accentHigh = accentPulse.process(args.sampleTime);
        outputs[OUTPUT_ACCENT].setVoltage(accentHigh ? 10.f : 0.f);

        // Slide output (indicates current step has slide, useful for external portamento)
        if (currentStep >= 0 && currentStep < patternLength) {
            SequenceStep currentStepData = masterPattern.getStep(currentStep, density, spread, accentDensity, slideDensity);
            outputs[OUTPUT_SLIDE].setVoltage(currentStepData.slide ? 10.f : 0.f);
        }

        // --- Update Lights ---
        // Generate light fades out
        generateLightBrightness *= 1.f - args.sampleTime * 4.f;
        lights[LIGHT_GENERATE].setBrightness(generateLightBrightness);

        // Step lights (show current position in first 16 steps)
        for (int i = 0; i < 16; i++) {
            bool isCurrentStep = (currentStep == i);
            bool hasNote = (i < patternLength) && !displayPattern.steps[i].isRest();

            if (isCurrentStep) {
                lights[LIGHT_STEP + i].setBrightness(1.f);
            } else if (hasNote) {
                lights[LIGHT_STEP + i].setBrightness(0.15f);
            } else {
                lights[LIGHT_STEP + i].setBrightness(0.f);
            }
        }

        // Octave indicator lights (-2 to +2, index 0-4)
        int octaveIndex = octaveOffset + 2;  // Convert -2..+2 to 0..4
        for (int i = 0; i < 5; i++) {
            lights[LIGHT_OCTAVE + i].setBrightness(i == octaveIndex ? 1.f : 0.1f);
        }
    }

    //-------------------------------------------------------------------------
    // JSON Serialization - Save/Restore State
    //-------------------------------------------------------------------------
    // Strategy: Save the seed to regenerate the master pattern deterministically.
    // Also save the master pattern as backup in case the generator algorithm changes.
    //
    // Saved data:
    //   - version: Schema version for future compatibility
    //   - seed: The RNG seed used to generate master pattern
    //   - currentStep: Playback position
    //   - masterPattern: Full master pattern backup (barActivationOrder, scalePriorityOrder, steps)
    //-------------------------------------------------------------------------

    static constexpr int JSON_VERSION = 3;

    json_t* dataToJson() override {
        json_t* rootJ = json_object();

        // Version for future compatibility
        json_object_set_new(rootJ, "version", json_integer(JSON_VERSION));

        // Core state
        json_object_set_new(rootJ, "seed", json_integer(currentSeed));
        json_object_set_new(rootJ, "currentStep", json_integer(currentStep));

        // Save master pattern
        json_t* masterJ = json_object();

        // Bar activation order
        json_t* barOrderJ = json_array();
        for (int i = 0; i < BAR_LEN; i++) {
            json_array_append_new(barOrderJ, json_integer(masterPattern.barActivationOrder[i]));
        }
        json_object_set_new(masterJ, "barActivationOrder", barOrderJ);

        // Scale priority order
        json_t* scaleOrderJ = json_array();
        for (int i = 0; i < SCALE_SIZE; i++) {
            json_array_append_new(scaleOrderJ, json_integer(masterPattern.scalePriorityOrder[i]));
        }
        json_object_set_new(masterJ, "scalePriorityOrder", scaleOrderJ);

        // Steps
        json_t* stepsJ = json_array();
        for (int i = 0; i < MAX_STEPS; i++) {
            json_t* stepJ = json_object();
            json_object_set_new(stepJ, "p", json_integer(masterPattern.steps[i].notePoolIndex));
            json_object_set_new(stepJ, "o", json_integer(masterPattern.steps[i].octave));
            json_object_set_new(stepJ, "a", json_real(masterPattern.steps[i].accentProb));
            json_object_set_new(stepJ, "s", json_real(masterPattern.steps[i].slideProb));
            json_object_set_new(stepJ, "m", json_boolean(masterPattern.muted[i]));
            json_array_append_new(stepsJ, stepJ);
        }
        json_object_set_new(masterJ, "steps", stepsJ);

        json_object_set_new(rootJ, "masterPattern", masterJ);

        // Save slide/portamento state for seamless restoration mid-playback
        json_object_set_new(rootJ, "currentSlideActive", json_boolean(currentSlideActive));
        json_object_set_new(rootJ, "currentPitch", json_real(currentPitch));
        json_object_set_new(rootJ, "slideTargetPitch", json_real(slideTargetPitch));
        json_object_set_new(rootJ, "slideRate", json_real(slideRate));

        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        // Check version (for future migrations)
        json_t* versionJ = json_object_get(rootJ, "version");
        int version = versionJ ? json_integer_value(versionJ) : 0;

        // Load seed
        json_t* seedJ = json_object_get(rootJ, "seed");
        if (seedJ) {
            currentSeed = static_cast<uint32_t>(json_integer_value(seedJ));
        }

        // Load playback position
        json_t* stepJ = json_object_get(rootJ, "currentStep");
        if (stepJ) {
            currentStep = json_integer_value(stepJ);
        }

        // Try to load master pattern (version 2+)
        bool loaded = false;
        json_t* masterJ = json_object_get(rootJ, "masterPattern");
        if (masterJ && version >= 2) {
            // Load bar activation order
            json_t* barOrderJ = json_object_get(masterJ, "barActivationOrder");
            if (barOrderJ) {
                for (int i = 0; i < BAR_LEN && i < (int)json_array_size(barOrderJ); i++) {
                    masterPattern.barActivationOrder[i] = json_integer_value(json_array_get(barOrderJ, i));
                }
            }

            // Load scale priority order
            json_t* scaleOrderJ = json_object_get(masterJ, "scalePriorityOrder");
            if (scaleOrderJ) {
                for (int i = 0; i < SCALE_SIZE && i < (int)json_array_size(scaleOrderJ); i++) {
                    masterPattern.scalePriorityOrder[i] = json_integer_value(json_array_get(scaleOrderJ, i));
                }
            }

            // Load steps
            json_t* stepsJ = json_object_get(masterJ, "steps");
            if (stepsJ) {
                for (int i = 0; i < MAX_STEPS && i < (int)json_array_size(stepsJ); i++) {
                    json_t* stepDataJ = json_array_get(stepsJ, i);
                    if (stepDataJ) {
                        json_t* pJ = json_object_get(stepDataJ, "p");
                        json_t* oJ = json_object_get(stepDataJ, "o");
                        json_t* aJ = json_object_get(stepDataJ, "a");
                        json_t* sJ = json_object_get(stepDataJ, "s");
                        json_t* mJ = json_object_get(stepDataJ, "m");

                        if (pJ) masterPattern.steps[i].notePoolIndex = json_integer_value(pJ);
                        if (oJ) masterPattern.steps[i].octave = json_integer_value(oJ);
                        if (aJ) masterPattern.steps[i].accentProb = json_real_value(aJ);
                        if (sJ) masterPattern.steps[i].slideProb = json_real_value(sJ);
                        if (mJ) masterPattern.muted[i] = json_boolean_value(mJ);
                    }
                }
            }

            loaded = true;
        }

        // Fallback: regenerate from seed (version 1 or missing data)
        if (!loaded) {
            generateMaster(currentSeed, masterPattern);
        }

        // Force display pattern update
        cachedDensity = -1.f;

        // Load slide/portamento state
        json_t* slideActiveJ = json_object_get(rootJ, "currentSlideActive");
        if (slideActiveJ) {
            currentSlideActive = json_boolean_value(slideActiveJ);
        }

        json_t* currentPitchJ = json_object_get(rootJ, "currentPitch");
        if (currentPitchJ) {
            currentPitch = static_cast<float>(json_real_value(currentPitchJ));
        }

        json_t* slideTargetJ = json_object_get(rootJ, "slideTargetPitch");
        if (slideTargetJ) {
            slideTargetPitch = static_cast<float>(json_real_value(slideTargetJ));
        }

        json_t* slideRateJ = json_object_get(rootJ, "slideRate");
        if (slideRateJ) {
            slideRate = static_cast<float>(json_real_value(slideRateJ));
        }
    }
};

//-----------------------------------------------------------------------------
// Pattern Visualization Widget - Shows note bars, accent/slide indicators
//-----------------------------------------------------------------------------

struct PatternDisplay : widget::OpaqueWidget {
    AcidSeq* module = nullptr;

    static constexpr const char* NOTE_NAMES[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

    void draw(const DrawArgs& args) override {
        NVGcontext* vg = args.vg;

        // Background
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 0, 0, box.size.x, box.size.y, 3.f);
        nvgFillColor(vg, nvgRGB(0x0a, 0x0a, 0x0a));
        nvgFill(vg);

        // Border
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 0, 0, box.size.x, box.size.y, 3.f);
        nvgStrokeColor(vg, nvgRGB(0x33, 0x33, 0x33));
        nvgStrokeWidth(vg, 1.f);
        nvgStroke(vg);

        int patternLength = module ? module->cachedPatternLength : 16;
        int currentStep = module ? module->currentStep : -1;

        // Auto-follow: calculate which page of 16 steps to show
        int viewOffset = 0;
        if (currentStep >= 0) {
            viewOffset = (currentStep / 16) * 16;  // Pages: 0-15, 16-31, 32-47, 48-63
        }

        // Layout
        float padding = 3.f;
        float barAreaWidth = box.size.x - padding * 2;
        float barWidth = barAreaWidth / 16.f - 1.f;
        float barMaxHeight = box.size.y - padding * 2 - 16.f;  // Leave room for indicators + page
        float indicatorY = box.size.y - padding - 12.f;

        // Draw page indicator at top-right
        if (patternLength > 16) {
            int currentPage = (viewOffset / 16) + 1;
            int totalPages = (patternLength + 15) / 16;
            char pageStr[8];
            snprintf(pageStr, sizeof(pageStr), "%d/%d", currentPage, totalPages);

            nvgFontSize(vg, 8.f);
            nvgFontFaceId(vg, APP->window->uiFont->handle);
            nvgFillColor(vg, nvgRGB(0x60, 0x60, 0x60));
            nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
            nvgText(vg, box.size.x - padding, padding, pageStr, nullptr);
        }

        for (int i = 0; i < 16; i++) {
            int stepIndex = viewOffset + i;  // Actual step index in pattern
            float x = padding + i * (barAreaWidth / 16.f) + 0.5f;

            // Get step data
            SequenceStep step;
            if (module && stepIndex < MAX_STEPS) {
                step = module->displayPattern.steps[stepIndex];
            }

            bool isRest = step.isRest();
            bool isCurrentStep = (stepIndex == currentStep);
            bool isOutsidePattern = (stepIndex >= patternLength);

            // Draw bar background (dim for steps outside pattern length)
            nvgBeginPath(vg);
            nvgRect(vg, x, padding, barWidth, barMaxHeight);
            if (isOutsidePattern) {
                nvgFillColor(vg, nvgRGB(0x15, 0x15, 0x15));
            } else {
                nvgFillColor(vg, nvgRGB(0x1a, 0x1a, 0x1a));
            }
            nvgFill(vg);

            if (!isRest && !isOutsidePattern) {
                // Calculate bar height based on note (0-6 range typically)
                float noteHeight = (step.note + 1) / 7.f;  // Normalize to 0-1
                noteHeight = std::min(1.f, std::max(0.15f, noteHeight));  // Clamp with minimum visibility

                // Octave affects brightness
                float octaveBrightness = 0.6f + step.octave * 0.2f;
                octaveBrightness = std::min(1.f, std::max(0.4f, octaveBrightness));

                float barHeight = noteHeight * barMaxHeight;
                float barY = padding + barMaxHeight - barHeight;

                // Bar color - cyan/teal, brighter for current step
                NVGcolor barColor;
                if (isCurrentStep) {
                    barColor = nvgRGB(0x79, 0xd8, 0xb9);  // Bright cyan
                } else {
                    int brightness = (int)(0x50 * octaveBrightness);
                    barColor = nvgRGB(brightness, (int)(0x90 * octaveBrightness), (int)(0x80 * octaveBrightness));
                }

                nvgBeginPath(vg);
                nvgRect(vg, x, barY, barWidth, barHeight);
                nvgFillColor(vg, barColor);
                nvgFill(vg);
            }

            // Current step indicator (bottom line)
            if (isCurrentStep && !isOutsidePattern) {
                nvgBeginPath(vg);
                nvgRect(vg, x, padding + barMaxHeight + 1, barWidth, 2.f);
                nvgFillColor(vg, nvgRGB(0xff, 0xff, 0xff));
                nvgFill(vg);
            }

            // Accent indicator (small dot)
            if (!isRest && step.accent && !isOutsidePattern) {
                float dotX = x + barWidth / 2;
                float dotY = indicatorY;
                nvgBeginPath(vg);
                nvgCircle(vg, dotX, dotY, 2.f);
                nvgFillColor(vg, isCurrentStep ? nvgRGB(0xff, 0x80, 0x40) : nvgRGB(0xaa, 0x55, 0x22));
                nvgFill(vg);
            }

            // Slide indicator (small triangle/line below accent)
            if (!isRest && step.slide && !isOutsidePattern) {
                float dotX = x + barWidth / 2;
                float dotY = indicatorY + 4.f;
                nvgBeginPath(vg);
                nvgMoveTo(vg, dotX - 2.f, dotY);
                nvgLineTo(vg, dotX + 2.f, dotY);
                nvgLineTo(vg, dotX + 4.f, dotY + 2.f);
                nvgStrokeColor(vg, isCurrentStep ? nvgRGB(0x40, 0x80, 0xff) : nvgRGB(0x22, 0x55, 0xaa));
                nvgStrokeWidth(vg, 1.5f);
                nvgStroke(vg);
            }
        }
    }
};

//-----------------------------------------------------------------------------
// Scale/Root + Current Note Display Widget
//-----------------------------------------------------------------------------

struct InfoDisplay : widget::OpaqueWidget {
    AcidSeq* module = nullptr;

    static constexpr const char* NOTE_NAMES[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

    static const char* getScaleAbbrev(Scale scale) {
        switch (scale) {
            case Scale::MAJOR: return "MAJ";
            case Scale::MINOR: return "MIN";
            case Scale::DORIAN: return "DOR";
            case Scale::MIXOLYDIAN: return "MIX";
            case Scale::LYDIAN: return "LYD";
            case Scale::PHRYGIAN: return "PHR";
            case Scale::LOCRIAN: return "LOC";
            case Scale::HARMONIC_MINOR: return "H-m";
            case Scale::HARMONIC_MAJOR: return "H-M";
            case Scale::DORIAN_NR_4: return "D#4";
            case Scale::PHRYGIAN_DOMINANT: return "PhD";
            case Scale::MELODIC_MINOR: return "Mm";
            case Scale::LYDIAN_AUGMENTED: return "L+";
            case Scale::LYDIAN_DOMINANT: return "LD";
            case Scale::HUNGARIAN_MINOR: return "HUN";
            case Scale::SUPER_LOCRIAN: return "SuL";
            case Scale::SPANISH: return "SPA";
            case Scale::BHAIRAV: return "BHV";
            case Scale::PENTATONIC_MINOR: return "Pm";
            case Scale::PENTATONIC_MAJOR: return "PM";
            case Scale::BLUES_MINOR: return "BLU";
            case Scale::WHOLE_TONE: return "WHL";
            case Scale::CHROMATIC: return "CHR";
            case Scale::JAPANESE_IN_SEN: return "INS";
            default: return "---";
        }
    }

    void draw(const DrawArgs& args) override {
        NVGcontext* vg = args.vg;

        // Background
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 0, 0, box.size.x, box.size.y, 2.f);
        nvgFillColor(vg, nvgRGB(0x0a, 0x0a, 0x0a));
        nvgFill(vg);

        // Get values
        Scale scale = module ? module->cachedScale : Scale::MINOR;
        int rootNote = module ? module->cachedRootNote : 0;
        int currentStep = module ? module->currentStep : -1;
        int patternLength = module ? module->cachedPatternLength : 16;

        const char* rootName = NOTE_NAMES[rootNote % 12];
        const char* scaleName = getScaleAbbrev(scale);

        // Get current playing note
        char currentNoteStr[8] = "---";
        if (module && currentStep >= 0 && currentStep < patternLength) {
            SequenceStep step = module->displayPattern.steps[currentStep];
            if (!step.isRest()) {
                int noteInScale = step.note % 7;
                int octave = step.octave + 4;  // Base octave
                // Get the actual note name based on scale and root
                int midiNote = getNoteInScale(step.note, scale, rootNote, step.octave);
                const char* noteName = NOTE_NAMES[midiNote % 12];
                snprintf(currentNoteStr, sizeof(currentNoteStr), "%s%d", noteName, octave);
            }
        }

        // Draw scale/root on left
        nvgFontSize(vg, 10.f);
        nvgFontFaceId(vg, APP->window->uiFont->handle);
        nvgFillColor(vg, nvgRGB(0x79, 0xd8, 0xb9));
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

        char scaleStr[16];
        snprintf(scaleStr, sizeof(scaleStr), "%s %s", rootName, scaleName);
        nvgText(vg, 4, box.size.y / 2, scaleStr, nullptr);

        // Draw current note on right (brighter)
        nvgFillColor(vg, nvgRGB(0xff, 0xff, 0xff));
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgText(vg, box.size.x - 4, box.size.y / 2, currentNoteStr, nullptr);
    }
};

//-----------------------------------------------------------------------------
// Module Widget (Panel UI) - 12HP version for 4ms Metamodule
//-----------------------------------------------------------------------------

struct AcidSeqWidget : ModuleWidget {
    AcidSeqWidget(AcidSeq* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/AcidGen.svg")));

        // Screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Layout constants for 12HP (60.96mm width)
        const float COL1 = 12.f;     // Left column
        const float COL2 = 30.48f;   // Center column
        const float COL3 = 49.f;     // Right column
        const float CENTER = 30.48f; // Module center

        // === Row 1: Main knobs (Density, Spread, Length) ===
        addParam(createParamCentered<Rogan1PWhite>(mm2px(Vec(COL1, 20)), module, AcidSeq::PARAM_DENSITY));
        addParam(createParamCentered<Rogan1PWhite>(mm2px(Vec(COL2, 20)), module, AcidSeq::PARAM_SPREAD));
        addParam(createParamCentered<Rogan1PWhite>(mm2px(Vec(COL3, 20)), module, AcidSeq::PARAM_PATTERN_LENGTH));

        // === Row 2: Small knobs (Acc, Sld, Root, Scale) ===
        addParam(createParamCentered<Rogan1PWhite>(mm2px(Vec(COL1, 38)), module, AcidSeq::PARAM_ACCENT_DENSITY));
        addParam(createParamCentered<Rogan1PWhite>(mm2px(Vec(24, 38)), module, AcidSeq::PARAM_SLIDE_DENSITY));
        addParam(createParamCentered<Rogan1PWhite>(mm2px(Vec(37, 38)), module, AcidSeq::PARAM_ROOT_NOTE));
        addParam(createParamCentered<Rogan1PWhite>(mm2px(Vec(COL3, 38)), module, AcidSeq::PARAM_SCALE));

        // === Pattern Display (note bars with accent/slide indicators) ===
        {
            PatternDisplay* patternDisp = new PatternDisplay();
            patternDisp->box.pos = mm2px(Vec(4, 46));
            patternDisp->box.size = mm2px(Vec(52.96, 22));
            patternDisp->module = module;
            addChild(patternDisp);
        }

        // === Info Display (Scale/Root + Current Note) ===
        {
            InfoDisplay* infoDisp = new InfoDisplay();
            infoDisp->box.pos = mm2px(Vec(4, 70));
            infoDisp->box.size = mm2px(Vec(36, 7));
            infoDisp->module = module;
            addChild(infoDisp);
        }

        // === Generate Button with LED (button left, LED right) ===
        addParam(createParamCentered<VCVButton>(mm2px(Vec(46, 73.5)), module, AcidSeq::PARAM_GENERATE));
        addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(54, 73.5)), module, AcidSeq::LIGHT_GENERATE));

        // === Octave Controls (buttons + LED indicators) ===
        // Octave down button
        addParam(createParamCentered<TL1105>(mm2px(Vec(8, 82)), module, AcidSeq::PARAM_OCTAVE_DOWN));
        // Octave up button
        addParam(createParamCentered<TL1105>(mm2px(Vec(20, 82)), module, AcidSeq::PARAM_OCTAVE_UP));

        // Octave LED indicators (-2, -1, 0, +1, +2)
        for (int i = 0; i < 5; i++) {
            float x = 30.f + i * 5.5f;
            addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(x, 82)), module, AcidSeq::LIGHT_OCTAVE + i));
        }

        // === Inputs Row (well-spaced) ===
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10, 100)), module, AcidSeq::INPUT_CLOCK));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(24, 100)), module, AcidSeq::INPUT_RESET));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(38, 100)), module, AcidSeq::INPUT_GENERATE));

        // === Outputs Row (well-spaced) ===
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10, 117)), module, AcidSeq::OUTPUT_PITCH));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(24, 117)), module, AcidSeq::OUTPUT_GATE));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(38, 117)), module, AcidSeq::OUTPUT_ACCENT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(51, 117)), module, AcidSeq::OUTPUT_SLIDE));
    }

    // Context menu for scale selection
    void appendContextMenu(Menu* menu) override {
        AcidSeq* module = dynamic_cast<AcidSeq*>(this->module);
        if (!module) return;

        menu->addChild(new MenuSeparator());
        menu->addChild(createMenuLabel("Scale"));

        for (int i = 0; i < static_cast<int>(Scale::NUM_SCALES); i++) {
            Scale s = static_cast<Scale>(i);
            menu->addChild(createCheckMenuItem(
                getScaleName(s),
                "",
                [=]() { return static_cast<int>(module->params[AcidSeq::PARAM_SCALE].getValue()) == i; },
                [=]() { module->params[AcidSeq::PARAM_SCALE].setValue(static_cast<float>(i)); }
            ));
        }
    }
};

Model* modelAcidSeq = createModel<AcidSeq, AcidSeqWidget>("AcidGenMini");
