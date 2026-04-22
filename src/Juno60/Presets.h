#pragma once
#include <string>
#include <vector>

namespace Juno60 {

struct JunoPreset {
    std::string name;
    // LFO
    float lfoRate, lfoDelay;
    // DCO
    float dcoLfo, pwm;
    bool pwmLfo;     // true=LFO, false=Manual
    int range;       // 0=16', 1=8', 2=4'
    bool sawOn, pulseOn, subOn;
    float noise;
    // HPF
    int hpf;         // 0-3
    // VCF
    float vcfFreq, vcfRes, vcfEnv, vcfLfo;
    int keyFollow;   // 0, 1, 2
    // VCA
    bool vcaGate;    // true=Gate, false=Env
    float vcaLevel;
    // ADSR
    float attack, decay, sustain, release;
    // Chorus
    int chorus;      // 0=off, 1=I, 2=II, 3=I+II
};

inline const std::vector<JunoPreset>& getFactoryPresets()
{
    static const std::vector<JunoPreset> presets = {
        // 1. Init Patch — basic saw, mid filter, no chorus
        {
            "Init Patch",
            1.0f, 0.0f,                          // lfoRate, lfoDelay
            0.0f, 0.5f, false, 1,                 // dcoLfo, pwm, pwmLfo, range(8')
            true, false, false,                    // saw, pulse, sub
            0.0f,                                  // noise
            0,                                     // hpf
            8000.0f, 0.0f, 0.0f, 0.0f, 0,         // vcf
            false, 0.8f,                           // vca (env mode)
            0.01f, 0.3f, 0.8f, 0.3f,              // ADSR
            0                                      // chorus
        },
        // 2. Juno Strings — saw+pulse, slow attack, chorus I+II, medium filter
        {
            "Juno Strings",
            0.5f, 0.3f,
            0.1f, 0.55f, true, 1,
            true, true, false,
            0.0f,
            0,
            4500.0f, 0.0f, 0.15f, 0.0f, 1,
            false, 0.85f,
            0.6f, 1.0f, 0.75f, 1.2f,
            3
        },
        // 3. Juno Pad — pulse PWM from LFO, slow attack/release, chorus I+II, low filter
        {
            "Juno Pad",
            0.4f, 0.5f,
            0.0f, 0.65f, true, 1,
            false, true, false,
            0.0f,
            0,
            2500.0f, 0.1f, 0.2f, 0.05f, 0,
            false, 0.8f,
            1.2f, 1.5f, 0.7f, 2.0f,
            3
        },
        // 4. Bass 1 — saw+sub, 16', no chorus, fast env, low filter
        {
            "Bass 1",
            1.0f, 0.0f,
            0.0f, 0.5f, false, 0,
            true, false, true,
            0.0f,
            0,
            1200.0f, 0.15f, 0.45f, 0.0f, 0,
            false, 0.9f,
            0.005f, 0.3f, 0.0f, 0.15f,
            0
        },
        // 5. Bass 2 — pulse+sub, 16', chorus II, punchy env
        {
            "Bass 2",
            1.0f, 0.0f,
            0.0f, 0.4f, false, 0,
            false, true, true,
            0.0f,
            0,
            1500.0f, 0.2f, 0.5f, 0.0f, 0,
            false, 0.9f,
            0.005f, 0.25f, 0.3f, 0.12f,
            2
        },
        // 6. Brass — saw, medium attack, high filter env, chorus I
        {
            "Brass",
            1.0f, 0.0f,
            0.0f, 0.5f, false, 1,
            true, false, false,
            0.0f,
            0,
            2000.0f, 0.0f, 0.7f, 0.0f, 1,
            false, 0.85f,
            0.15f, 0.6f, 0.65f, 0.25f,
            1
        },
        // 7. Lead Sync — saw, 4', high res, no chorus, fast env
        {
            "Lead Sync",
            1.0f, 0.0f,
            0.0f, 0.5f, false, 2,
            true, false, false,
            0.0f,
            0,
            6000.0f, 0.55f, 0.3f, 0.0f, 2,
            false, 0.9f,
            0.005f, 0.2f, 0.7f, 0.15f,
            0
        },
        // 8. Organ — pulse 50%, gate VCA, no chorus
        {
            "Organ",
            1.0f, 0.0f,
            0.0f, 0.5f, false, 1,
            false, true, false,
            0.0f,
            0,
            10000.0f, 0.0f, 0.0f, 0.0f, 1,
            true, 0.8f,
            0.005f, 0.5f, 1.0f, 0.01f,
            0
        },
        // 9. Electric Piano — pulse PWM, medium decay, chorus I
        {
            "Electric Piano",
            0.8f, 0.0f,
            0.0f, 0.45f, true, 1,
            false, true, false,
            0.0f,
            0,
            5000.0f, 0.05f, 0.35f, 0.0f, 1,
            false, 0.8f,
            0.005f, 0.8f, 0.3f, 0.4f,
            1
        },
        // 10. Choir — pulse slow PWM, slow attack, chorus I+II
        {
            "Choir",
            0.3f, 0.8f,
            0.0f, 0.6f, true, 1,
            false, true, false,
            0.05f,
            0,
            3000.0f, 0.0f, 0.1f, 0.05f, 1,
            false, 0.8f,
            0.8f, 1.2f, 0.7f, 1.5f,
            3
        },
        // 11. Synth Pluck — saw, fast attack/decay, low sustain, chorus II
        {
            "Synth Pluck",
            1.0f, 0.0f,
            0.0f, 0.5f, false, 1,
            true, false, false,
            0.0f,
            0,
            6000.0f, 0.1f, 0.55f, 0.0f, 1,
            false, 0.85f,
            0.005f, 0.35f, 0.0f, 0.2f,
            2
        },
        // 12. Sub Bass — sub only, 16', no chorus, fast env
        {
            "Sub Bass",
            1.0f, 0.0f,
            0.0f, 0.5f, false, 0,
            false, false, true,
            0.0f,
            0,
            800.0f, 0.0f, 0.2f, 0.0f, 0,
            false, 0.95f,
            0.005f, 0.4f, 0.6f, 0.15f,
            0
        },
        // 13. Funk Clav — pulse narrow, gate VCA, HPF 2, chorus I
        {
            "Funk Clav",
            1.0f, 0.0f,
            0.0f, 0.15f, false, 1,
            false, true, false,
            0.0f,
            2,
            7000.0f, 0.15f, 0.4f, 0.0f, 2,
            true, 0.85f,
            0.005f, 0.15f, 0.0f, 0.08f,
            1
        },
        // 14. Atmosphere — noise+pulse, slow everything, chorus I+II
        {
            "Atmosphere",
            0.25f, 1.0f,
            0.05f, 0.6f, true, 1,
            false, true, false,
            0.25f,
            0,
            2000.0f, 0.15f, 0.2f, 0.1f, 0,
            false, 0.75f,
            2.0f, 2.5f, 0.6f, 3.0f,
            3
        },
        // 15. Reso Sweep — saw, high resonance, filter env high, chorus II
        {
            "Reso Sweep",
            1.0f, 0.0f,
            0.0f, 0.5f, false, 1,
            true, false, false,
            0.0f,
            0,
            1000.0f, 0.7f, 0.8f, 0.0f, 1,
            false, 0.8f,
            0.1f, 0.8f, 0.3f, 0.5f,
            2
        },
        // 16. Unison Lead — saw+sub, 8', no chorus
        {
            "Unison Lead",
            1.0f, 0.0f,
            0.0f, 0.5f, false, 1,
            true, false, true,
            0.0f,
            0,
            8000.0f, 0.1f, 0.3f, 0.0f, 2,
            false, 0.9f,
            0.005f, 0.3f, 0.7f, 0.2f,
            0
        }
    };
    return presets;
}

} // namespace Juno60
