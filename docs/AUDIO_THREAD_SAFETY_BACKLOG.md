# Audio-thread safety backlog

Findings from the 2026-06-10 full-codebase review. Root theme: UI/MIDI threads
mutate audio-engine data (clips, MIDI events, samples, graph topology, params)
without synchronizing against the audio thread. The app has the right primitives
(`PluginHost::ScopedGraphEdit` / `suspendProcessing`, `ClipPlayerNode::clearSlotDeferred`
+ `maintain`, atomics) — most fixes are routing the bypassing paths through them.

Severity: CRITICAL = crash/UAF · HIGH = glitch or wrong output · MED/LOW = quality.

## Done
- [x] CRITICAL clip-slot UAF in `restoreSnapshot` + RT recording-buffer growth — `b18193d`
- [x] CRITICAL plugin-graph topology race (load/unload/rewire/midi-route) — `947f294`

## Tier 2 — RT allocations + clip edits
- [x] HIGH record-START allocation on the audio thread — spare-clip pool `b8e129e`.
- [x] HIGH Juno arpeggiator allocates in the RT callback — fixed-capacity buffers + array velocity (commit below).
- [ ] CRITICAL clip-event edits bypass sync — PianoRoll `applyNoteListToClip`
      (`PianoRollComponent.cpp:92`); Timeline split/resize/transpose/velocity/quantize
      (`:587/997/1065/2025/2066`), delete (`:864` → use `clearSlotDeferred`), cross-track
      move (`:551`). Fix: edit under suspend or a stopped+flushed slot; deferred free.
- [ ] CRITICAL `PianoRollWindow` holds a `MidiClip&` that dangles after clip delete /
      project reload. Fix: weak handle (track+slot) re-resolved each access; close
      open windows on delete/reload.

## Tier 3 — sampler + controllers + export
- [ ] CRITICAL sampler swaps sample buffer/kit map while voices read it —
      `BuiltinSamplerProcessor.h:56-158`. Fix: publish `shared_ptr<const SampleSet>`
      via one atomic store; voices grab it at note-on.
- [ ] CRITICAL `ChordDetector` raced MIDI-thread vs UI-thread — `MainComponent.cpp:2632`
      (`std::set` + `juce::String` refcount). Fix: lock or single-thread feed.
- [ ] CRITICAL Launchkey launches clips from the MIDI thread — `LaunchkeyMK4Controller.cpp:229`
      touches SessionView + repaint. Fix: marshal via `controllerLaunchClipAt`/callAsync.
- [ ] HIGH `midiMappings` Array reallocated under MIDI-thread iteration —
      `MainComponent.cpp:2658` vs `:8234`. Fix: lock or atomic snapshot-swap.
- [ ] HIGH `out()`/`deviceOutputs` lazy map-insert + concurrent sends raced across
      MIDI + timer threads — all four controllers / `MainComponent.cpp:9224`. Fix:
      resolve+cache MidiOutput* on the message thread; don't open from MIDI callback.
- [x] HIGH SpinLock held across plugin setValue (live + offline paths) — collect-then-apply.
- [ ] HIGH offline export races the live audio device — `AudioExporter.h:117`. Fix:
      stop/suspend the live callback before `releaseResources()`/`setNonRealtime`.
- [ ] HIGH capture ring buffer multi-producer race — `MainComponent.cpp:2924` (MIDI+UI).
      Fix: single `fetch_add` reservation, or one producer.
- [ ] HIGH `controllerSaveSnapshot` reads `juce::Slider` off the message thread —
      `MainComponent.cpp:8998`. Fix: marshal the body via callAsync (like its siblings).
- [x] HIGH missing graph prepareToPlay on the Juno load path.
- [ ] MED Launchkey mode/animation scalars shared MIDI↔message unsynchronized —
      `LaunchkeyMK4Controller.cpp:191-272` (make `std::atomic`).

## Tier 4 — quality / lower stakes
- [ ] MED gain smoother coeff is sample-rate-dependent + no denormal flush — `GainProcessor.cpp:42-50`.
- [ ] MED audio thread clobbers UI-owned touch index — `PluginHost.cpp:649`.
- [ ] MED LadderFilter `tan(g)` unclamped near Nyquist — `Juno60/LadderFilter.cpp:44`.
- [ ] MED Metal renderer `waitUntilCompleted` hitches + unfenced buffer swap —
      `MetalVisualizerRenderer.mm:226-276`.
- [ ] LOW `getMillisecondCounter` on the RT path — `PluginHost.cpp:645`, `GainProcessor.cpp:24,79`.
- [x] LOW per-block param-array copy in offline export loop (auto&).
- [ ] LOW 60Hz paint does string formatting + per-pixel waveform loops — `TimelineComponent` paint.
- [ ] LOW `getSlot` not bounds-checked vs `slotCount`; clear cached selection on reload.
- [ ] LOW Midi2Handler `headerLen` not clamped before JSON copy — `Midi2Handler.cpp:268`.
