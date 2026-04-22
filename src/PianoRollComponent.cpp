#include "PianoRollComponent.h"
#include "DawLookAndFeel.h"
#include <set>
#include <map>

PianoRollComponent::PianoRollComponent(MidiClip& c, SequencerEngine& eng)
    : clip(c), engine(eng)
{
    rebuildNoteList();
    scrollY = 48; // start around C3-C5 range
    startTimerHz(30); // 30fps playhead update

    setWantsKeyboardFocus(true);

    // Grid selector setup
    addAndMakeVisible(gridSelector);
    gridSelector.addItem("1/4", 1);
    gridSelector.addItem("1/8", 2);
    gridSelector.addItem("1/16", 3);
    gridSelector.addItem("1/32", 4);
    gridSelector.addItem("1/4T", 5);
    gridSelector.addItem("1/8T", 6);
    gridSelector.setSelectedId(3); // default 1/16
    gridSelector.onChange = [this] {
        switch (gridSelector.getSelectedId()) {
            case 1: gridResolution = 1.0; break;
            case 2: gridResolution = 0.5; break;
            case 3: gridResolution = 0.25; break;
            case 4: gridResolution = 0.125; break;
            case 5: gridResolution = 2.0 / 3.0; break;
            case 6: gridResolution = 1.0 / 3.0; break;
        }
        repaint();
    };

    // Snap toggle setup
    addAndMakeVisible(snapButton);
    snapButton.setClickingTogglesState(true);
    snapButton.setToggleState(true, juce::dontSendNotification);
    snapButton.onClick = [this] {
        snapEnabled = snapButton.getToggleState();
        repaint();
    };
}

// ── Note list management ─────────────────────────────────────────────────────

void PianoRollComponent::rebuildNoteList()
{
    noteEvents.clear();

    for (int i = 0; i < clip.events.getNumEvents(); ++i)
    {
        auto* event = clip.events.getEventPointer(i);
        if (event->message.isNoteOn())
        {
            NoteEvent n;
            n.noteNumber = event->message.getNoteNumber();
            n.startBeat = event->message.getTimeStamp();
            n.velocity = event->message.getVelocity();

            // Find matching note-off
            n.lengthBeats = 0.25; // default
            if (event->noteOffObject != nullptr)
            {
                n.lengthBeats = event->noteOffObject->message.getTimeStamp() - n.startBeat;
                if (n.lengthBeats < 0.05) n.lengthBeats = 0.25;
            }

            noteEvents.add(n);
        }
    }
}

void PianoRollComponent::applyNoteListToClip()
{
    clip.events.clear();

    for (auto& n : noteEvents)
    {
        auto noteOn = juce::MidiMessage::noteOn(1, n.noteNumber, (juce::uint8) n.velocity);
        noteOn.setTimeStamp(n.startBeat);

        auto noteOff = juce::MidiMessage::noteOff(1, n.noteNumber);
        noteOff.setTimeStamp(n.startBeat + n.lengthBeats);

        clip.events.addEvent(noteOn);
        clip.events.addEvent(noteOff);
    }

    clip.events.sort();
    clip.events.updateMatchedPairs();
}

// ── Coordinate conversion ────────────────────────────────────────────────────

double PianoRollComponent::xToBeat(float x) const
{
    return scrollX + (x - pianoKeyWidth) / pixelsPerBeat;
}

float PianoRollComponent::beatToX(double beat) const
{
    return pianoKeyWidth + static_cast<float>((beat - scrollX) * pixelsPerBeat);
}

int PianoRollComponent::yToNote(float y) const
{
    float adjustedY = y - toolbarHeight;
    int visNote = static_cast<int>(adjustedY / noteHeight);
    return (scrollY + visibleNotes() - 1) - visNote;
}

float PianoRollComponent::noteToY(int note) const
{
    int row = (scrollY + visibleNotes() - 1) - note;
    return static_cast<float>(row * noteHeight) + toolbarHeight;
}

juce::Rectangle<float> PianoRollComponent::getNoteRect(const NoteEvent& n) const
{
    float x = beatToX(n.startBeat);
    float y = noteToY(n.noteNumber);
    float w = static_cast<float>(n.lengthBeats * pixelsPerBeat);
    return { x, y, juce::jmax(4.0f, w), static_cast<float>(noteHeight - 1) };
}

