#include "TimelineComponent.h"
#include "DawLookAndFeel.h"

TimelineComponent::TimelineComponent(PluginHost& host)
    : pluginHost(host)
{
    setWantsKeyboardFocus(true);
    // Pro devices get 120Hz for butter-smooth playhead and scrolling
    startTimerHz(60);
}

// ── Coordinate conversion ────────────────────────────────────────────────────

float TimelineComponent::beatToX(double beat) const
{
    return trackLabelWidth + static_cast<float>((beat - scrollX) * pixelsPerBeat);
}

double TimelineComponent::xToBeat(float x) const
{
    return scrollX + (x - trackLabelWidth) / pixelsPerBeat;
}

int TimelineComponent::yToTrack(float y) const
{
    return static_cast<int>((y - headerHeight + scrollY) / trackHeight);
}

// ── Hit testing ──────────────────────────────────────────────────────────────

juce::Rectangle<float> TimelineComponent::getClipRect(int trackIndex, int slotIndex) const
{
    auto* cp = pluginHost.getTrack(trackIndex).clipPlayer;
    if (cp == nullptr) return {};

    auto& slot = cp->getSlot(slotIndex);

    double pos = 0.0, len = 0.0;
    if (slot.clip != nullptr)
    {
        pos = slot.clip->timelinePosition;
        len = slot.clip->lengthInBeats;
    }
    else if (slot.audioClip != nullptr)
    {
        pos = slot.audioClip->timelinePosition;
        len = slot.audioClip->lengthInBeats;
    }
    else
    {
        return {};
    }

    float x1 = beatToX(pos);
    float x2 = beatToX(pos + len);
    float y = static_cast<float>(headerHeight + trackIndex * trackHeight - scrollY + 2);
    float h = static_cast<float>(trackHeight - 4);

    return { x1, y, x2 - x1, h };
}

TimelineComponent::ClipRef TimelineComponent::hitTestClip(float x, float y) const
{
    int trackIdx = yToTrack(y);
    if (trackIdx < 0 || trackIdx >= PluginHost::NUM_TRACKS) return {};

    auto* cp = pluginHost.getTrack(trackIdx).clipPlayer;
    if (cp == nullptr) return {};

    for (int s = 0; s < cp->getNumSlots(); ++s)
    {
        auto rect = getClipRect(trackIdx, s);
        if (!rect.isEmpty() && rect.contains(x, y))
            return { trackIdx, s };
    }
    return {};
}

bool TimelineComponent::isOnClipLeftEdge(float x, const juce::Rectangle<float>& rect) const
{
    return x < rect.getX() + 6.0f;
}

bool TimelineComponent::isOnClipRightEdge(float x, const juce::Rectangle<float>& rect) const
{
    return x > rect.getRight() - 6.0f;
}

ClipSlot* TimelineComponent::getSlot(const ClipRef& ref) const
{
    if (!ref.isValid()) return nullptr;
    auto* cp = pluginHost.getTrack(ref.trackIndex).clipPlayer;
    if (cp == nullptr) return nullptr;
    return &cp->getSlot(ref.slotIndex);
}

MidiClip* TimelineComponent::getClip(const ClipRef& ref) const
{
    auto* slot = getSlot(ref);
    if (slot == nullptr) return nullptr;
    return slot->clip.get();
}

// ── Timer ────────────────────────────────────────────────────────────────────

void TimelineComponent::timerCallback()
{
    auto& engine = pluginHost.getEngine();
    double pos = engine.getPositionInBeats();
    float playheadX = beatToX(pos);
    float viewWidth = static_cast<float>(getWidth());

    // Check if user scroll override has expired
    if (userScrollActive)
    {
        double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        if (now > userScrollExpireTime)
            userScrollActive = false;
    }

    if (engine.isPlaying() && !userScrollActive)
    {
        // Auto-scroll to follow playhead during playback
        if (playheadX > viewWidth * 0.8f)
            scrollX = pos - (viewWidth * 0.2 - trackLabelWidth) / pixelsPerBeat;
        else if (playheadX < static_cast<float>(trackLabelWidth))
        {
            scrollX = pos - 1.0;
            if (scrollX < 0.0) scrollX = 0.0;
        }
    }
    else if (!engine.isPlaying() && !userScrollActive)
    {
        // When stopped, check if playhead moved (e.g. reset to 0) and follow it
        if (playheadX < static_cast<float>(trackLabelWidth) || playheadX > viewWidth)
        {
            scrollX = pos;
            if (scrollX < 0.0) scrollX = 0.0;
        }
    }

    // Long press on clip — show context menu
    if (longPressTrack >= 0 && !longPressTriggered && mouseDownTime > 0)
    {
        juce::int64 holdTime = juce::Time::currentTimeMillis() - mouseDownTime;
        if (holdTime >= longPressMs)
        {
            longPressTriggered = true;
            clipClickPending = false;  // cancel deferred drag
            auto hit = hitTestClip(longPressPos.x, longPressPos.y);
            if (hit.isValid())
            {
                selectedClip = hit;
                showClipContextMenu(hit);
            }
        }
    }

    // Only repaint when playing or scroll changed
    if (engine.isPlaying() || scrollX != lastPaintedScrollX)
        repaint();
    lastPaintedScrollX = scrollX;
}

// ── Mouse handling ───────────────────────────────────────────────────────────

void TimelineComponent::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();

    // Track first finger position for pinch distance calculation
    if (e.source.getIndex() == 0)
        firstFingerPos = e.position;

    // Second finger = start touch scroll + pinch zoom
    if (e.source.getIndex() > 0)
    {
        touchScrolling = true;
        touchScrollStart = e.position;
        touchScrollStartX = scrollX;
        touchScrollStartY = scrollY;
        pinchStartPixelsPerBeat = pixelsPerBeat;
        pinchStartDistance = firstFingerPos.getDistanceFrom(e.position);
        if (pinchStartDistance < 1.0f) pinchStartDistance = 1.0f;
        return;
    }

    float mx = static_cast<float>(e.x);
    float my = static_cast<float>(e.y);

    // ── Loop Set Mode: two-tap workflow ──
    if (loopSetMode && e.x >= trackLabelWidth)
    {
        double beat = snapToGrid(xToBeat(mx));
        if (beat < 0.0) beat = 0.0;

        if (loopSetTapCount == 0)
        {
            loopSetStartBeat = beat;
            loopSetTapCount = 1;
            if (onLoopSetProgress) onLoopSetProgress(1);
            repaint();
        }
        else
        {
            double ls = juce::jmin(loopSetStartBeat, beat);
            double le = juce::jmax(loopSetStartBeat, beat);
            if (le - ls > 0.1)
                pluginHost.getEngine().setLoopRegion(ls, le);
            loopSetMode = false;
            loopSetTapCount = 0;
            if (onLoopSetProgress) onLoopSetProgress(0);
            repaint();
        }
        return;
    }

    // ── Check for loop handle drag — header only, not clip area ──
    if (e.y < headerHeight && e.x >= trackLabelWidth)
    {
        auto& engine = pluginHost.getEngine();
        if (engine.hasLoopRegion())
        {
            float lx = beatToX(engine.getLoopStart());
            float rx = beatToX(engine.getLoopEnd());

            // When handles are very close, prefer the nearest one
            float distL = std::abs(mx - lx);
            float distR = std::abs(mx - rx);

            if (distL < loopHandleHitZone && distL <= distR)
            {
                loopHandleDrag = DragLoopStart;
                repaint();
                return;
            }
            if (distR < loopHandleHitZone)
            {
                loopHandleDrag = DragLoopEnd;
                repaint();
                return;
            }
        }
    }

    // Click on header — start loop drag or jump playhead
    if (e.y < headerHeight && e.x >= trackLabelWidth)
    {
        double beat = snapToGrid(xToBeat(mx));
        if (beat < 0.0) beat = 0.0;

        // Begin loop region drag
        draggingLoop = true;
        loopDragStartBeat = beat;
        pluginHost.getEngine().setPosition(beat);
        repaint();
        return;
    }

    // Handle clicks in the track control area
    if (e.x < trackLabelWidth)
    {
        int trackIdx = yToTrack(my);
        if (trackIdx >= 0 && trackIdx < PluginHost::NUM_TRACKS)
        {
            // Check if pressing the track select button — use long press for lock-arm
            auto selRect = getSelectButtonRect(trackIdx);
            if (selRect.toFloat().contains(mx, my))
            {
                longPressTrack = trackIdx;
                longPressPos = { mx, my };
                mouseDownTime = juce::Time::currentTimeMillis();
                longPressTriggered = false;
            }
            else
            {
                handleTrackControlClick(trackIdx, mx, my);
            }
        }
        return;
    }
    // Check for automation point drag
    auto autoHit = hitTestAutoPoint(mx, my);
    if (autoHit.isValid())
    {
        dragAutoPoint = autoHit;
        draggingAutoPoint = true;
        if (onBeforeEdit) onBeforeEdit();
        repaint();
        return;
    }

    auto hit = hitTestClip(mx, my);

    if (e.mods.isRightButtonDown() && hit.isValid())
    {
        // Right-click → open piano roll
        auto* clip = getClip(hit);
        if (clip != nullptr)
        {
            new PianoRollWindow("Piano Roll - Track " + juce::String(hit.trackIndex + 1)
                + " Slot " + juce::String(hit.slotIndex + 1), *clip,
                pluginHost.getEngine());
        }
        return;
    }

    if (hit.isValid())
    {
        selectedClip = hit;
        // Defer drag setup to allow long press detection on clips
        clipClickPending = true;
        pendingClipHit = hit;
        pendingClickX = mx;
        pendingClickY = my;
        longPressPos = { mx, my };
        mouseDownTime = juce::Time::currentTimeMillis();
        longPressTriggered = false;
        longPressTrack = hit.trackIndex;  // reuse long press tracking
        dragMode = NoDrag;
    }
    else
    {
        selectedClip = {};
        // Start single-finger pan on empty space
        panning = true;
        panStart = e.position;
        panStartScrollX = scrollX;
        panStartScrollY = scrollY;
    }

    repaint();
}

