#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace cinelab
{

// ============================================================================
// Identificadores de parámetros (APVTS). Cada parámetro es automatizable y
// se guarda en presets del DAW.
// ============================================================================
namespace IDs
{
#define CINELAB_DECLARE(name) inline const juce::String name { #name }
    CINELAB_DECLARE (mode);          // Director | Pro (choice)
    CINELAB_DECLARE (scene);         // escena seleccionada (choice, informativa)
    CINELAB_DECLARE (destination);   // destino de entrega (choice)
    CINELAB_DECLARE (intensity);     // 0..1 "cuánto efecto" (Director)

    CINELAB_DECLARE (eqRoomSize);    // escala completa de la curva X
    CINELAB_DECLARE (eqLowGain);     // shelf de graves
    CINELAB_DECLARE (eqLowFreq);
    CINELAB_DECLARE (eqG2k);
    CINELAB_DECLARE (eqG4k);
    CINELAB_DECLARE (eqG8k);
    CINELAB_DECLARE (eqAir);
    CINELAB_DECLARE (eqAirFreq);

    CINELAB_DECLARE (sceneHp);
    CINELAB_DECLARE (sceneBodyFreq);
    CINELAB_DECLARE (sceneBodyGain);
    CINELAB_DECLARE (scenePresFreq);
    CINELAB_DECLARE (scenePresGain);
    CINELAB_DECLARE (sceneAirGain);
    CINELAB_DECLARE (sceneDeEss);
    CINELAB_DECLARE (sceneComp);
    CINELAB_DECLARE (sceneWidth);

    CINELAB_DECLARE (normEnable);
    CINELAB_DECLARE (targetLufs);
    CINELAB_DECLARE (manualGain);

    CINELAB_DECLARE (limEnable);
    CINELAB_DECLARE (limCeiling);

    CINELAB_DECLARE (simEnable);     // room simulator (preview)
    CINELAB_DECLARE (abBypass);      // A/B: dry bypass of processing

    CINELAB_DECLARE (surroundRear);  // rear surround gain (dB)
    CINELAB_DECLARE (lfeLevel);      // LFE gain (dB)

    CINELAB_DECLARE (downmixEnable); // 5.1/7.1 → stereo
    CINELAB_DECLARE (truePeakEnable); // true-peak limiting (oversampled)

    CINELAB_DECLARE (bmEnable);       // bass management (LFE routing)
    CINELAB_DECLARE (bmCrossover);    // crossover Hz (40–300)
    CINELAB_DECLARE (bmLfeGain);      // LFE gain (dB)
    CINELAB_DECLARE (bmSendToLfe);    // sum main-channel low end into LFE
    CINELAB_DECLARE (bmHpMain);       // high-pass mains (low end only on LFE)

    CINELAB_DECLARE (nrEnable);       // spectral noise reduction
    CINELAB_DECLARE (nrAmount);       // NR strength (0..1)
    CINELAB_DECLARE (nrFloor);        // NR max attenuation (dB)

    CINELAB_DECLARE (atmosEnable);    // Atmos-style height upmix
    CINELAB_DECLARE (atmosAmount);    // space/height amount (0..1)
#undef CINELAB_DECLARE
}

// ============================================================================
// English display names for the options (choices and UI).
// ============================================================================
inline const juce::StringArray modeNames          { "Director", "Pro" };
inline const juce::StringArray sceneNames         { "Dialogue", "SFX", "Foley", "Music", "Ambience", "Boom/LFE", "Final mix" };
inline const juce::StringArray destinationNames   { "Cinema", "TV", "Netflix", "Web", "Podcast", "Manual" };

// ============================================================================
// Range helpers (log-ish skew for frequencies). In JUCE 9 the 5th ctor arg
// of NormalisableRange is `bool symmetricSkew` (no default-value slot), so
// the repeated `def` parameter exists only for call-site compatibility.
// ============================================================================
inline juce::NormalisableRange<float> freqRange (float min, float max, float = 0.0f)
{
    return { min, max, 0.01f, 0.3f };
}

inline juce::NormalisableRange<float> linRange (float min, float max, float = 0.0f)
{
    return { min, max, 0.001f, 1.0f };
}

// ============================================================================
// Construye el layout completo de parámetros del plugin.
// ============================================================================
struct Parameters
{
    explicit Parameters (juce::AudioProcessor& owner)
        : apvts (owner, nullptr, "CineLabParams", createParameterLayout())
    {
    }

    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        layout.add (std::make_unique<juce::AudioParameterChoice> (IDs::mode, "Mode", modeNames, 0));
        layout.add (std::make_unique<juce::AudioParameterChoice> (IDs::scene, "Scene", sceneNames, 0));
        layout.add (std::make_unique<juce::AudioParameterChoice> (IDs::destination, "Delivery target", destinationNames, 0));
        layout.add (std::make_unique<juce::AudioParameterFloat>  (IDs::intensity, "Intensity", linRange (0.0f, 1.0f, 1.0f), 1.0f));

        // Cinema EQ
        layout.add (std::make_unique<juce::AudioParameterFloat> (IDs::eqRoomSize, "Room size", linRange (0.0f, 1.0f, 1.0f), 1.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (IDs::eqLowGain,  "Low shelf (X-curve)", linRange (-8.0f, 0.0f, -4.0f), -4.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (IDs::eqLowFreq,  "Low freq", freqRange (40.0f, 400.0f, 100.0f), 100.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (IDs::eqG2k, "X-curve 2 kHz", linRange (-3.0f, 9.0f, 1.0f), 1.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (IDs::eqG4k, "X-curve 4 kHz", linRange (-3.0f, 9.0f, 3.5f), 3.5f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (IDs::eqG8k, "X-curve 8 kHz", linRange (-3.0f, 12.0f, 6.0f), 6.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (IDs::eqAir, "Air (10 kHz)", linRange (-6.0f, 6.0f, 1.0f), 1.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (IDs::eqAirFreq, "Air freq", freqRange (6000.0f, 16000.0f, 10000.0f), 10000.0f));

        // Scene
        layout.add (std::make_unique<juce::AudioParameterFloat> (IDs::sceneHp, "Scene high-pass", freqRange (20.0f, 240.0f, 30.0f), 30.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (IDs::sceneBodyFreq, "Body freq", freqRange (100.0f, 900.0f, 250.0f), 250.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (IDs::sceneBodyGain, "Body gain", linRange (-6.0f, 9.0f, 0.0f), 0.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (IDs::scenePresFreq, "Presence freq", freqRange (1500.0f, 8000.0f, 3200.0f), 3200.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (IDs::scenePresGain, "Presence gain", linRange (-4.0f, 9.0f, 2.0f), 2.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (IDs::sceneAirGain, "Scene air", linRange (-6.0f, 6.0f, 0.0f), 0.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (IDs::sceneDeEss, "De-esser", linRange (0.0f, 1.0f, 0.0f), 0.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (IDs::sceneComp, "Glue compression", linRange (0.0f, 1.0f, 0.0f), 0.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (IDs::sceneWidth, "Width", linRange (0.0f, 2.0f, 1.0f), 1.0f));

        // Normalization
        layout.add (std::make_unique<juce::AudioParameterBool> (IDs::normEnable, "Normalize", true));
        layout.add (std::make_unique<juce::AudioParameterFloat> (IDs::targetLufs, "Target LUFS", linRange (-36.0f, -8.0f, -24.0f), -24.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (IDs::manualGain, "Manual gain", linRange (-12.0f, 12.0f, 0.0f), 0.0f));

        // Limiter
        layout.add (std::make_unique<juce::AudioParameterBool> (IDs::limEnable, "Limiter", true));
        layout.add (std::make_unique<juce::AudioParameterFloat> (IDs::limCeiling, "Ceiling (dBFS)", linRange (-6.0f, 0.0f, -1.0f), -1.0f));

        // Monitoring / preview
        layout.add (std::make_unique<juce::AudioParameterBool> (IDs::simEnable, "Room simulator", false));
        layout.add (std::make_unique<juce::AudioParameterBool> (IDs::abBypass,  "A/B dry", false));

        // Surround (multichannel mastering)
        layout.add (std::make_unique<juce::AudioParameterFloat> (IDs::surroundRear, "Rear surrounds", linRange (-12.0f, 12.0f, 0.0f), 0.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (IDs::lfeLevel,     "LFE",               linRange (-12.0f, 12.0f, 0.0f), 0.0f));

        // Delivery
        layout.add (std::make_unique<juce::AudioParameterBool> (IDs::downmixEnable, "Downmix 5.1→2.0", false));
        layout.add (std::make_unique<juce::AudioParameterBool> (IDs::truePeakEnable, "True peak (oversampled)", true));

        // Bass management
        layout.add (std::make_unique<juce::AudioParameterBool>  (IDs::bmEnable,    "Bass management", false));
        layout.add (std::make_unique<juce::AudioParameterFloat> (IDs::bmCrossover, "Bass crossover Hz", freqRange (40.0f, 300.0f, 80.0f), 80.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (IDs::bmLfeGain,   "Bass LFE gain", linRange (-12.0f, 12.0f, 0.0f), 0.0f));
        layout.add (std::make_unique<juce::AudioParameterBool>  (IDs::bmSendToLfe, "Send low end to LFE", true));
        layout.add (std::make_unique<juce::AudioParameterBool>  (IDs::bmHpMain,    "High-pass mains", false));

        // Noise reduction (spectral, outdoor dialogue)
        layout.add (std::make_unique<juce::AudioParameterBool>  (IDs::nrEnable, "Noise reduction", false));
        layout.add (std::make_unique<juce::AudioParameterFloat> (IDs::nrAmount, "NR strength", linRange (0.0f, 1.0f, 0.8f), 0.8f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (IDs::nrFloor,  "NR floor dB", linRange (-60.0f, -12.0f, -40.0f), -40.0f));

        // Atmos-style height upmix
        layout.add (std::make_unique<juce::AudioParameterBool>  (IDs::atmosEnable, "Atmos upmix", false));
        layout.add (std::make_unique<juce::AudioParameterFloat> (IDs::atmosAmount, "Atmos amount", linRange (0.0f, 1.0f, 0.35f), 0.35f));

        return layout;
    }

    float getFloat (const juce::String& id) const noexcept
    {
        if (auto* v = apvts.getRawParameterValue (id))
            return v->load();

        return 0.0f;
    }
};

} // namespace cinelab