// ── Hit testing ──────────────────────────────────────────────────────────────

int PianoRollComponent::hitTestNote(float x, float y) const
{
    for (int i = noteEvents.size() - 1; i >= 0; --i)
    {
        if (getNoteRect(noteEvents[i]).contains(x, y))
            return i;
    }
    return -1;
}

bool PianoRollComponent::isOnNoteRightEdge(float x, const NoteEvent& n) const
{
    auto rect = getNoteRect(n);
    return x > rect.getRight() - 6.0f;
}

// ── Mouse handling ───────────────────────────────────────────────────────────

int PianoRollComponent::velocityLaneHitTest(float x) const
{
    for (int i = 0; i < noteEvents.size(); ++i)
    {
        auto& ne = noteEvents.getReference(i);
        float nx = beatToX(ne.startBeat);
        float nw = static_cast<float>(ne.lengthBeats * pixelsPerBeat);
        if (nw < 4.0f) nw = 4.0f;
        if (x >= nx && x <= nx + nw)
            return i;
    }
    return -1;
}

void PianoRollComponent::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();

    if (e.source.getIndex() > 0) {
        touchScrolling = true;
        lastTouchX = e.position.x;
        lastTouchY = e.position.y;
        return;
    }

    // Ignore clicks in toolbar area (handled by child components)
    if (e.position.y < toolbarHeight)
        return;

    if (e.position.x < pianoKeyWidth) {
        int note = yToNote(e.position.y);
        if (note >= 0 && note <= 127 && onNotePreview)
            onNotePreview(note, true);
        return;
    }

    float mx = static_cast<float>(e.x);
    float my = static_cast<float>(e.y);

    // --- Velocity lane click ---
    if (my >= velLaneTop())
    {
        int hitIdx = velocityLaneHitTest(mx);
        if (hitIdx >= 0)
        {
            // If the note is selected, we'll adjust all selected; otherwise just this one
            if (selectedNotes.find(hitIdx) == selectedNotes.end())
            {
                selectedNotes.clear();
                selectedNotes.insert(hitIdx);
            }
            primarySelectedNote = hitIdx;
            dragMode = VelocityLaneDrag;

            // Store original velocities for proportional adjustment
            selectedOrigStates.clear();
            for (int idx : selectedNotes)
            {
                auto& sn = noteEvents.getReference(idx);
                selectedOrigStates[idx] = { sn.startBeat, sn.noteNumber, sn.lengthBeats, sn.velocity };
            }
        }
        repaint();
        return;
    }

    int hitIndex = hitTestNote(mx, my);

    if (e.mods.isRightButtonDown() || (hitIndex >= 0 && e.getNumberOfClicks() == 2))
    {
        // Right click or double-tap = delete note(s)
        if (hitIndex >= 0)
        {
            if (selectedNotes.find(hitIndex) != selectedNotes.end())
            {
                // Delete all selected notes (remove from highest index first)
                std::vector<int> toRemove(selectedNotes.begin(), selectedNotes.end());
                std::sort(toRemove.rbegin(), toRemove.rend());
                for (int idx : toRemove)
                    noteEvents.remove(idx);
            }
            else
            {
                noteEvents.remove(hitIndex);
            }
            selectedNotes.clear();
            primarySelectedNote = -1;
            applyNoteListToClip();
            repaint();
        }
        return;
    }

    if (hitIndex >= 0)
    {
        // Clicking a note
        if (e.mods.isShiftDown())
        {
            // Shift+click toggles selection
            if (selectedNotes.find(hitIndex) != selectedNotes.end())
                selectedNotes.erase(hitIndex);
            else
                selectedNotes.insert(hitIndex);
            primarySelectedNote = hitIndex;
        }
        else
        {
            // Regular click: if not already selected, select only this one
            if (selectedNotes.find(hitIndex) == selectedNotes.end())
            {
                selectedNotes.clear();
                selectedNotes.insert(hitIndex);
            }
            primarySelectedNote = hitIndex;
        }

        auto& n = noteEvents.getReference(hitIndex);
        noteOrigStart = n.startBeat;
        noteOrigNote = n.noteNumber;
        noteOrigLength = n.lengthBeats;
        dragStartBeat = xToBeat(mx);
        dragStartNote = yToNote(my);

        if (isOnNoteRightEdge(mx, n))
            dragMode = ResizeNote;
        else
            dragMode = MoveNote;

        // Store original state for all selected notes
        selectedOrigStates.clear();
        for (int idx : selectedNotes)
        {
            auto& sn = noteEvents.getReference(idx);
            selectedOrigStates[idx] = { sn.startBeat, sn.noteNumber, sn.lengthBeats, sn.velocity };
        }
    }
    else
    {
        // Click empty space
        if (e.mods.isShiftDown())
        {
            // Shift+click empty = create new note
            double beat = xToBeat(mx);
            int note = yToNote(my);

            if (snapEnabled)
                beat = std::floor(beat / gridResolution) * gridResolution;
            if (beat < 0.0) beat = 0.0;

            NoteEvent newNote;
            newNote.noteNumber = juce::jlimit(0, 127, note);
            newNote.startBeat = beat;
            newNote.lengthBeats = 0.25;

            noteEvents.add(newNote);
            int newIdx = noteEvents.size() - 1;
            selectedNotes.clear();
            selectedNotes.insert(newIdx);
            primarySelectedNote = newIdx;
            dragMode = ResizeNote;
            noteOrigLength = 0.25;
            dragStartBeat = beat;

            applyNoteListToClip();
        }
        else
        {
            // Start marquee selection
            selectedNotes.clear();
            primarySelectedNote = -1;
            marqueeStart = e.position;
            marqueeRect = {};
            dragMode = MarqueeSelect;
        }
    }

    repaint();
}

void PianoRollComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (touchScrolling && e.source.getIndex() > 0) {
        float dx = e.position.x - lastTouchX;
        float dy = e.position.y - lastTouchY;
        scrollX -= dx / pixelsPerBeat;
        scrollY -= static_cast<int>(dy / noteHeight);
        scrollX = juce::jmax(0.0, scrollX);
        scrollY = juce::jlimit(0, 127 - 10, scrollY);
        lastTouchX = e.position.x;
        lastTouchY = e.position.y;
        repaint();
        return;
    }

    float mx = static_cast<float>(e.x);
    float my = static_cast<float>(e.y);

    if (dragMode == MarqueeSelect)
    {
        float x1 = juce::jmin(marqueeStart.x, mx);
        float y1 = juce::jmin(marqueeStart.y, my);
        float x2 = juce::jmax(marqueeStart.x, mx);
        float y2 = juce::jmax(marqueeStart.y, my);
        marqueeRect = { x1, y1, x2 - x1, y2 - y1 };

        // Update selection to all notes intersecting the marquee
        selectedNotes.clear();
        for (int i = 0; i < noteEvents.size(); ++i)
        {
            auto rect = getNoteRect(noteEvents[i]);
            if (marqueeRect.intersects(rect))
                selectedNotes.insert(i);
        }
        repaint();
        return;
    }

    if (dragMode == VelocityLaneDrag)
    {
        // Drag velocity in the lane: y position maps to velocity
        float velTop = static_cast<float>(velLaneTop());
        float velBot = static_cast<float>(getHeight());
        float frac = 1.0f - juce::jlimit(0.0f, 1.0f, (my - velTop) / (velBot - velTop));
        int newVel = juce::jlimit(1, 127, static_cast<int>(frac * 127.0f));

        if (primarySelectedNote >= 0 && selectedOrigStates.count(primarySelectedNote))
        {
            int origVel = selectedOrigStates[primarySelectedNote].velocity;
            int delta = newVel - origVel;

            for (int idx : selectedNotes)
            {
                if (idx >= 0 && idx < noteEvents.size() && selectedOrigStates.count(idx))
                {
                    int v = juce::jlimit(1, 127, selectedOrigStates[idx].velocity + delta);
                    noteEvents.getReference(idx).velocity = v;
                }
            }
        }
        repaint();
        return;
    }

    if (primarySelectedNote < 0 || primarySelectedNote >= noteEvents.size()) return;

    if (dragMode == MoveNote)
    {
        double beatDelta = xToBeat(mx) - dragStartBeat;
        int noteDelta = yToNote(my) - dragStartNote;

        // Compute snapped delta from the primary note
        double newPrimaryStart = noteOrigStart + beatDelta;
        if (snapEnabled)
            newPrimaryStart = std::floor(newPrimaryStart / gridResolution + 0.5) * gridResolution;
        if (newPrimaryStart < 0.0) newPrimaryStart = 0.0;
        double snappedBeatDelta = newPrimaryStart - noteOrigStart;

        // Move all selected notes by the same delta
        for (int idx : selectedNotes)
        {
            if (idx >= 0 && idx < noteEvents.size() && selectedOrigStates.count(idx))
            {
                auto& sn = noteEvents.getReference(idx);
                double ns = selectedOrigStates[idx].startBeat + snappedBeatDelta;
                if (ns < 0.0) ns = 0.0;
                sn.startBeat = ns;
                sn.noteNumber = juce::jlimit(0, 127, selectedOrigStates[idx].noteNumber + noteDelta);
            }
        }
    }
    else if (dragMode == ResizeNote)
    {
        auto& n = noteEvents.getReference(primarySelectedNote);
        double endBeat = xToBeat(mx);
        if (snapEnabled)
            endBeat = std::floor(endBeat / gridResolution + 0.5) * gridResolution;
        double newLength = endBeat - n.startBeat;
        if (newLength < 0.0625) newLength = 0.0625;
        n.lengthBeats = newLength;
    }

    repaint();
}