void TimelineComponent::mouseDrag(const juce::MouseEvent& e)
{
    // Track first finger movement
    if (e.source.getIndex() == 0)
        firstFingerPos = e.position;

    // Two-finger touch scroll + pinch zoom (skip if dragging a loop handle)
    if (touchScrolling && e.source.getIndex() > 0 && loopHandleDrag == NoHandle)
    {
        float dx = e.position.x - touchScrollStart.x;
        float dy = e.position.y - touchScrollStart.y;

        // Pinch zoom — compare current finger distance to start distance
        float currentDistance = firstFingerPos.getDistanceFrom(e.position);
        if (pinchStartDistance > 1.0f && currentDistance > 1.0f)
        {
            float pinchRatio = currentDistance / pinchStartDistance;
            double newPPB = juce::jlimit(10.0, 300.0, pinchStartPixelsPerBeat * static_cast<double>(pinchRatio));

            // Zoom centered between the two fingers
            float centerX = (firstFingerPos.x + e.position.x) * 0.5f;
            double beatAtCenter = touchScrollStartX + (centerX - trackLabelWidth) / pinchStartPixelsPerBeat;
            pixelsPerBeat = newPPB;
            scrollX = beatAtCenter - (centerX - trackLabelWidth) / pixelsPerBeat;
            if (scrollX < 0.0) scrollX = 0.0;
        }
        else
        {
            scrollX = touchScrollStartX - dx / pixelsPerBeat;
            if (scrollX < 0.0) scrollX = 0.0;
        }

        // Suppress auto-follow
        userScrollActive = true;
        userScrollExpireTime = juce::Time::getMillisecondCounterHiRes() * 0.001 + 3.0;

        int totalContent = PluginHost::NUM_TRACKS * trackHeight;
        int maxScroll = juce::jmax(0, totalContent - (getHeight() - headerHeight));
        scrollY = juce::jlimit(0, maxScroll, touchScrollStartY - static_cast<int>(dy));

        repaint();
        return;
    }

    // Automation point drag
    if (draggingAutoPoint && dragAutoPoint.isValid())
    {
        float mx = static_cast<float>(e.x);
        float my = static_cast<float>(e.y);
        auto& track = pluginHost.getTrack(dragAutoPoint.trackIndex);
        if (dragAutoPoint.laneIndex < track.automationLanes.size())
        {
            auto* lane = track.automationLanes[dragAutoPoint.laneIndex];
            const juce::SpinLock::ScopedLockType lock(track.automationLock);
            if (dragAutoPoint.pointIndex < lane->points.size())
            {
                auto& pt = lane->points.getReference(dragAutoPoint.pointIndex);
                pt.beat = juce::jmax(0.0, snapToGrid(xToBeat(mx)));
                pt.value = yToAutoValue(dragAutoPoint.trackIndex, my);
            }
            std::sort(lane->points.begin(), lane->points.end(),
                [](const AutomationPoint& a, const AutomationPoint& b) { return a.beat < b.beat; });
        }
        repaint();
        return;
    }

    // Loop handle drag — adjust existing loop start or end
    if (loopHandleDrag != NoHandle)
    {
        float mx = static_cast<float>(e.x);
        double beat = snapToGrid(xToBeat(mx));
        if (beat < 0.0) beat = 0.0;

        auto& engine = pluginHost.getEngine();
        double ls = engine.getLoopStart();
        double le = engine.getLoopEnd();

        if (loopHandleDrag == DragLoopStart)
        {
            ls = juce::jmin(beat, le - 0.25); // keep minimum 1/4 beat
        }
        else
        {
            le = juce::jmax(beat, ls + 0.25);
        }
        engine.setLoopRegion(ls, le);
        repaint();
        return;
    }

    // Loop region drag on header
    if (draggingLoop)
    {
        float mx = static_cast<float>(e.x);
        double beat = snapToGrid(xToBeat(mx));
        if (beat < 0.0) beat = 0.0;

        double ls = juce::jmin(loopDragStartBeat, beat);
        double le = juce::jmax(loopDragStartBeat, beat);
        if (le - ls > 0.1)  // minimum size of ~0.1 beats
            pluginHost.getEngine().setLoopRegion(ls, le);

        repaint();
        return;
    }

    // Single-finger pan on empty space
    if (panning && e.source.getIndex() == 0)
    {
        float dx = e.position.x - panStart.x;
        float dy = e.position.y - panStart.y;

        scrollX = panStartScrollX - dx / pixelsPerBeat;
        if (scrollX < 0.0) scrollX = 0.0;

        int totalContent = PluginHost::NUM_TRACKS * trackHeight;
        int maxScroll = juce::jmax(0, totalContent - (getHeight() - headerHeight));
        scrollY = juce::jlimit(0, maxScroll, panStartScrollY - static_cast<int>(dy));

        userScrollActive = true;
        userScrollExpireTime = juce::Time::getMillisecondCounterHiRes() * 0.001 + 3.0;
        repaint();
        return;
    }

    // Deferred drag setup for clip clicks — only start dragging if long press hasn't triggered
    if (clipClickPending && !longPressTriggered && pendingClipHit.isValid())
    {
        float dx = e.position.x - pendingClickX;
        float dy = e.position.y - pendingClickY;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist > 4.0f)  // movement threshold to start drag
        {
            clipClickPending = false;
            dragClip = pendingClipHit;
            auto* pendClip = getClip(pendingClipHit);
            auto* pendSlot = getSlot(pendingClipHit);
            if (pendClip != nullptr)
            {
                clipOrigPosition = pendClip->timelinePosition;
                clipOrigLength = pendClip->lengthInBeats;
            }
            else if (pendSlot != nullptr && pendSlot->audioClip != nullptr)
            {
                clipOrigPosition = pendSlot->audioClip->timelinePosition;
                clipOrigLength = pendSlot->audioClip->lengthInBeats;
            }
            else
            {
                return;
            }
            dragStartBeat = xToBeat(pendingClickX);
            dragStartTrack = yToTrack(pendingClickY);

            auto rect = getClipRect(pendingClipHit.trackIndex, pendingClipHit.slotIndex);

            if (onBeforeEdit) onBeforeEdit();

            if (isOnClipRightEdge(pendingClickX, rect))
                dragMode = ResizeClipRight;
            else if (isOnClipLeftEdge(pendingClickX, rect))
                dragMode = ResizeClipLeft;
            else
                dragMode = MoveClip;

            // Reset long press tracking since we're dragging now
            longPressTrack = -1;
            mouseDownTime = 0;
        }
    }

    if (dragMode == NoDrag || !dragClip.isValid()) return;

    float mx = static_cast<float>(e.x);
    float my = static_cast<float>(e.y);
    auto* clip = getClip(dragClip);
    auto* slot = getSlot(dragClip);
    auto* aClip = (slot != nullptr) ? slot->audioClip.get() : nullptr;
    if (clip == nullptr && aClip == nullptr) return;

    double currentBeat = xToBeat(mx);

    if (dragMode == MoveClip)
    {
        double beatDelta = currentBeat - dragStartBeat;
        double newPos = clipOrigPosition + beatDelta;
        // Snap to grid resolution
        newPos = snapToGrid(newPos);
        if (newPos < 0.0) newPos = 0.0;
        if (clip) clip->timelinePosition = newPos;
        if (aClip) aClip->timelinePosition = newPos;

        // Check if dragged to a different track
        int newTrack = yToTrack(my);
        if (newTrack >= 0 && newTrack < PluginHost::NUM_TRACKS && newTrack != dragClip.trackIndex)
        {
            // Move clip to different track — find an empty slot
            auto* srcCp = pluginHost.getTrack(dragClip.trackIndex).clipPlayer;
            auto* dstCp = pluginHost.getTrack(newTrack).clipPlayer;

            if (srcCp != nullptr && dstCp != nullptr)
            {
                int emptySlot = dstCp->findOrCreateEmptySlot();

                if (emptySlot >= 0)
                {
                    // Move the clip
                    auto& srcSlot = srcCp->getSlot(dragClip.slotIndex);
                    auto& dstSlot = dstCp->getSlot(emptySlot);

                    dstSlot.clip = std::move(srcSlot.clip);
                    dstSlot.audioClip = std::move(srcSlot.audioClip);
                    dstSlot.state.store(srcSlot.state.load());
                    srcSlot.state.store(ClipSlot::Empty);

                    dragClip.trackIndex = newTrack;
                    dragClip.slotIndex = emptySlot;
                    selectedClip = dragClip;
                }
            }
        }
    }
    else if (dragMode == ResizeClipRight)
    {
        double clipPos = clip ? clip->timelinePosition : aClip->timelinePosition;
        double newEnd = snapToGrid(currentBeat);
        double newLength = newEnd - clipPos;
        if (newLength < gridResolution) newLength = gridResolution;
        if (clip) clip->lengthInBeats = newLength;
        if (aClip) aClip->lengthInBeats = newLength;
    }
    else if (dragMode == ResizeClipLeft)
    {
        double newStart = snapToGrid(currentBeat);
        if (newStart < 0.0) newStart = 0.0;

        double origEnd = clipOrigPosition + clipOrigLength;
        double newLength = origEnd - newStart;
        if (newLength < 0.25) newLength = 0.25;

        // Shift MIDI events to compensate for the start position change
        if (clip)
        {
            double shift = clip->timelinePosition - newStart;
            if (std::abs(shift) > 0.001)
            {
                for (int i = 0; i < clip->events.getNumEvents(); ++i)
                {
                    auto* event = clip->events.getEventPointer(i);
                    event->message.setTimeStamp(event->message.getTimeStamp() + shift);
                }
            }
            clip->timelinePosition = newStart;
            clip->lengthInBeats = newLength;
            // Remove any events that ended up with negative timestamps
            for (int i = clip->events.getNumEvents() - 1; i >= 0; --i)
                if (clip->events.getEventPointer(i)->message.getTimeStamp() < 0.0)
                    clip->events.deleteEvent(i, true);
            clip->events.updateMatchedPairs();
        }
        if (aClip)
        {
            aClip->timelinePosition = newStart;
            aClip->lengthInBeats = newLength;
        }
    }

    repaint();
}

