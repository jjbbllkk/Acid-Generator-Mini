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
        LIGHTS_LEN
    };

    // Schmitt triggers for edge detection
    dsp::SchmittTrigger clockTrigger;
    dsp::SchmittTrigger resetTrigger;
    dsp::SchmittTrigger generateTrigger;
    dsp::SchmittTrigger generateButtonTrigger;

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
// Module Widget (Panel UI) - Compact 8HP version for 4ms Metamodule
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

        // Layout constants for 8HP (40.64mm width)
        const float COL1 = 10.16f;  // Left column center
        const float COL2 = 30.48f;  // Right column center
        const float CENTER = 20.32f;  // Module center

        // === Row 1: Density & Length ===
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL1, 20)), module, AcidSeq::PARAM_DENSITY));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL2, 20)), module, AcidSeq::PARAM_PATTERN_LENGTH));

        // === Row 2: Spread & Scale ===
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL1, 38)), module, AcidSeq::PARAM_SPREAD));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL2, 38)), module, AcidSeq::PARAM_SCALE));

        // === Row 3: Accent/Slide & Root/Octave ===
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(COL1 - 5, 54)), module, AcidSeq::PARAM_ACCENT_DENSITY));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(COL1 + 5, 54)), module, AcidSeq::PARAM_SLIDE_DENSITY));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(COL2 - 5, 54)), module, AcidSeq::PARAM_ROOT_NOTE));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(COL2 + 5, 54)), module, AcidSeq::PARAM_OCTAVE));

        // === Generate Button with LED (centered) ===
        addParam(createParamCentered<VCVButton>(mm2px(Vec(CENTER, 68)), module, AcidSeq::PARAM_GENERATE));
        addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(CENTER, 62)), module, AcidSeq::LIGHT_GENERATE));

        // === Step Indicator LEDs (2 rows of 8) ===
        for (int i = 0; i < 8; i++) {
            float x = 5.f + i * 4.5f;
            addChild(createLightCentered<TinyLight<GreenLight>>(mm2px(Vec(x, 78)), module, AcidSeq::LIGHT_STEP + i));
            addChild(createLightCentered<TinyLight<GreenLight>>(mm2px(Vec(x, 82)), module, AcidSeq::LIGHT_STEP + 8 + i));
        }

        // === Inputs Row ===
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL1 - 5, 94)), module, AcidSeq::INPUT_CLOCK));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL1 + 5, 94)), module, AcidSeq::INPUT_RESET));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(CENTER, 94)), module, AcidSeq::INPUT_GENERATE));

        // === Outputs Row ===
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COL1 - 5, 110)), module, AcidSeq::OUTPUT_PITCH));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COL1 + 5, 110)), module, AcidSeq::OUTPUT_GATE));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COL2 - 5, 110)), module, AcidSeq::OUTPUT_ACCENT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COL2 + 5, 110)), module, AcidSeq::OUTPUT_SLIDE));
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

Model* modelAcidSeq = createModel<AcidSeq, AcidSeqWidget>("AcidGen");