void PianoRollComponent::mouseUp(const juce::MouseEvent& e)
{
    if (e.source.getIndex() > 0) {
        touchScrolling = false;
        return;
    }

    if (e.position.x < pianoKeyWidth) {
        int note = yToNote(e.position.y);
        if (note >= 0 && note <= 127 && onNotePreview)
            onNotePreview(note, false);
        return;
    }

    if (dragMode == MarqueeSelect)
    {
        marqueeRect = {};
        dragMode = None;
        repaint();
        return;
    }

    if (dragMode != None)
    {
        applyNoteListToClip();
        dragMode = None;
    }
}

void PianoRollComponent::mouseMove(const juce::MouseEvent& e)
{
    float mx = static_cast<float>(e.x);
    float my = static_cast<float>(e.y);

    int hit = hitTestNote(mx, my);
    if (hit >= 0 && hit < static_cast<int>(noteEvents.size()) && isOnNoteRightEdge(mx, noteEvents[hit]))
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    else if (hit >= 0)
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    else
        setMouseCursor(juce::MouseCursor::NormalCursor);
}

void PianoRollComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCtrlDown())
    {
        // Zoom horizontal
        double zoomFactor = 1.0 + w.deltaY * 0.3;
        pixelsPerBeat = juce::jlimit(20.0, 400.0, pixelsPerBeat * zoomFactor);
    }
    else if (e.mods.isShiftDown())
    {
        // Scroll horizontal
        scrollX -= w.deltaY * 2.0;
        if (scrollX < 0.0) scrollX = 0.0;
    }
    else
    {
        // Scroll vertical (notes)
        scrollY -= static_cast<int>(w.deltaY * 5.0f);
        scrollY = juce::jlimit(MIN_NOTE, MAX_NOTE - 10, scrollY);
    }

    repaint();
}

void PianoRollComponent::mouseMagnify(const juce::MouseEvent& /*e*/, float scaleFactor)
{
    pixelsPerBeat *= scaleFactor;
    pixelsPerBeat = juce::jlimit(20.0, 400.0, pixelsPerBeat);
    repaint();
}

// ── Drawing ──────────────────────────────────────────────────────────────────

void PianoRollComponent::paint(juce::Graphics& g)
{
    auto* tc = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel());
    g.fillAll(juce::Colour(tc ? tc->getTheme().timelineBg : 0xff1a1a1a));
    drawToolbar(g);
    drawPianoKeys(g);
    drawGrid(g);
    drawNotes(g);
    drawVelocityLane(g);
    drawMarquee(g);
    drawPlayhead(g);
}