void TimelineComponent::mouseUp(const juce::MouseEvent& e)
{
    if (e.source.getIndex() > 0)
        touchScrolling = false;

    panning = false;
    draggingLoop = false;
    loopHandleDrag = NoHandle;
    draggingAutoPoint = false;
    dragAutoPoint = {};

    // Handle clip click pending — short tap selects, long press already handled in timerCallback
    if (clipClickPending)
    {
        clipClickPending = false;
        if (!longPressTriggered)
        {
            // Short tap on clip = just select it (already set in mouseDown)
            selectedClip = pendingClipHit;
        }
        longPressTrack = -1;
        mouseDownTime = 0;
        repaint();
        dragMode = NoDrag;
        dragClip = {};
        return;
    }

    // Handle ARM button release — distinguish tap vs long press
    if (longPressTrack >= 0)
    {
        auto& track = pluginHost.getTrack(longPressTrack);
        juce::int64 holdTime = juce::Time::currentTimeMillis() - mouseDownTime;

        if (holdTime >= longPressMs)
        {
            // Long press = toggle lock-arm
            if (track.clipPlayer != nullptr)
            {
                bool wasLocked = track.clipPlayer->armLocked.load();
                track.clipPlayer->armLocked.store(!wasLocked);
                track.clipPlayer->armed.store(!wasLocked);
            }
        }
        else
        {
            // Short tap = select track (auto-arms)
            pluginHost.setSelectedTrack(longPressTrack);
        }

        longPressTrack = -1;
        repaint();
    }

    // After moving/resizing a clip, set it to Playing so it produces sound
    if (dragMode != NoDrag && dragClip.isValid())
    {
        auto* slot = getSlot(dragClip);
        if (slot != nullptr && slot->hasContent())
            slot->state.store(ClipSlot::Playing);
    }

    dragMode = NoDrag;
    dragClip = {};
}

void TimelineComponent::mouseMove(const juce::MouseEvent& e)
{
    float mx = static_cast<float>(e.x);
    float my = static_cast<float>(e.y);
    auto hit = hitTestClip(mx, my);

    if (hit.isValid())
    {
        auto rect = getClipRect(hit.trackIndex, hit.slotIndex);
        if (isOnClipRightEdge(mx, rect) || isOnClipLeftEdge(mx, rect))
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        else
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    }
    else
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
}

void TimelineComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    // Double-click header = clear loop region
    if (e.y < headerHeight && e.x >= trackLabelWidth)
    {
        pluginHost.getEngine().clearLoopRegion();
        repaint();
        return;
    }

    if (e.x < trackLabelWidth) return;

    float mx = static_cast<float>(e.x);
    float my = static_cast<float>(e.y);
    auto hit = hitTestClip(mx, my);

    // Double-tap automation point → delete it
    auto autoHit = hitTestAutoPoint(mx, my);
    if (autoHit.isValid())
    {
        if (onBeforeEdit) onBeforeEdit();
        auto& track = pluginHost.getTrack(autoHit.trackIndex);
        if (autoHit.laneIndex < track.automationLanes.size())
        {
            const juce::SpinLock::ScopedLockType lock(track.automationLock);
            auto* lane = track.automationLanes[autoHit.laneIndex];
            if (autoHit.pointIndex < lane->points.size())
                lane->points.remove(autoHit.pointIndex);
            // Remove empty lanes
            if (lane->points.isEmpty())
                track.automationLanes.remove(autoHit.laneIndex);
        }
        repaint();
        return;
    }

    if (!hit.isValid())
    {
        // Double-click empty space → create new empty clip
        int trackIdx = yToTrack(my);
        double beatPos = xToBeat(mx);
        beatPos = std::floor(beatPos); // snap to beat
        if (beatPos < 0.0) beatPos = 0.0;

        if (trackIdx >= 0 && trackIdx < PluginHost::NUM_TRACKS)
            createEmptyClip(trackIdx, beatPos);
    }
    else
    {
        // Double-click clip → open piano roll
        auto* clip = getClip(hit);
        if (clip != nullptr)
        {
            new PianoRollWindow("Piano Roll - Track " + juce::String(hit.trackIndex + 1),
                *clip, pluginHost.getEngine());
        }
    }
}

void TimelineComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCtrlDown())
    {
        // Ctrl+wheel = zoom centered on mouse position
        double beatAtMouse = xToBeat(static_cast<float>(e.x));
        double zoomFactor = 1.0 + w.deltaY * 0.3;
        pixelsPerBeat = juce::jlimit(10.0, 300.0, pixelsPerBeat * zoomFactor);
        // Keep the beat under the mouse in the same screen position
        scrollX = beatAtMouse - (e.x - trackLabelWidth) / pixelsPerBeat;
        if (scrollX < 0.0) scrollX = 0.0;
    }
    else
    {
        // Two-finger scroll: horizontal and vertical simultaneously
        if (w.deltaX != 0.0f)
        {
            scrollX -= w.deltaX * 4.0;
            if (scrollX < 0.0) scrollX = 0.0;
        }

        if (w.deltaY != 0.0f)
        {
            if (e.mods.isShiftDown())
            {
                // Shift+wheel = horizontal scroll
                scrollX -= w.deltaY * 4.0;
                if (scrollX < 0.0) scrollX = 0.0;
            }
            else
            {
                // Vertical scroll = track scroll
                int totalContent = PluginHost::NUM_TRACKS * trackHeight;
                int maxScroll = juce::jmax(0, totalContent - (getHeight() - headerHeight));
                scrollY = juce::jlimit(0, maxScroll, scrollY - static_cast<int>(w.deltaY * trackHeight));
            }
        }
    }

    // Suppress auto-follow for 3 seconds after user scrolls
    userScrollActive = true;
    userScrollExpireTime = juce::Time::getMillisecondCounterHiRes() * 0.001 + 3.0;

    repaint();
}

void TimelineComponent::mouseMagnify(const juce::MouseEvent& e, float scaleFactor)
{
    // Pinch-to-zoom centered on the pinch point
    double beatAtPinch = xToBeat(static_cast<float>(e.x));
    pixelsPerBeat = juce::jlimit(10.0, 300.0, pixelsPerBeat * static_cast<double>(scaleFactor));
    // Keep the beat under the pinch center in the same screen position
    scrollX = beatAtPinch - (e.x - trackLabelWidth) / pixelsPerBeat;
    if (scrollX < 0.0) scrollX = 0.0;
    repaint();
}

bool TimelineComponent::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (onBeforeEdit) onBeforeEdit();
        deleteSelectedClip();
        return true;
    }

    if (key.getModifiers().isCtrlDown() && key.getKeyCode() == 'D')
    {
        if (onBeforeEdit) onBeforeEdit();
        duplicateSelectedClip();
        return true;
    }

    if (key.getModifiers().isCtrlDown() && key.getKeyCode() == 'B')
    {
        // Split at playhead
        if (selectedClip.isValid())
        {
            double playheadBeat = pluginHost.getEngine().getPositionInBeats();
            auto* clip = getClip(selectedClip);
            if (clip != nullptr)
            {
                double clipStart = clip->timelinePosition;
                double clipEnd = clipStart + clip->lengthInBeats;
                if (playheadBeat > clipStart && playheadBeat < clipEnd)
                    splitClipAtBeat(selectedClip, playheadBeat);
            }
        }
        return true;
    }

    return false;
}

