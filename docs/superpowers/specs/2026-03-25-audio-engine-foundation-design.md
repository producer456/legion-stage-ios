# Sub-project 1: Audio Engine Foundation — Design Spec

## Purpose

First sub-project of a clip-based music sequencer. Establishes the JUCE project structure, build system, and ASIO audio output. Outputs a test tone to prove the full audio chain works end-to-end.

This is the foundation that all subsequent sub-projects (VST hosting, MIDI input, track system, clip sequencer) build on.

## Tech Stack

- **Framework:** JUCE 7.0.12 (pinned via git submodule tag)
- **Build:** CMake, targeting Visual Studio 2019 (x64)
- **Language:** C++17
- **JUCE as:** Git submodule in `libs/JUCE/`

## Project Structure

```
C:/dev/sequencer/
  CMakeLists.txt              — top-level CMake, adds JUCE subdir, defines app target
  .gitignore                  — build/, .vs/, *.user, *.suo (libs/JUCE tracked as submodule, NOT ignored)
  libs/
    JUCE/                     — JUCE git submodule (github.com/juce-framework/JUCE)
  src/
    Main.cpp                  — JUCEApplication subclass, creates MainWindow
    MainComponent.h           — AudioAppComponent subclass declaration
    MainComponent.cpp         — Audio device setup, test tone generation, UI
  docs/
    superpowers/
      specs/                  — this spec
```

## CMake Configuration

- `cmake_minimum_required(VERSION 3.22)`
- `project(Sequencer VERSION 0.1.0)`
- `add_subdirectory(libs/JUCE)`
- Use `juce_add_gui_app` (not plugin — this is a standalone app)
- Product name: "Sequencer"
- Company: "Dev"
- Link against: `juce::juce_audio_utils`, `juce::juce_audio_devices`
- JUCE flags: `JUCE_ASIO=1` (enable ASIO support), `JUCE_WASAPI=1`, `JUCE_DIRECTSOUND=1`
- ASIO SDK: Must be downloaded separately from Steinberg (free registration). Extract and set `JUCE_ASIO_SDK_DIR` to the SDK path, or place the SDK in `libs/asio-sdk/` and configure in CMake.
- Transitive dependencies (juce_core, juce_audio_basics, etc.) are pulled in automatically by the listed modules.

## Application Architecture

### Main.cpp

Standard JUCE standalone app boilerplate:
- `SequencerApplication` extends `juce::JUCEApplication`
- `getApplicationName()` returns "Sequencer"
- `initialise()` creates a `MainWindow`
- `MainWindow` extends `juce::DocumentWindow`, contains `MainComponent`
- Window size: 800x600, resizable

### MainComponent (AudioAppComponent)

The main component inherits from `juce::AudioAppComponent`.

**Constructor:**
- Calls `setAudioChannels(0, 2)` — 0 inputs, 2 outputs (stereo)
- This triggers JUCE's audio device manager setup
- Sets component size to 800x600

**Audio callbacks:**
- `prepareToPlay(samplesPerBlock, sampleRate)` — stores sample rate, resets phase
- `getNextAudioBlock(bufferToFill)` — if test tone enabled, fills buffer with sine wave (440Hz). Otherwise fills with silence.
- `releaseResources()` — cleanup

**Test tone generation:**
- Simple sine wave at 440Hz, amplitude 0.25
- Phase accumulator incremented per sample: `phase += 2π × 440 / sampleRate`
- Wrap phase at 2π
- Same signal to both channels (mono test tone)
- The "test tone enabled" flag must be a `std::atomic<bool>` since it is written on the message thread (button click) and read on the audio thread (`getNextAudioBlock`)

**UI (minimal):**
- "Audio Settings" button — opens `juce::AudioDeviceSelectorComponent` in a dialog window. This lets the user select ASIO driver, sample rate, buffer size.
- "Test Tone" toggle button — enables/disables the sine wave
- Status label showing current audio device name and sample rate
- All laid out vertically, simple `setBounds` in `resized()`

## ASIO Support

- Enabled via CMake flag `JUCE_ASIO=1`
- ASIO SDK must be downloaded from Steinberg's developer portal (free, requires registration)
- Extract to `libs/asio-sdk/` — the `common/` directory containing `asiosdk.h` etc. must be accessible
- CMake must set the include path so JUCE can find the ASIO headers
- The `AudioDeviceSelectorComponent` will automatically list available ASIO drivers
- User can select their ASIO driver (e.g., Steinberg UR-C ASIO, or whatever interface they have)
- If no ASIO driver is available, WASAPI and DirectSound are also enabled as fallbacks

## Build Instructions

```bash
cd /c/dev/sequencer
git init
git submodule add https://github.com/juce-framework/JUCE.git libs/JUCE
cd libs/JUCE && git checkout 7.0.12 && cd ../..
# Download ASIO SDK from Steinberg, extract to libs/asio-sdk/
cmake -B build -G "Visual Studio 16 2019" -A x64 -DJUCE_ASIO_SDK_DIR=libs/asio-sdk
cmake --build build --config Release
# Run: look for Sequencer.exe under build/ (exact path depends on CMake generator)
```

## Out of Scope (for this sub-project)

- VST plugin loading
- MIDI input
- Multiple tracks
- Sequencing / clips
- Save/load
- Any UI beyond audio settings + test tone toggle

## Success Criteria

1. Project builds with CMake + VS 2019 without errors
2. Application launches and shows the window
3. User can open Audio Settings and select an ASIO driver
4. Test tone toggle produces audible sine wave through selected output
5. No audio glitches at reasonable buffer sizes (256-1024 samples)