void PianoRollComponent::timerCallback()
{
    if (engine.isPlaying())
    {
        // Auto-scroll to follow playhead
        if (followPlayhead && clip.lengthInBeats > 0.0)
        {
            double clipPos = std::fmod(engine.getPositionInBeats() - clip.timelinePosition, clip.lengthInBeats);
            if (clipPos < 0.0) clipPos += clip.lengthInBeats;
            float playheadX = beatToX(clipPos);
            float viewRight = static_cast<float>(getWidth());

            // If playhead is past 75% of the view, scroll to keep it visible
            if (playheadX > viewRight * 0.75f || playheadX < static_cast<float>(pianoKeyWidth))
            {
                scrollX = clipPos - (viewRight * 0.25 - pianoKeyWidth) / pixelsPerBeat;
                if (scrollX < 0.0) scrollX = 0.0;
            }
        }

        repaint();
    }
}

void PianoRollComponent::drawPlayhead(juce::Graphics& g)
{
    if (!engine.isPlaying() || clip.lengthInBeats <= 0.0) return;

    auto* tc = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel());

    double clipPos = std::fmod(engine.getPositionInBeats() - clip.timelinePosition, clip.lengthInBeats);
    if (clipPos < 0.0) clipPos += clip.lengthInBeats;
    float x = beatToX(clipPos);

    if (x < pianoKeyWidth || x > getWidth()) return;

    // Playhead line
    g.setColour(juce::Colour(tc ? tc->getTheme().playhead : 0xddffcc00));
    g.drawVerticalLine(static_cast<int>(x), static_cast<float>(toolbarHeight), static_cast<float>(getHeight()));

    // Slightly wider glow
    g.setColour(juce::Colour(tc ? tc->getTheme().playheadGlow : 0x33ffcc00));
    g.fillRect(x - 1.0f, static_cast<float>(toolbarHeight), 3.0f, static_cast<float>(getHeight() - toolbarHeight));
}

void PianoRollComponent::resized()
{
    int x = pianoKeyWidth + 4;
    gridSelector.setBounds(x, 0, 70, toolbarHeight);
    x += 74;
    snapButton.setBounds(x, 0, 44, toolbarHeight);
}

bool PianoRollComponent::isBlackKey(int note)
{
    int n = note % 12;
    return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
}

juce::String PianoRollComponent::noteName(int note)
{
    static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    return juce::String(names[note % 12]) + juce::String(note / 12 - 1);
}

void PianoRollComponent::drawPianoKeys(juce::Graphics& g)
{
    auto* tc = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel());
    int topNote = scrollY + visibleNotes() - 1;

    for (int note = scrollY; note <= topNote && note <= MAX_NOTE; ++note)
    {
        float y = noteToY(note);
        bool black = isBlackKey(note);

        g.setColour(black ? juce::Colour(tc ? tc->getTheme().bodyDark : 0xff333333)
                          : juce::Colour(tc ? tc->getTheme().textPrimary : 0xffcccccc));
        g.fillRect(0.0f, y, static_cast<float>(pianoKeyWidth), static_cast<float>(noteHeight - 1));

        g.setColour(juce::Colour(tc ? tc->getTheme().timelineGridMajor : 0xff555555));
        g.drawLine(0, y + noteHeight - 1, static_cast<float>(pianoKeyWidth), y + noteHeight - 1);

        // Label C notes
        if (note % 12 == 0)
        {
            g.setColour(black ? juce::Colours::white : juce::Colours::black);
            g.setFont(10.0f);
            g.drawText(noteName(note), 2, static_cast<int>(y), pianoKeyWidth - 4, noteHeight, juce::Justification::centredLeft);
        }
    }
}