// ── Editing operations ───────────────────────────────────────────────────────

void TimelineComponent::deleteSelectedClip()
{
    if (!selectedClip.isValid()) return;

    auto* slot = getSlot(selectedClip);
    if (slot == nullptr) return;

    slot->clip = nullptr;
    slot->audioClip = nullptr;
    slot->state.store(ClipSlot::Empty);
    selectedClip = {};
    repaint();
}

void TimelineComponent::duplicateSelectedClip()
{
    if (!selectedClip.isValid()) return;

    auto* srcClip = getClip(selectedClip);
    if (srcClip == nullptr) return;

    auto* cp = pluginHost.getTrack(selectedClip.trackIndex).clipPlayer;
    if (cp == nullptr) return;

    // Find empty slot on same track
    int emptySlot = cp->findOrCreateEmptySlot();
    if (emptySlot < 0) return;

    auto newClip = std::make_unique<MidiClip>();
    newClip->lengthInBeats = srcClip->lengthInBeats;
    newClip->timelinePosition = srcClip->timelinePosition + srcClip->lengthInBeats; // place after original

    // Copy MIDI events
    for (int i = 0; i < srcClip->events.getNumEvents(); ++i)
    {
        auto* event = srcClip->events.getEventPointer(i);
        newClip->events.addEvent(event->message);
    }
    newClip->events.updateMatchedPairs();

    cp->getSlot(emptySlot).clip = std::move(newClip);
    cp->getSlot(emptySlot).state.store(ClipSlot::Playing);

    selectedClip = { selectedClip.trackIndex, emptySlot };
    repaint();
}

void TimelineComponent::splitClipAtBeat(const ClipRef& ref, double beat)
{
    auto* srcClip = getClip(ref);
    if (srcClip == nullptr) return;

    double clipStart = srcClip->timelinePosition;
    double splitPoint = beat - clipStart; // relative to clip start

    if (splitPoint <= 0.0 || splitPoint >= srcClip->lengthInBeats) return;

    auto* cp = pluginHost.getTrack(ref.trackIndex).clipPlayer;
    if (cp == nullptr) return;

    // Find or create empty slot for the second half
    int emptySlot = cp->findOrCreateEmptySlot();
    if (emptySlot < 0) return;

    // Create second half clip
    auto newClip = std::make_unique<MidiClip>();
    newClip->timelinePosition = beat;
    newClip->lengthInBeats = srcClip->lengthInBeats - splitPoint;

    // Split MIDI events
    juce::MidiMessageSequence firstHalf, secondHalf;

    for (int i = 0; i < srcClip->events.getNumEvents(); ++i)
    {
        auto* event = srcClip->events.getEventPointer(i);
        double t = event->message.getTimeStamp();

        if (t < splitPoint)
        {
            firstHalf.addEvent(event->message);
        }
        else
        {
            auto msg = event->message;
            msg.setTimeStamp(t - splitPoint);
            secondHalf.addEvent(msg);
        }
    }

    firstHalf.updateMatchedPairs();
    secondHalf.updateMatchedPairs();

    // Fix orphaned notes spanning the split point:
    // For the first half: add note-off at split point for any note-on without a matching note-off
    for (int i = 0; i < firstHalf.getNumEvents(); ++i)
    {
        auto* ev = firstHalf.getEventPointer(i);
        if (ev->message.isNoteOn() && ev->noteOffObject == nullptr)
        {
            auto noteOff = juce::MidiMessage::noteOff(ev->message.getChannel(),
                                                       ev->message.getNoteNumber());
            noteOff.setTimeStamp(splitPoint - 0.001);
            firstHalf.addEvent(noteOff);
        }
    }
    firstHalf.updateMatchedPairs();

    // For the second half: add note-on at beat 0 for any note-off without a matching note-on
    for (int i = 0; i < secondHalf.getNumEvents(); ++i)
    {
        auto* ev = secondHalf.getEventPointer(i);
        if (ev->message.isNoteOff())
        {
            // Check if there's a matching note-on before this note-off
            bool hasMatchingNoteOn = false;
            for (int j = 0; j < i; ++j)
            {
                auto* prev = secondHalf.getEventPointer(j);
                if (prev->message.isNoteOn()
                    && prev->message.getNoteNumber() == ev->message.getNoteNumber()
                    && prev->message.getChannel() == ev->message.getChannel()
                    && prev->noteOffObject == ev)
                {
                    hasMatchingNoteOn = true;
                    break;
                }
            }
            if (!hasMatchingNoteOn)
            {
                auto noteOn = juce::MidiMessage::noteOn(ev->message.getChannel(),
                                                         ev->message.getNoteNumber(),
                                                         (juce::uint8) 100);
                noteOn.setTimeStamp(0.0);
                secondHalf.addEvent(noteOn);
            }
        }
    }
    secondHalf.updateMatchedPairs();

    // Update original clip (first half)
    srcClip->events = firstHalf;
    srcClip->lengthInBeats = splitPoint;

    // Set up second half
    newClip->events = secondHalf;
    cp->getSlot(emptySlot).clip = std::move(newClip);
    cp->getSlot(emptySlot).state.store(ClipSlot::Playing);

    repaint();
}

double TimelineComponent::snapToGrid(double beat) const
{
    return std::round(beat / gridResolution) * gridResolution;
}

void TimelineComponent::quantizeSelectedClip()
{
    auto* clip = getClip(selectedClip);
    if (clip == nullptr) return;

    // Ensure matched pairs are up to date so we can find paired note-offs
    clip->events.updateMatchedPairs();

    juce::MidiMessageSequence quantized;

    for (int i = 0; i < clip->events.getNumEvents(); ++i)
    {
        auto* event = clip->events.getEventPointer(i);
        auto msg = event->message;

        if (msg.isNoteOn())
        {
            // Snap note-on to grid
            double t = msg.getTimeStamp();
            double snapped = std::round(t / gridResolution) * gridResolution;
            if (snapped < 0.0) snapped = 0.0;
            double delta = snapped - t;
            msg.setTimeStamp(snapped);
            quantized.addEvent(msg);

            // Shift the paired note-off by the same delta to preserve note length
            if (event->noteOffObject != nullptr)
            {
                auto offMsg = event->noteOffObject->message;
                double offT = offMsg.getTimeStamp() + delta;
                if (offT < snapped) offT = snapped;
                offMsg.setTimeStamp(offT);
                quantized.addEvent(offMsg);
            }
        }
        else if (msg.isNoteOff())
        {
            // Already handled via paired note-on above
            continue;
        }
        else
        {
            // Non-note events: snap to grid
            double t = msg.getTimeStamp();
            t = std::round(t / gridResolution) * gridResolution;
            if (t < 0.0) t = 0.0;
            msg.setTimeStamp(t);
            quantized.addEvent(msg);
        }
    }

    quantized.updateMatchedPairs();
    clip->events = quantized;
    repaint();
}

void TimelineComponent::createClipAtPlayhead()
{
    int trackIdx = pluginHost.getSelectedTrack();
    double beatPos = pluginHost.getEngine().getPositionInBeats();
    beatPos = std::floor(beatPos); // snap to beat
    if (beatPos < 0.0) beatPos = 0.0;
    createEmptyClip(trackIdx, beatPos);
}

void TimelineComponent::deleteSelected()
{
    deleteSelectedClip();
}

void TimelineComponent::duplicateSelected()
{
    duplicateSelectedClip();
}

void TimelineComponent::splitSelected()
{
    if (!selectedClip.isValid()) return;
    auto* clip = getClip(selectedClip);
    if (clip == nullptr) return;

    double playheadBeat = pluginHost.getEngine().getPositionInBeats();
    double clipStart = clip->timelinePosition;
    double clipEnd = clipStart + clip->lengthInBeats;

    if (playheadBeat > clipStart && playheadBeat < clipEnd)
        splitClipAtBeat(selectedClip, playheadBeat);
}

MidiClip* TimelineComponent::getSelectedClip()
{
    return getClip(selectedClip);
}

void TimelineComponent::createEmptyClip(int trackIndex, double beatPos)
{
    auto* cp = pluginHost.getTrack(trackIndex).clipPlayer;
    if (cp == nullptr) return;

    int emptySlot = cp->findOrCreateEmptySlot();
    if (emptySlot < 0) return;

    auto newClip = std::make_unique<MidiClip>();
    newClip->timelinePosition = beatPos;
    newClip->lengthInBeats = 4.0; // 1 bar

    cp->getSlot(emptySlot).clip = std::move(newClip);
    cp->getSlot(emptySlot).state.store(ClipSlot::Stopped);

    selectedClip = { trackIndex, emptySlot };
    repaint();
}

// ── Drawing ──────────────────────────────────────────────────────────────────

// Helper to get the current theme (returns nullptr if no DawLookAndFeel)
static const DawTheme* getThemeColors(juce::Component* comp)
{
    if (auto* lnf = dynamic_cast<DawLookAndFeel*>(&comp->getLookAndFeel()))
        return &lnf->getTheme();
    return nullptr;
}

