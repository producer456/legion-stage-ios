#pragma once

#include <JuceHeader.h>
#include <vector>
#include <cmath>

// OLED-style pixel art splash screen.
// Phase 1: Pixels scatter then morph into "LEGION STAGE"
// Phase 2: Hold with glow + subtitle
// Phase 3: Scrolling source code boot sequence
class SplashComponent : public juce::Component, public juce::Timer
{
public:
    std::function<void()> onFinished;

    SplashComponent()
    {
        setOpaque(true);
        buildTextPixels();
        startTimerHz(60);
    }

    ~SplashComponent() override { stopTimer(); }

    void timerCallback() override
    {
        elapsed += 1.0f / 60.0f;

        repaint();

        // Finish after logo fade completes (3.5s = holdEnd 2.5 + 1.0s fade)
        if (elapsed > 3.5f && onFinished)
        {
            stopTimer();
            onFinished();
        }
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);

        int w = getWidth();
        int h = getHeight();
        if (w < 10 || h < 10) return;

        juce::Colour ice(0xffb8d8f0);

        // Phase timing
        float assembleEnd = 1.5f;
        float holdEnd = 2.5f;

        // ── Phase 1 & 2: Logo animation ──
        if (elapsed < holdEnd + 0.5f)
        {
            drawLogoPhase(g, w, h, ice);
        }

        // Boot text removed — logo holds then fades to main UI
    }

private:
    float elapsed = 0.0f;
    static constexpr float bootStartTime = 2.2f;
    static constexpr float totalDuration = 5.5f;

    // ── Logo pixels ──
    struct Pixel {
        int row, col;
        float startX, startY, phase;
    };
    std::vector<Pixel> textPixels;
    int maxRow = 0, maxCol = 0;


    void drawLogoPhase(juce::Graphics& g, int w, int h, juce::Colour ice)
    {
        if (textPixels.empty()) return;

        float textW = static_cast<float>(maxCol + 1);
        float textH = static_cast<float>(maxRow + 1);
        float pixScale = juce::jmin(
            static_cast<float>(w) * 0.7f / textW,
            static_cast<float>(h) * 0.3f / textH);
        pixScale = juce::jmax(2.0f, std::floor(pixScale));

        float textDrawW = textW * pixScale;
        float textDrawH = textH * pixScale;
        float offsetX = (w - textDrawW) * 0.5f;
        float offsetY = (h * 0.35f - textDrawH * 0.5f);

        float assembleT = juce::jlimit(0.0f, 1.0f, elapsed / 1.5f);
        float eased = 1.0f - (1.0f - assembleT) * (1.0f - assembleT) * (1.0f - assembleT);

        float glowPulse = 0.0f;
        if (elapsed > 1.5f && elapsed < 2.5f)
            glowPulse = 0.3f + 0.2f * std::sin((elapsed - 1.5f) * 6.0f);

        // Fade logo during boot sequence
        float logoAlpha = 1.0f;
        if (elapsed > 2.5f)
            logoAlpha = juce::jmax(0.0f, 1.0f - (elapsed - 2.5f) / 1.0f);
        if (logoAlpha <= 0.0f) return;

        for (size_t i = 0; i < textPixels.size(); ++i)
        {
            auto& px = textPixels[i];
            float fx = px.startX + (px.col * pixScale + offsetX - px.startX) * eased;
            float fy = px.startY + (px.row * pixScale + offsetY - px.startY) * eased;

            if (assembleT < 1.0f)
            {
                float wobble = (1.0f - eased) * 3.0f;
                fx += std::sin(elapsed * 8.0f + px.phase) * wobble;
                fy += std::cos(elapsed * 7.0f + px.phase * 1.3f) * wobble;
            }

            float brightness = eased * 0.6f + 0.4f;
            if (assembleT < 0.3f)
                brightness = 0.3f + 0.7f * std::sin(elapsed * 12.0f + px.phase);

            float alpha = brightness * logoAlpha;

            if (glowPulse > 0.0f)
            {
                g.setColour(ice.withAlpha(glowPulse * logoAlpha * 0.3f));
                g.fillRect(fx - pixScale * 0.3f, fy - pixScale * 0.3f,
                           pixScale * 1.6f, pixScale * 1.6f);
            }

            g.setColour(ice.withAlpha(alpha));
            g.fillRect(fx, fy, pixScale, pixScale);
        }

        // Trail particles during scatter
        if (assembleT < 1.0f)
        {
            juce::Random rng(static_cast<int>(elapsed * 100));
            int numTrails = static_cast<int>((1.0f - eased) * 40);
            for (int i = 0; i < numTrails; ++i)
            {
                float tx = rng.nextFloat() * w;
                float ty = rng.nextFloat() * h;
                g.setColour(ice.withAlpha((1.0f - eased) * 0.15f));
                g.fillRect(tx, ty, pixScale * 0.5f, pixScale * 0.5f);
            }
        }

        // Subtitle
        if (elapsed > 1.8f && logoAlpha > 0.0f)
        {
            float subAlpha = juce::jmin(1.0f, (elapsed - 1.8f) / 0.5f) * logoAlpha;
            g.setColour(ice.withAlpha(subAlpha * 0.5f));
            g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain)));
            g.drawText("T O U C H   D A W", 0, static_cast<int>(offsetY + textDrawH + pixScale * 3),
                       w, 20, juce::Justification::centred);
        }
    }

    void buildTextPixels()
    {
        struct Glyph { char ch; const char* rows[5]; };
        Glyph glyphs[] = {
            { 'L', { "10000", "10000", "10000", "10000", "11111" } },
            { 'E', { "11111", "10000", "11110", "10000", "11111" } },
            { 'G', { "01110", "10000", "10011", "10001", "01110" } },
            { 'I', { "11111", "00100", "00100", "00100", "11111" } },
            { 'O', { "01110", "10001", "10001", "10001", "01110" } },
            { 'N', { "10001", "11001", "10101", "10011", "10001" } },
            { 'S', { "01111", "10000", "01110", "00001", "11110" } },
            { 'T', { "11111", "00100", "00100", "00100", "00100" } },
            { 'A', { "01110", "10001", "11111", "10001", "10001" } },
        };
        int numGlyphs = 9;

        juce::Random rng(42);

        auto addLine = [&](const char* text, int row0)
        {
            int len = static_cast<int>(std::strlen(text));
            int maxLineW = 6 * 6 - 1;
            int totalW = len * 6 - 1;
            int startCol = (maxLineW - totalW) / 2;

            for (int c = 0; c < len; ++c)
            {
                const char* rows[5] = {};
                for (int gi = 0; gi < numGlyphs; ++gi)
                    if (glyphs[gi].ch == text[c])
                        for (int r = 0; r < 5; ++r)
                            rows[r] = glyphs[gi].rows[r];

                for (int r = 0; r < 5; ++r)
                {
                    if (rows[r] == nullptr) continue;
                    for (int p = 0; p < 5; ++p)
                    {
                        if (rows[r][p] == '1')
                        {
                            Pixel px;
                            px.row = row0 + r;
                            px.col = startCol + c * 6 + p;
                            px.startX = rng.nextFloat() * 1920;
                            px.startY = rng.nextFloat() * 1080;
                            px.phase = rng.nextFloat() * 6.28f;
                            textPixels.push_back(px);
                            maxRow = juce::jmax(maxRow, px.row);
                            maxCol = juce::jmax(maxCol, px.col);
                        }
                    }
                }
            }
        };

        addLine("LEGION", 0);
        addLine("STAGE", 7);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SplashComponent)
};