void PianoRollComponent::drawGrid(juce::Graphics& g)
{
    auto* tc = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel());
    int w = getWidth();
    int h = velLaneTop();
    int topNote = scrollY + visibleNotes() - 1;

    // Horizontal lines (note lanes)
    for (int note = scrollY; note <= topNote && note <= MAX_NOTE; ++note)
    {
        float y = noteToY(note);
        bool black = isBlackKey(note);

        // Alternating lane colors
        g.setColour(black ? juce::Colour(tc ? tc->getTheme().timelineGridFaint : 0xff1e1e1e)
                          : juce::Colour(tc ? tc->getTheme().timelineGridMinor : 0xff242424));
        g.fillRect(static_cast<float>(pianoKeyWidth), y, static_cast<float>(w - pianoKeyWidth), static_cast<float>(noteHeight));

        g.setColour(juce::Colour(tc ? tc->getTheme().timelineGridBeat : 0xff2a2a2a));
        g.drawHorizontalLine(static_cast<int>(y + noteHeight - 1), static_cast<float>(pianoKeyWidth), static_cast<float>(w));
    }

    // Vertical lines (beats)
    double firstBeat = std::floor(scrollX);
    double lastBeat = scrollX + (w - pianoKeyWidth) / pixelsPerBeat;

    for (double beat = firstBeat; beat <= lastBeat; beat += 0.25)
    {
        float x = beatToX(beat);
        if (x < pianoKeyWidth) continue;

        double wholeBeat = std::fmod(beat, 1.0);
        bool isBeat = std::abs(wholeBeat) < 0.001 || std::abs(wholeBeat - 1.0) < 0.001;
        bool isBar = std::abs(std::fmod(beat, 4.0)) < 0.001;

        if (isBar)
            g.setColour(juce::Colour(tc ? tc->getTheme().timelineGridMajor : 0xff555555));
        else if (isBeat)
            g.setColour(juce::Colour(tc ? tc->getTheme().timelineGridBeat : 0xff3a3a3a));
        else
            g.setColour(juce::Colour(tc ? tc->getTheme().timelineGridMinor : 0xff2d2d2d));

        g.drawVerticalLine(static_cast<int>(x), static_cast<float>(toolbarHeight), static_cast<float>(h));

        // Beat numbers at top of grid area
        if (isBeat)
        {
            g.setColour(juce::Colour(tc ? tc->getTheme().textSecondary : 0xff888888));
            g.setFont(10.0f);
            g.drawText(juce::String(static_cast<int>(beat) + 1), static_cast<int>(x) + 2, toolbarHeight, 30, 12, juce::Justification::left);
        }
    }
}

void PianoRollComponent::drawNotes(juce::Graphics& g)
{
    auto* tc = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel());
    float vlt = static_cast<float>(velLaneTop());

    for (int i = 0; i < noteEvents.size(); ++i)
    {
        auto rect = getNoteRect(noteEvents[i]);

        // Skip if out of view (clip to above velocity lane)
        if (rect.getRight() < pianoKeyWidth || rect.getX() > getWidth()) continue;
        if (rect.getBottom() < 0 || rect.getY() > vlt) continue;

        // Clip note rect to velocity lane boundary
        if (rect.getBottom() > vlt)
            rect = rect.withBottom(vlt);

        // Note color
        bool isSelected = selectedNotes.find(i) != selectedNotes.end();
        if (isSelected)
            g.setColour(juce::Colour(tc ? tc->getTheme().amber : 0xffff9944)); // selected
        else
            g.setColour(juce::Colour(tc ? tc->getTheme().lcdText : 0xff5588cc)); // unselected

        g.fillRoundedRectangle(rect, 2.0f);

        // Border
        g.setColour(juce::Colour(tc ? tc->getTheme().borderLight : 0xff88aadd));
        g.drawRoundedRectangle(rect, 2.0f, 1.0f);

        // Velocity bar at bottom of note
        float velFrac = static_cast<float>(noteEvents[i].velocity) / 127.0f;
        auto velBar = rect;
        velBar = velBar.removeFromBottom(3.0f);
        velBar = velBar.withWidth(velBar.getWidth() * velFrac);
        g.setColour(juce::Colour(tc ? tc->getTheme().red : 0xffff6644).withAlpha(0.7f));
        g.fillRect(velBar);

        // Resize handle hint (right edge)
        g.setColour(juce::Colour(tc ? tc->getTheme().textBright : 0xffffffff).withAlpha(0.27f));
        g.fillRect(rect.getRight() - 4, rect.getY() + 2, 2.0f, rect.getHeight() - 4);
    }
}