void TimelineComponent::paint(juce::Graphics& g)
{
    auto* tc = getThemeColors(this);
    auto* dawLnf = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel());
    bool isGlassPane = dawLnf && dawLnf->isGlassOverlayTheme();
    if (isGlassPane)
    {
        juce::Path clip;
        clip.addRoundedRectangle(getLocalBounds().toFloat(), 12.0f);
        g.reduceClipRegion(clip);
        // Tint — adapt to light/dark theme
        bool lightTheme = (tc && juce::Colour(tc->body).getBrightness() > 0.5f);
        g.fillAll(lightTheme ? juce::Colour(0xd0f0f0f4) : juce::Colour(0xb0080810));
    }
    else
    {
        g.fillAll(juce::Colour(tc ? tc->timelineBg : 0xff000000));
    }
    drawHeader(g);
    drawTrackLanes(g);
    drawClips(g);
    drawLoopRegion(g);
    drawLoopHandles(g);
    drawAutomation(g);
    drawPlayhead(g);

    // Loop Set Mode indicator
    if (loopSetMode)
    {
        g.setColour(juce::Colours::orange.withAlpha(0.15f));
        g.fillRect(getLocalBounds());
        g.setColour(juce::Colours::orange);
        g.setFont(16.0f);
        juce::String msg = loopSetTapCount == 0
            ? "Tap to set LOOP START"
            : "Tap to set LOOP END";
        g.drawText(msg, getLocalBounds().withHeight(headerHeight), juce::Justification::centred);
    }
}

void TimelineComponent::resized()
{
    recalcTrackHeight();
}

void TimelineComponent::recalcTrackHeight()
{
    int available = getHeight() - headerHeight;
    if (available > 0)
        trackHeight = juce::jmax(40, available / visibleTracks);

    // Clamp scroll
    int totalContent = PluginHost::NUM_TRACKS * trackHeight;
    int maxScroll = juce::jmax(0, totalContent - (getHeight() - headerHeight));
    scrollY = juce::jlimit(0, maxScroll, scrollY);
}

void TimelineComponent::drawHeader(juce::Graphics& g)
{
    auto* tc = getThemeColors(this);
    auto* lnf = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel());
    bool glassPane = lnf && lnf->isGlassOverlayTheme();
    bool lightHeader = glassPane && tc && juce::Colour(tc->body).getBrightness() > 0.5f;
    g.setColour(glassPane ? (lightHeader ? juce::Colour(0xc0e8e8f0) : juce::Colour(0xa0080810))
                          : juce::Colour(tc ? tc->bodyDark : 0xff0a0a0a));
    g.fillRect(0, 0, getWidth(), headerHeight);

    double firstBeat = std::floor(scrollX / gridResolution) * gridResolution;
    double lastBeat = scrollX + (getWidth() - trackLabelWidth) / pixelsPerBeat;

    // Glass pane: grid lines only on the selected track
    int selTrack = pluginHost.getSelectedTrack();
    float selTop = static_cast<float>(headerHeight + selTrack * trackHeight - scrollY);
    float selBot = selTop + static_cast<float>(trackHeight);
    float gridTop = glassPane ? juce::jmax(selTop, static_cast<float>(headerHeight)) : static_cast<float>(headerHeight);
    float gridBot = glassPane ? juce::jmin(selBot, static_cast<float>(getHeight())) : static_cast<float>(getHeight());

    for (double beat = firstBeat; beat <= lastBeat; beat += gridResolution)
    {
        float x = beatToX(beat);
        if (x < trackLabelWidth) continue;

        bool isBar = std::abs(std::fmod(beat, 4.0)) < 0.001;
        bool isBeat = std::abs(std::fmod(beat, 1.0)) < 0.001;

        if (isBar)
        {
            int barNum = static_cast<int>(beat / 4.0) + 1;
            g.setColour(juce::Colour(tc ? tc->textPrimary : 0xffcccccc));
            g.setFont(11.0f);
            g.drawText(juce::String(barNum), static_cast<int>(x) + 2, 0, 40, headerHeight / 2,
                       juce::Justification::centredLeft);
            g.setColour(juce::Colour(tc ? tc->timelineGridMajor : 0xff888888));
            g.drawVerticalLine(static_cast<int>(x), static_cast<float>(headerHeight / 2),
                               static_cast<float>(headerHeight));
            g.setColour(juce::Colour(tc ? tc->timelineGridMajor : 0xff666666));
            g.drawVerticalLine(static_cast<int>(x), gridTop, gridBot);
        }
        else if (isBeat)
        {
            int beatInBar = (static_cast<int>(std::round(beat)) % 4) + 1;
            g.setColour(juce::Colour(tc ? tc->textSecondary : 0xff888888));
            g.setFont(9.0f);
            g.drawText(juce::String(beatInBar), static_cast<int>(x) + 1, headerHeight / 2 - 2,
                       20, headerHeight / 2, juce::Justification::centredLeft);
            g.setColour(juce::Colour(tc ? tc->timelineGridMinor : 0xff555555));
            g.drawVerticalLine(static_cast<int>(x), static_cast<float>(headerHeight * 2 / 3),
                               static_cast<float>(headerHeight));
            g.setColour(juce::Colour(tc ? tc->timelineGridBeat : 0xff444444));
            g.drawVerticalLine(static_cast<int>(x), gridTop, gridBot);
        }
        else
        {
            g.setColour(juce::Colour(tc ? tc->timelineGridBeat : 0xff444444));
            g.drawVerticalLine(static_cast<int>(x), static_cast<float>(headerHeight * 3 / 4),
                               static_cast<float>(headerHeight));
            g.setColour(juce::Colour(tc ? tc->timelineGridFaint : 0xff2d2d2d));
            g.drawVerticalLine(static_cast<int>(x), gridTop, gridBot);
        }
    }

    g.setColour(juce::Colour(tc ? tc->border : 0xff444444));
    g.drawHorizontalLine(headerHeight - 1, 0, static_cast<float>(getWidth()));
}

void TimelineComponent::drawTrackLanes(juce::Graphics& g)
{
    // Clip to track area (below header)
    g.saveState();
    g.reduceClipRegion(0, headerHeight, getWidth(), getHeight() - headerHeight);

    for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
    {
        int y = headerHeight + t * trackHeight - scrollY;

        // Skip off-screen tracks
        if (y + trackHeight < headerHeight || y > getHeight()) continue;

        // Selected track highlight
        auto* tc = getThemeColors(this);
        bool isSelected = (t == pluginHost.getSelectedTrack());
        auto* lnf2 = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel());
        bool glassPane = lnf2 && lnf2->isGlassOverlayTheme();

        if (glassPane)
        {
            // Glass pane: very subtle translucent fills
            if (isSelected)
                g.setColour(juce::Colour(tc->timelineSelectedRow));
            else if (t % 2 != 0)
                g.setColour(juce::Colour(0x08ffffff));  // barely visible alt row
            else
                continue;  // skip even rows — fully transparent
            g.fillRect(0, y, getWidth(), trackHeight);
            // Still need lane separator
            g.setColour(juce::Colour(tc ? tc->timelineGridMinor : 0xff333333));
            g.drawHorizontalLine(y + trackHeight - 1, 0, static_cast<float>(getWidth()));
            continue;
        }

        if (isSelected)
            g.setColour(juce::Colour(tc ? tc->timelineSelectedRow : 0xff0a1520));
        else
            g.setColour(t % 2 == 0 ? juce::Colour(tc ? tc->timelineBg : 0xff000000)
                                    : juce::Colour(tc ? tc->timelineAltRow : 0xff060606));
        g.fillRect(0, y, getWidth(), trackHeight);

        g.setColour(juce::Colour(tc ? tc->timelineGridMinor : 0xff333333));
        g.drawHorizontalLine(y + trackHeight - 1, 0, static_cast<float>(getWidth()));
    }

    drawTrackControls(g);
    g.restoreState();
}

juce::Rectangle<int> TimelineComponent::getSelectButtonRect(int trackIndex) const
{
    int y = headerHeight + trackIndex * trackHeight - scrollY;
    return { 16, y + 2, trackLabelWidth - 54, trackHeight - 4 };
}

juce::Rectangle<int> TimelineComponent::getMuteButtonRect(int trackIndex) const
{
    int y = headerHeight + trackIndex * trackHeight - scrollY;
    return { trackLabelWidth - 36, y + 3, 34, (trackHeight - 8) / 2 };
}

juce::Rectangle<int> TimelineComponent::getSoloButtonRect(int trackIndex) const
{
    int y = headerHeight + trackIndex * trackHeight - scrollY;
    int halfH = (trackHeight - 8) / 2;
    return { trackLabelWidth - 36, y + 3 + halfH + 2, 34, halfH };
}

