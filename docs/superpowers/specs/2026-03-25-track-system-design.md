# Sub-project 4: 16-Track System — Design Spec

## Purpose

Expand from a single plugin slot to 16 independent tracks, each with an instrument plugin, volume/pan controls, and mute/solo. All tracks mix to a master stereo output. MIDI input routes to the selected (armed) track.

Builds on Sub-project 3 (MIDI input routing to VST3 plugins).

## Architecture

### Track Structure

Each track is represented by a struct:

```cpp
struct Track {
    int index;                              // 0-15
    juce::String name;                      // "Track 1" - "Track 16"
    AudioProcessorGraph::Node::Ptr pluginNode;    // instrument plugin (nullable)
    AudioProcessorGraph::Node::Ptr gainNode;      // GainProcessor for volume/pan
    float volume = 0.8f;                    // 0.0 - 1.0
    float pan = 0.0f;                       // -1.0 (left) to 1.0 (right)
    bool muted = false;
    bool soloed = false;
};
```

### GainProcessor

A minimal custom `AudioProcessor` that applies volume and pan to a stereo signal:

- Reads `volume` and `pan` from atomic floats (set from UI thread)
- `processBlock`: for each sample, multiply left channel by `volume * leftGain` and right by `volume * rightGain`, where gains are derived from equal-power pan law
- If muted, output silence
- No editor, no parameters — just raw float members

### Audio Graph Layout

```
MIDI Input Node ──MIDI──► Track 0 Plugin ──audio──► Track 0 Gain ──►┐
                          Track 1 Plugin ──audio──► Track 1 Gain ──►├──► Audio Output Node
                          ...                                       │
                          Track 15 Plugin ──audio──► Track 15 Gain ─►┘
```

- Each track has its own plugin node and gain node
- All gain nodes connect their stereo output to the audio output node (summing)
- MIDI input node connects to the **selected track's** plugin only
- When the selected track changes, the MIDI connection is moved

### MIDI Routing

- Only one track receives MIDI at a time (the "selected" track)
- When the user clicks a different track in the track list:
  1. Remove MIDI connection from old track's plugin
  2. Add MIDI connection to new track's plugin
- This can be done without suspending audio — `AudioProcessorGraph::addConnection/removeConnection` is safe for MIDI connections while running
- If the selected track has no plugin loaded, MIDI goes nowhere (acceptable)

### Solo Logic

- If any track is soloed, only soloed tracks produce audio (non-soloed tracks are effectively muted)
- Multiple tracks can be soloed simultaneously
- Solo state is checked in `GainProcessor::processBlock` — if any solo is active globally and this track is not soloed, output silence
- Use a shared `std::atomic<int> soloCount` in PluginHost that GainProcessors reference

## UI Layout

Functional layout — no fancy graphics, just JUCE components:

```
┌──────────────────────────────────────────────────────────────┐
│ [MIDI Input: ▼ dropdown___________] [Refresh]                │
│ [Plugin: ▼ dropdown_______________] [Open Editor] [Test Note]│
│ [Audio Settings]                                             │
├────────────────────────┬─────────────────────────────────────┤
│  Track List (left)     │  (empty space / future viewer)      │
│  ► Track 1  [Diva] ▓░ │                                     │
│    Track 2  [----] ▓░  │                                     │
│    Track 3  [----] ▓░  │                                     │
│    ...                 │                                     │
│    Track 16 [----] ▓░  │                                     │
├────────────────────────┴─────────────────────────────────────┤
│ Status: Track 1 | Loaded: Diva | MIDI: LPMiniMK3 | 44100 Hz │
└──────────────────────────────────────────────────────────────┘
```

### Track List Item

Each track row shows:
- Track number (highlighted if selected/armed)
- Plugin name (or "----" if empty)
- Volume slider (horizontal, small)
- Mute button [M]
- Solo button [S]

Click anywhere on the track row to select it. The plugin dropdown at the top loads into the selected track.

## File Structure

```
src/
  Main.cpp                  — unchanged
  MainComponent.h           — modify: add track list UI, selected track index
  MainComponent.cpp         — modify: track list rendering, selection, route plugin actions to selected track
  PluginHost.h              — major rewrite: Track struct array, per-track plugin/gain nodes
  PluginHost.cpp            — major rewrite: multi-track graph management
  GainProcessor.h           — new: simple volume/pan AudioProcessor
  GainProcessor.cpp         — new: processBlock with volume/pan/mute
  TrackComponent.h          — new: single track row UI component
  TrackComponent.cpp        — new: volume slider, mute/solo buttons, selection highlight
```

## CMake Changes

Add new source files to `target_sources`:
- `src/GainProcessor.h`, `src/GainProcessor.cpp`
- `src/TrackComponent.h`, `src/TrackComponent.cpp`

## Thread Safety

- Volume/pan/mute are `std::atomic<float>` / `std::atomic<bool>` — written from UI, read from audio thread
- Solo count is `std::atomic<int>` — shared across all GainProcessors
- MIDI connection changes (selecting tracks) are safe without suspending audio
- Plugin loading still requires suspend/resume (same as Sub-project 2)
- GainProcessor reads atomics in processBlock — no locks needed

## Error Handling

- Loading a plugin on a track that already has one: unload old, load new (same as before)
- Solo with no tracks soloed: all tracks play (solo disengaged)
- Selected track has no plugin: MIDI goes nowhere, "Play Test Note" does nothing, status shows "No plugin"

## Out of Scope

- Effect plugin chains (future enhancement)
- Track reordering
- Track naming (auto-named "Track 1" through "Track 16")
- Master volume/pan
- Metering / level display
- Undo/redo

## Success Criteria

1. 16 tracks visible in the track list
2. Clicking a track selects it (highlighted) and arms it for MIDI
3. Plugin dropdown loads instruments into the selected track
4. Each track can have a different plugin loaded simultaneously
5. Volume sliders control per-track volume
6. Mute buttons silence individual tracks
7. Solo buttons isolate tracks
8. MIDI input plays the selected track's plugin
9. Multiple plugins producing audio simultaneously without glitches
10. Switching selected track moves MIDI routing cleanly