void PianoRollComponent::drawVelocityLane(juce::Graphics& g)
{
    auto* tc = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel());
    int vlt = velLaneTop();

    // Separator line
    g.setColour(juce::Colour(tc ? tc->getTheme().border : 0xff444444));
    g.drawHorizontalLine(vlt, static_cast<float>(pianoKeyWidth), static_cast<float>(getWidth()));

    // Background
    g.setColour(juce::Colour(tc ? tc->getTheme().bodyDark : 0xff181818));
    g.fillRect(pianoKeyWidth, vlt + 1, getWidth() - pianoKeyWidth, velLaneHeight - 1);

    // Velocity bars
    for (int i = 0; i < noteEvents.size(); ++i)
    {
        auto& ne = noteEvents.getReference(i);
        float x = beatToX(ne.startBeat);
        float w = static_cast<float>(ne.lengthBeats * pixelsPerBeat);
        if (x + w < pianoKeyWidth || x > getWidth()) continue;

        float barH = (static_cast<float>(ne.velocity) / 127.0f) * static_cast<float>(velLaneHeight - 4);
        float barW = juce::jmax(2.0f, w - 1.0f);

        bool isSelected = selectedNotes.find(i) != selectedNotes.end();
        if (isSelected)
            g.setColour(juce::Colour(tc ? tc->getTheme().amber : 0xffff9944).withAlpha(0.9f));
        else
            g.setColour(juce::Colour(tc ? tc->getTheme().amber : 0xffff9944).withAlpha(0.5f));

        g.fillRect(x, static_cast<float>(vlt + velLaneHeight) - barH - 2.0f, barW, barH);
    }
}

void PianoRollComponent::drawMarquee(juce::Graphics& g)
{
    if (dragMode != MarqueeSelect || marqueeRect.isEmpty()) return;

    auto* tc = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel());
    g.setColour(juce::Colour(tc ? tc->getTheme().amber : 0xffff9944).withAlpha(0.15f));
    g.fillRect(marqueeRect);
    g.setColour(juce::Colour(tc ? tc->getTheme().amber : 0xffff9944).withAlpha(0.6f));
    g.drawRect(marqueeRect, 1.0f);
}

void PianoRollComponent::drawToolbar(juce::Graphics& g)
{
    auto* tc = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel());

    // Toolbar background
    g.setColour(juce::Colour(tc ? tc->getTheme().bodyDark : 0xff1a1a1a));
    g.fillRect(0, 0, getWidth(), toolbarHeight);

    // Separator line
    g.setColour(juce::Colour(tc ? tc->getTheme().timelineGridMajor : 0xff555555));
    g.drawHorizontalLine(toolbarHeight - 1, 0.0f, static_cast<float>(getWidth()));

    // Label
    g.setColour(juce::Colour(tc ? tc->getTheme().textSecondary : 0xff888888));
    g.setFont(10.0f);
    g.drawText("Grid:", 2, 0, pianoKeyWidth - 2, toolbarHeight, juce::Justification::centredRight);
}