void TimelineComponent::drawTrackControls(juce::Graphics& g)
{
    for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
    {
        auto& track = pluginHost.getTrack(t);
        bool isSelected = (t == pluginHost.getSelectedTrack());
        bool isArmed = track.clipPlayer != nullptr && track.clipPlayer->armed.load();
        bool isLocked = track.clipPlayer != nullptr && track.clipPlayer->armLocked.load();

        // Track select button — color shows selection + arm state
        auto selRect = getSelectButtonRect(t);

        auto* tc = getThemeColors(this);
        if (isLocked)
            g.setColour(juce::Colour(tc ? tc->trackArmed : 0xff882222));
        else if (isSelected)
            g.setColour(juce::Colour(tc ? tc->trackSelected : 0xff3a5a8a));
        else
            g.setColour(juce::Colour(tc ? tc->timelineGridMinor : 0xff333333));

        g.fillRoundedRectangle(selRect.toFloat(), 3.0f);

        // Arm indicator dot on the left side
        if (isArmed || isLocked)
        {
            g.setColour(juce::Colour(tc ? tc->red : 0xffcc2222));
            g.fillEllipse(static_cast<float>(selRect.getX() + 4),
                         static_cast<float>(selRect.getCentreY() - 4), 8.0f, 8.0f);
        }

        // Track label
        g.setColour(juce::Colour(tc ? tc->textBright : 0xffffffff));
        g.setFont(13.0f);
        juce::String label = juce::String(t + 1);
        if (track.plugin != nullptr)
            label += " " + track.plugin->getName().substring(0, 7);
        g.drawText(label, selRect.reduced(14, 0), juce::Justification::centredLeft);

        // Mute button
        auto muteRect = getMuteButtonRect(t);
        bool isMuted = track.gainProcessor != nullptr && track.gainProcessor->muted.load();
        g.setColour(isMuted ? juce::Colour(tc ? tc->trackMuteOn : 0xffff0000)
                            : juce::Colour(tc ? tc->border : 0xff444444));
        g.fillRoundedRectangle(muteRect.toFloat(), 3.0f);
        g.setColour(juce::Colour(tc ? tc->textBright : 0xffffffff));
        g.setFont(13.0f);
        g.drawText("M", muteRect, juce::Justification::centred);

        // Solo button
        auto soloRect = getSoloButtonRect(t);
        bool isSoloed = track.gainProcessor != nullptr && track.gainProcessor->soloed.load();
        g.setColour(isSoloed ? juce::Colour(tc ? tc->trackSoloOn : 0xffffff00)
                             : juce::Colour(tc ? tc->border : 0xff444444));
        g.fillRoundedRectangle(soloRect.toFloat(), 3.0f);
        g.setColour(isSoloed ? juce::Colour(tc ? tc->trackSoloText : 0xff000000)
                             : juce::Colour(tc ? tc->textBright : 0xffffffff));
        g.setFont(13.0f);
        g.drawText("S", soloRect, juce::Justification::centred);

        // VU meter (vertical) + CPU meter
        if (track.gainProcessor != nullptr)
        {
            int y = headerHeight + t * trackHeight - scrollY;
            int meterH = trackHeight - 6;
            int meterX = 2;

            // Get theme meter color
            uint32_t meterColor = tc ? tc->lcdText : 0xffc8e4ff;
            uint32_t meterBgColor = tc ? tc->bodyDark : 0xff1a1a22;
            uint32_t cpuColor = tc ? tc->green : 0xff44dd66;

            float pkL = juce::jlimit(0.0f, 1.0f, track.gainProcessor->peakLevelL.load());
            float pkR = juce::jlimit(0.0f, 1.0f, track.gainProcessor->peakLevelR.load());
            float cpu = juce::jlimit(0.0f, 100.0f, track.gainProcessor->cpuPercent.load());

            int fillL = static_cast<int>(pkL * meterH);
            int fillR = static_cast<int>(pkR * meterH);

            // VU background — two thin vertical bars
            g.setColour(juce::Colour(meterBgColor));
            g.fillRect(meterX, y + 3, 3, meterH);
            g.fillRect(meterX + 4, y + 3, 3, meterH);

            // VU fill — L channel (bottom-up)
            g.setColour(pkL > 0.9f ? juce::Colour(tc ? tc->red : 0xffee4444) : juce::Colour(meterColor));
            g.fillRect(meterX, y + 3 + meterH - fillL, 3, fillL);

            // VU fill — R channel (bottom-up)
            g.setColour(pkR > 0.9f ? juce::Colour(tc ? tc->red : 0xffee4444) : juce::Colour(meterColor));
            g.fillRect(meterX + 4, y + 3 + meterH - fillR, 3, fillR);

            // CPU bar — thin vertical bar next to VU
            if (cpu > 0.1f)
            {
                int fillCpu = static_cast<int>((cpu / 100.0f) * meterH);
                g.setColour(cpu > 50.0f ? juce::Colour(tc ? tc->red : 0xffee4444) : juce::Colour(cpuColor));
                g.fillRect(meterX + 9, y + 3 + meterH - fillCpu, 2, fillCpu);
            }
        }

        // Divider
        g.setColour(juce::Colour(tc ? tc->border : 0xff444444));
        g.drawVerticalLine(trackLabelWidth - 1, static_cast<float>(headerHeight),
                           static_cast<float>(getHeight()));
    }
}

void TimelineComponent::handleTrackControlClick(int trackIndex, float x, float y)
{
    auto& track = pluginHost.getTrack(trackIndex);

    // Check Mute button
    auto muteRect = getMuteButtonRect(trackIndex);
    if (muteRect.toFloat().contains(x, y))
    {
        if (track.gainProcessor != nullptr)
            track.gainProcessor->muted.store(!track.gainProcessor->muted.load());
        repaint();
        return;
    }

    // Check Solo button
    auto soloRect = getSoloButtonRect(trackIndex);
    if (soloRect.toFloat().contains(x, y))
    {
        if (track.gainProcessor != nullptr)
        {
            bool was = track.gainProcessor->soloed.load();
            bool now = !was;
            track.gainProcessor->soloed.store(now);
            if (now && !was) pluginHost.soloCount.fetch_add(1);
            else if (!now && was) pluginHost.soloCount.fetch_sub(1);
        }
        repaint();
        return;
    }

    // Click on track name = select track
    auto selRect = getSelectButtonRect(trackIndex);
    if (selRect.toFloat().contains(x, y))
    {
        pluginHost.setSelectedTrack(trackIndex);
        repaint();
        return;
    }
}

void TimelineComponent::drawClips(juce::Graphics& g)
{
    // Clip drawing to the timeline area only — don't draw over track controls
    g.saveState();
    g.reduceClipRegion(trackLabelWidth, headerHeight,
                       getWidth() - trackLabelWidth, getHeight() - headerHeight);

    for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
    {
        auto* cp = pluginHost.getTrack(t).clipPlayer;
        if (cp == nullptr) continue;

        for (int s = 0; s < cp->getNumSlots(); ++s)
        {
            auto& slot = cp->getSlot(s);
            if (slot.clip == nullptr && slot.audioClip == nullptr) continue;

            auto clipRect = getClipRect(t, s);
            if (clipRect.isEmpty()) continue;
            if (clipRect.getRight() < trackLabelWidth || clipRect.getX() > getWidth()) continue;

            // Color based on state
            auto* tc = getThemeColors(this);
            auto state = slot.state.load();
            juce::Colour clipColor;
            if (state == ClipSlot::Playing)
                clipColor = juce::Colour(tc ? tc->clipPlaying : 0xff338844);
            else if (state == ClipSlot::Recording)
                clipColor = juce::Colour(tc ? tc->clipRecording : 0xff5588bb);
            else if (state == ClipSlot::Armed)
                clipColor = juce::Colour(tc ? tc->clipQueued : 0xff884400);
            else
                clipColor = juce::Colour(tc ? tc->clipDefault : 0xff445566);

            // Selected highlight
            bool isSelected = selectedClip.trackIndex == t && selectedClip.slotIndex == s;
            if (isSelected)
                clipColor = clipColor.brighter(0.3f);

            g.setColour(clipColor);
            g.fillRoundedRectangle(clipRect, 3.0f);

            g.setColour(isSelected ? juce::Colours::white : clipColor.brighter(0.3f));
            g.drawRoundedRectangle(clipRect, 3.0f, isSelected ? 2.0f : 1.0f);

            // Resize handles
            if (isSelected)
            {
                g.setColour(juce::Colours::white.withAlpha(0.4f));
                g.fillRect(clipRect.getX(), clipRect.getY() + 4, 3.0f, clipRect.getHeight() - 8);
                g.fillRect(clipRect.getRight() - 3, clipRect.getY() + 4, 3.0f, clipRect.getHeight() - 8);
            }

            // Mini note / waveform preview
            if (slot.hasContent())
            {
                g.saveState();
                g.reduceClipRegion(clipRect.toNearestInt());

                if (slot.clip != nullptr)
                {
                    drawMiniNotes(g, *slot.clip, clipRect);
                }
                else if (slot.audioClip != nullptr)
                {
                    // Draw waveform
                    auto& audio = *slot.audioClip;
                    if (audio.samples.getNumSamples() > 0)
                    {
                        int numSamples = audio.samples.getNumSamples();
                        int ch = 0; // draw first channel
                        auto* data = audio.samples.getReadPointer(ch);

                        float w = clipRect.getWidth();
                        float h = clipRect.getHeight();
                        float midY = clipRect.getCentreY();

                        g.setColour(clipColor.brighter(0.2f));

                        int pixelWidth = static_cast<int>(w);
                        for (int px = 0; px < pixelWidth; ++px)
                        {
                            float frac = static_cast<float>(px) / w;
                            int sampleIdx = static_cast<int>(frac * numSamples);

                            // Find peak in this pixel's sample range
                            int nextSampleIdx = static_cast<int>((px + 1.0f) / w * numSamples);
                            nextSampleIdx = juce::jmin(nextSampleIdx, numSamples - 1);
                            sampleIdx = juce::jmin(sampleIdx, numSamples - 1);

                            float peak = 0.0f;
                            for (int si = sampleIdx; si <= nextSampleIdx; ++si)
                                peak = juce::jmax(peak, std::abs(data[si]));

                            float barH = peak * h * 0.45f;
                            float x = clipRect.getX() + px;
                            g.drawVerticalLine(static_cast<int>(x), midY - barH, midY + barH);
                        }
                    }
                }

                g.restoreState();
            }
        }
    }

    g.restoreState();
}

void TimelineComponent::drawMiniNotes(juce::Graphics& g, const MidiClip& clip, juce::Rectangle<float> area)
{
    if (clip.events.getNumEvents() == 0 || clip.lengthInBeats <= 0.0) return;

    int minNote = 127, maxNote = 0;
    for (int i = 0; i < clip.events.getNumEvents(); ++i)
    {
        auto& msg = clip.events.getEventPointer(i)->message;
        if (msg.isNoteOn())
        {
            minNote = juce::jmin(minNote, msg.getNoteNumber());
            maxNote = juce::jmax(maxNote, msg.getNoteNumber());
        }
    }

    if (minNote > maxNote) return;
    int noteRange = juce::jmax(1, maxNote - minNote + 1);

    float noteH = juce::jmax(1.0f, (area.getHeight() - 6.0f) / static_cast<float>(noteRange));
    float beatsToPixels = area.getWidth() / static_cast<float>(clip.lengthInBeats);

    auto* tc = getThemeColors(this);
    g.setColour(tc ? juce::Colour(tc->clipNotePreview) : juce::Colours::white.withAlpha(0.5f));

    for (int i = 0; i < clip.events.getNumEvents(); ++i)
    {
        auto* event = clip.events.getEventPointer(i);
        if (!event->message.isNoteOn()) continue;

        float nx = area.getX() + static_cast<float>(event->message.getTimeStamp()) * beatsToPixels;
        float noteLen = 0.25f;
        if (event->noteOffObject != nullptr)
        {
            noteLen = static_cast<float>(event->noteOffObject->message.getTimeStamp()
                                         - event->message.getTimeStamp());
            if (noteLen < 0.05f) noteLen = 0.25f;
        }
        float nw = noteLen * beatsToPixels;

        int noteRow = maxNote - event->message.getNoteNumber();
        float ny = area.getY() + 3.0f + noteRow * noteH;

        g.fillRect(nx, ny, juce::jmax(1.0f, nw), juce::jmax(1.0f, noteH - 1.0f));
    }
}

void TimelineComponent::drawAutomation(juce::Graphics& g)
{
    // Use OLED blue for all automation lanes with varying alpha
    uint32_t lcdBlue = 0xffb8d8f0;
    if (auto* lnf = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel()))
        lcdBlue = lnf->getTheme().lcdText;
    static const float alphas[] = { 0.9f, 0.7f, 0.55f, 0.85f, 0.65f, 0.5f };
    (void)alphas;

    for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
    {
        auto& track = pluginHost.getTrack(t);
        int laneY = headerHeight + t * trackHeight - scrollY;
        int colorIdx = 0;

        for (auto* lane : track.automationLanes)
        {
            if (lane->points.size() < 2) { colorIdx++; continue; }

            g.setColour(juce::Colour(lcdBlue).withAlpha(alphas[colorIdx % 6]));
            colorIdx++;

            juce::Path path;
            bool started = false;

            for (auto& pt : lane->points)
            {
                float x = beatToX(pt.beat);
                float y = static_cast<float>(laneY + trackHeight - 4)
                         - pt.value * static_cast<float>(trackHeight - 8);

                if (x < static_cast<float>(trackLabelWidth) || x > static_cast<float>(getWidth()))
                    continue;

                if (!started)
                {
                    path.startNewSubPath(x, y);
                    started = true;
                }
                else
                {
                    path.lineTo(x, y);
                }
            }

            if (started)
                g.strokePath(path, juce::PathStrokeType(2.0f));

            // Draw circles at each point for editing targets
            auto pointColor = juce::Colour(lcdBlue).withAlpha(alphas[(colorIdx - 1) % 6]);
            for (auto& pt : lane->points)
            {
                float x = beatToX(pt.beat);
                if (x < static_cast<float>(trackLabelWidth) || x > static_cast<float>(getWidth()))
                    continue;
                float y = static_cast<float>(laneY + trackHeight - 4)
                         - pt.value * static_cast<float>(trackHeight - 8);

                // Filled dot
                g.setColour(pointColor);
                g.fillEllipse(x - 4.0f, y - 4.0f, 8.0f, 8.0f);
                // Bright border
                g.setColour(pointColor.brighter(0.5f));
                g.drawEllipse(x - 4.0f, y - 4.0f, 8.0f, 8.0f, 1.0f);
            }
        }
    }
}

void TimelineComponent::drawPlayhead(juce::Graphics& g)
{
    auto& engine = pluginHost.getEngine();
    double pos = engine.getPositionInBeats();
    float x = beatToX(pos);

    if (x < static_cast<float>(trackLabelWidth) || x > static_cast<float>(getWidth())) return;

    auto* lnf = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel());
    uint32_t phColor = 0xdd44dd66;
    uint32_t phGlow  = 0x3344dd66;
    if (lnf)
    {
        phColor = lnf->getTheme().playhead;
        phGlow  = lnf->getTheme().playheadGlow;
    }

    bool glassTheme = lnf && lnf->isGlassOverlayTheme();
    float hTop = static_cast<float>(headerHeight);
    float hBot = static_cast<float>(getHeight());

    // ── Glass overlay: luminous wake trailing behind the playhead ──
    if (glassTheme && engine.isPlaying())
    {
        juce::Colour ph(phColor);
        float t = static_cast<float>(juce::Time::getMillisecondCounterHiRes() * 0.001);

        // Soft wake trailing behind (to the left of playhead)
        float wakeWidth = 30.0f;
        for (float dx = 1.0f; dx < wakeWidth; dx += 2.0f)
        {
            float fade = 1.0f - (dx / wakeWidth);
            fade = fade * fade;  // ease out
            // Gentle wave distortion in the wake
            float waveOff = std::sin(dx * 0.15f + t * 2.0f) * 1.5f * fade;
            float alpha = fade * 0.08f;
            g.setColour(ph.withAlpha(alpha));
            g.drawVerticalLine(static_cast<int>(x - dx + waveOff), hTop, hBot);
        }

        // Pulsing glow around the playhead
        float pulse = 0.5f + 0.5f * std::sin(t * 3.0f);
        float glowW = 6.0f + 2.0f * pulse;
        juce::ColourGradient glow(
            ph.withAlpha(0.15f + 0.08f * pulse), x, hTop,
            ph.withAlpha(0.0f), x - glowW, hTop, false);
        g.setGradientFill(glow);
        g.fillRect(x - glowW, hTop, glowW, hBot - hTop);

        juce::ColourGradient glow2(
            ph.withAlpha(0.15f + 0.08f * pulse), x, hTop,
            ph.withAlpha(0.0f), x + glowW, hTop, false);
        g.setGradientFill(glow2);
        g.fillRect(x, hTop, glowW, hBot - hTop);
    }

    // Main playhead line
    g.setColour(juce::Colour(phColor));
    g.drawVerticalLine(static_cast<int>(x), hTop, hBot);

    // Standard glow
    g.setColour(juce::Colour(phGlow));
    g.fillRect(x - 1.0f, hTop, 3.0f, hBot - hTop);

    // Top triangle
    g.setColour(juce::Colour(phColor));
    juce::Path triangle;
    triangle.addTriangle(x - 5, hTop, x + 5, hTop, x, hTop + 8);
    g.fillPath(triangle);
}