bool PianoRollComponent::keyPressed(const juce::KeyPress& key)
{
    auto keyCode = key.getKeyCode();
    bool shift = key.getModifiers().isShiftDown();
    bool cmdOrCtrl = key.getModifiers().isCommandDown();

    // Delete/Backspace: delete selected notes
    if (keyCode == juce::KeyPress::deleteKey || keyCode == juce::KeyPress::backspaceKey)
    {
        if (!selectedNotes.empty())
        {
            std::vector<int> toRemove(selectedNotes.begin(), selectedNotes.end());
            std::sort(toRemove.rbegin(), toRemove.rend());
            for (int idx : toRemove)
                noteEvents.remove(idx);
            selectedNotes.clear();
            primarySelectedNote = -1;
            applyNoteListToClip();
            repaint();
        }
        return true;
    }

    // Arrow Up: move selected notes up by 1 semitone (Shift: 12 semitones / 1 octave)
    if (keyCode == juce::KeyPress::upKey)
    {
        int semitones = shift ? 12 : 1;
        for (int idx : selectedNotes)
        {
            auto& n = noteEvents.getReference(idx);
            n.noteNumber = juce::jlimit(0, 127, n.noteNumber + semitones);
        }
        if (!selectedNotes.empty())
        {
            applyNoteListToClip();
            repaint();
        }
        return true;
    }

    // Arrow Down: move selected notes down by 1 semitone (Shift: 12 semitones / 1 octave)
    if (keyCode == juce::KeyPress::downKey)
    {
        int semitones = shift ? 12 : 1;
        for (int idx : selectedNotes)
        {
            auto& n = noteEvents.getReference(idx);
            n.noteNumber = juce::jlimit(0, 127, n.noteNumber - semitones);
        }
        if (!selectedNotes.empty())
        {
            applyNoteListToClip();
            repaint();
        }
        return true;
    }

    // Arrow Left: move selected notes left by grid resolution (Shift: by 1 beat)
    if (keyCode == juce::KeyPress::leftKey)
    {
        double amount = shift ? 1.0 : gridResolution;
        for (int idx : selectedNotes)
        {
            auto& n = noteEvents.getReference(idx);
            n.startBeat = juce::jmax(0.0, n.startBeat - amount);
        }
        if (!selectedNotes.empty())
        {
            applyNoteListToClip();
            repaint();
        }
        return true;
    }

    // Arrow Right: move selected notes right by grid resolution (Shift: by 1 beat)
    if (keyCode == juce::KeyPress::rightKey)
    {
        double amount = shift ? 1.0 : gridResolution;
        for (int idx : selectedNotes)
        {
            auto& n = noteEvents.getReference(idx);
            n.startBeat += amount;
        }
        if (!selectedNotes.empty())
        {
            applyNoteListToClip();
            repaint();
        }
        return true;
    }

    // Cmd/Ctrl+A: select all notes
    if (cmdOrCtrl && (keyCode == 'A' || keyCode == 'a'))
    {
        selectedNotes.clear();
        for (int i = 0; i < noteEvents.size(); ++i)
            selectedNotes.insert(i);
        repaint();
        return true;
    }

    // Cmd/Ctrl+D: duplicate selected notes (place after the latest selected note)
    if (cmdOrCtrl && (keyCode == 'D' || keyCode == 'd'))
    {
        if (!selectedNotes.empty())
        {
            // Find the latest end position and earliest start among selected notes
            double latestEnd = 0.0;
            double earliestStart = std::numeric_limits<double>::max();
            for (int idx : selectedNotes)
            {
                const auto& n = noteEvents.getReference(idx);
                double end = n.startBeat + n.lengthBeats;
                if (end > latestEnd) latestEnd = end;
                if (n.startBeat < earliestStart) earliestStart = n.startBeat;
            }

            // Snap the copy offset to next grid position after latest end
            double offset = snapEnabled
                ? std::ceil(latestEnd / gridResolution) * gridResolution
                : latestEnd;

            // Duplicate and select the new notes
            std::set<int> newSelection;
            std::vector<NoteEvent> copies;
            for (int idx : selectedNotes)
            {
                NoteEvent copy = noteEvents.getReference(idx);
                copy.startBeat = offset + (copy.startBeat - earliestStart);
                copies.push_back(copy);
            }
            for (auto& copy : copies)
            {
                noteEvents.add(copy);
                newSelection.insert(noteEvents.size() - 1);
            }
            selectedNotes = newSelection;
            primarySelectedNote = -1;
            applyNoteListToClip();
            repaint();
        }
        return true;
    }

    // +/= key: zoom in horizontally
    if (keyCode == '+' || keyCode == '=')
    {
        pixelsPerBeat = juce::jlimit(20.0, 400.0, pixelsPerBeat * 1.2);
        repaint();
        return true;
    }

    // - key: zoom out horizontally
    if (keyCode == '-')
    {
        pixelsPerBeat = juce::jlimit(20.0, 400.0, pixelsPerBeat / 1.2);
        repaint();
        return true;
    }

    // Escape: deselect all
    if (keyCode == juce::KeyPress::escapeKey)
    {
        selectedNotes.clear();
        primarySelectedNote = -1;
        repaint();
        return true;
    }

    return false;
}