void TimelineComponent::drawLoopRegion(juce::Graphics& g)
{
    auto& engine = pluginHost.getEngine();
    if (!engine.hasLoopRegion()) return;

    float x1 = beatToX(engine.getLoopStart());
    float x2 = beatToX(engine.getLoopEnd());

    if (x2 < static_cast<float>(trackLabelWidth) || x1 > static_cast<float>(getWidth())) return;

    x1 = juce::jmax(x1, static_cast<float>(trackLabelWidth));
    x2 = juce::jmin(x2, static_cast<float>(getWidth()));

    // Get theme colors (fall back to blue if no LookAndFeel set)
    uint32_t regionColor = 0x2244aaff;
    uint32_t borderColor = 0xff4488cc;
    if (auto* lnf = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel()))
    {
        regionColor = lnf->getTheme().loopRegion;
        borderColor = lnf->getTheme().loopBorder;
    }

    // Header bar
    g.setColour(juce::Colour(borderColor).withAlpha(0.4f));
    g.fillRect(x1, 0.0f, x2 - x1, static_cast<float>(headerHeight));

    // Overlay across tracks
    g.setColour(juce::Colour(regionColor));
    g.fillRect(x1, static_cast<float>(headerHeight), x2 - x1,
               static_cast<float>(getHeight() - headerHeight));

    // Left/right borders
    g.setColour(juce::Colour(borderColor));
    g.drawVerticalLine(static_cast<int>(x1), 0.0f, static_cast<float>(getHeight()));
    g.drawVerticalLine(static_cast<int>(x2), 0.0f, static_cast<float>(getHeight()));

    // "L" markers at top
    g.setFont(10.0f);
    g.drawText("L", static_cast<int>(x1) + 2, 1, 12, headerHeight - 2, juce::Justification::topLeft);
    g.drawText("R", static_cast<int>(x2) - 14, 1, 12, headerHeight - 2, juce::Justification::topRight);
}

void TimelineComponent::drawLoopHandles(juce::Graphics& g)
{
    auto& engine = pluginHost.getEngine();
    if (!engine.hasLoopRegion()) return;

    float x1 = beatToX(engine.getLoopStart());
    float x2 = beatToX(engine.getLoopEnd());

    // Skip if entirely off-screen
    float minX = static_cast<float>(trackLabelWidth);
    float maxX = static_cast<float>(getWidth());
    if (x1 > maxX && x2 > maxX) return;
    if (x1 < minX && x2 < minX) return;

    uint32_t handleColor = 0xff4488cc;
    if (auto* lnf = dynamic_cast<DawLookAndFeel*>(&getLookAndFeel()))
        handleColor = lnf->getTheme().loopBorder;

    auto col = juce::Colour(handleColor);
    int handleW = 10;
    int handleH = headerHeight;

    auto drawHandle = [&](float x) {
        // Clamp to visible area
        if (x < minX - handleW || x > maxX + handleW) return;

        auto rect = juce::Rectangle<float>(x - handleW / 2.0f, 0.0f,
                                            (float)handleW, (float)handleH);
        g.setColour(col.withAlpha(0.9f));
        g.fillRoundedRectangle(rect, 3.0f);
        // Grip dots
        g.setColour(col.brighter(0.5f));
        float cy = rect.getCentreY();
        for (int i = 0; i < 3; ++i)
            g.fillEllipse(rect.getCentreX() - 1.5f, cy - 6.0f + i * 6.0f, 3.0f, 3.0f);
    };

    drawHandle(x1);
    drawHandle(x2);
}

// ── Automation point helpers ────────────────────────────────────────────────

float TimelineComponent::autoPointToY(int trackIndex, float value) const
{
    int laneY = headerHeight + trackIndex * trackHeight - scrollY;
    return static_cast<float>(laneY + trackHeight - 4) - value * static_cast<float>(trackHeight - 8);
}

float TimelineComponent::yToAutoValue(int trackIndex, float y) const
{
    int laneY = headerHeight + trackIndex * trackHeight - scrollY;
    float range = static_cast<float>(trackHeight - 8);
    float val = (static_cast<float>(laneY + trackHeight - 4) - y) / range;
    return juce::jlimit(0.0f, 1.0f, val);
}

TimelineComponent::AutoPointRef TimelineComponent::hitTestAutoPoint(float x, float y) const
{
    for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
    {
        auto& track = pluginHost.getTrack(t);
        int laneIdx = 0;
        for (auto* lane : track.automationLanes)
        {
            for (int pi = lane->points.size() - 1; pi >= 0; --pi)
            {
                auto& pt = lane->points.getReference(pi);
                float px = beatToX(pt.beat);
                float py = autoPointToY(t, pt.value);
                float dx = x - px;
                float dy = y - py;
                if (dx * dx + dy * dy < autoPointHitRadius * autoPointHitRadius)
                    return { t, laneIdx, pi };
            }
            laneIdx++;
        }
    }
    return {};
}

// ── Clip context menu (long-press) ──────────────────────────────────────────

void TimelineComponent::showClipContextMenu(const ClipRef& ref)
{
    auto* slot = getSlot(ref);
    if (slot == nullptr) return;

    auto* midiClip = getClip(ref);
    bool isAudio = (slot->audioClip != nullptr);
    bool isMidi = (midiClip != nullptr);

    if (!isAudio && !isMidi) return;

    juce::PopupMenu menu;
    if (isMidi)
    {
        menu.addItem(1, "Copy");
        menu.addItem(2, "Paste", clipboardClip != nullptr);
        menu.addSeparator();
        menu.addItem(3, "Transpose +Octave");
        menu.addItem(4, "Transpose -Octave");
        menu.addSeparator();
        menu.addItem(5, "Velocity 50%");
        menu.addItem(6, "Velocity 75%");
        menu.addItem(7, "Velocity 150%");
        menu.addSeparator();
    }
    menu.addItem(8, "Duplicate");
    menu.addItem(9, "Delete");

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(
        localAreaToGlobal(getClipRect(ref.trackIndex, ref.slotIndex).toNearestInt())),
        [this, ref](int result)
    {
        auto* clip = getClip(ref);
        // For audio clips, only allow duplicate/delete
        if (clip == nullptr && result != 2 && result != 8 && result != 9) return;

        switch (result)
        {
            case 1: // Copy
            {
                clipboardClip = std::make_unique<MidiClip>();
                clipboardClip->lengthInBeats = clip->lengthInBeats;
                clipboardClip->timelinePosition = clip->timelinePosition;
                for (int i = 0; i < clip->events.getNumEvents(); ++i)
                    clipboardClip->events.addEvent(clip->events.getEventPointer(i)->message);
                clipboardClip->events.updateMatchedPairs();
                clipboardTrack = ref.trackIndex;
                break;
            }
            case 2: // Paste
            {
                if (clipboardClip == nullptr) return;
                auto* cp = pluginHost.getTrack(ref.trackIndex).clipPlayer;
                if (cp == nullptr) return;
                int emptySlot = cp->findOrCreateEmptySlot();
                if (emptySlot < 0) return;
                {
                    auto newClip = std::make_unique<MidiClip>();
                    newClip->lengthInBeats = clipboardClip->lengthInBeats;
                    newClip->timelinePosition = clipboardClip->timelinePosition;
                    for (int i = 0; i < clipboardClip->events.getNumEvents(); ++i)
                        newClip->events.addEvent(clipboardClip->events.getEventPointer(i)->message);
                    newClip->events.updateMatchedPairs();
                    cp->getSlot(emptySlot).clip = std::move(newClip);
                    cp->getSlot(emptySlot).state.store(ClipSlot::Stopped);
                }
                repaint();
                break;
            }
            case 3: // Transpose +Octave
            {
                if (onBeforeEdit) onBeforeEdit();
                juce::MidiMessageSequence newEvents;
                for (int i = 0; i < clip->events.getNumEvents(); ++i)
                {
                    auto msg = clip->events.getEventPointer(i)->message;
                    if (msg.isNoteOnOrOff())
                    {
                        int newNote = juce::jmin(127, msg.getNoteNumber() + 12);
                        msg.setNoteNumber(newNote);
                    }
                    newEvents.addEvent(msg);
                }
                newEvents.updateMatchedPairs();
                clip->events = newEvents;
                repaint();
                break;
            }
            case 4: // Transpose -Octave
            {
                if (onBeforeEdit) onBeforeEdit();
                juce::MidiMessageSequence newEvents;
                for (int i = 0; i < clip->events.getNumEvents(); ++i)
                {
                    auto msg = clip->events.getEventPointer(i)->message;
                    if (msg.isNoteOnOrOff())
                    {
                        int newNote = juce::jmax(0, msg.getNoteNumber() - 12);
                        msg.setNoteNumber(newNote);
                    }
                    newEvents.addEvent(msg);
                }
                newEvents.updateMatchedPairs();
                clip->events = newEvents;
                repaint();
                break;
            }
            case 5: // Velocity 50%
            case 6: // Velocity 75%
            case 7: // Velocity 150%
            {
                if (onBeforeEdit) onBeforeEdit();
                float scale = (result == 5) ? 0.5f : (result == 6) ? 0.75f : 1.5f;
                juce::MidiMessageSequence newEvents;
                for (int i = 0; i < clip->events.getNumEvents(); ++i)
                {
                    auto msg = clip->events.getEventPointer(i)->message;
                    if (msg.isNoteOn())
                    {
                        int vel = juce::jlimit(1, 127, static_cast<int>(msg.getVelocity() * scale));
                        msg.setVelocity(static_cast<float>(vel) / 127.0f);
                    }
                    newEvents.addEvent(msg);
                }
                newEvents.updateMatchedPairs();
                clip->events = newEvents;
                repaint();
                break;
            }
            case 8: // Duplicate
                duplicateSelectedClip();
                break;
            case 9: // Delete
                deleteSelectedClip();
                break;
        }
    });
}
