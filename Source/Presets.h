#pragma once

#include "Parameters.h"

namespace cinelab
{

// ============================================================================
// Presets creativos. Cada escena es un conjunto de valores del módulo de
// escena, con nombre y descripción en lenguaje humano para la cara Directora.
// "apply" escribe los parámetros (beginGesture/endGesture → automatizable).
// ============================================================================

struct ScenePreset
{
    const char* name;
    const char* humanName;
    const char* blurb;
    double hp, bodyF, bodyG, presF, presG, airG, deEss, comp, width;
};

inline const ScenePreset* getScenePresets()
{
    static const ScenePreset presets[] = {
        { "Dialogue", "Dialogue",
          "Clear in the last row: voice upfront, presence and de-essing.",
          100.0, 250.0, 1.5, 3200.0, 3.5, 0.5, 0.45, 0.55, 0.85 },
        { "SFX", "SFX / Effects",
          "Punchy impacts and details without muddying the lows.",
          40.0, 200.0, 1.0, 3800.0, 1.5, 1.0, 0.15, 0.20, 1.00 },
        { "Foley", "Foley / Steps",
          "Body and texture of prop sounds (steps, cloth, objects).",
          80.0, 250.0, 2.5, 2600.0, 1.5, 0.5, 0.20, 0.15, 0.90 },
        { "Music", "Music",
          "Wide and airy: let the score breathe.",
          30.0, 200.0, 0.5, 4500.0, 0.5, 2.0, 0.00, 0.25, 1.25 },
        { "Ambience", "Ambience",
          "The sound of the world: layers and space, nothing squeezed.",
          60.0, 300.0, 0.5, 3000.0, 0.5, 1.0, 0.00, 0.05, 1.30 },
        { "BoomLFE", "Boom / LFE",
          "Cinema lows: controlled weight, tight for the subwoofer.",
          22.0, 110.0, 3.0, 2500.0, 0.5, 0.0, 0.00, 0.65, 1.00 },
        { "FinalMix", "Final mix",
          "The last-pass filter: glue and fine polish.",
          30.0, 200.0, 0.5, 3200.0, 1.0, 0.5, 0.10, 0.30, 1.05 },
    };

    return presets;
}

inline int getNumScenePresets()
{
    return 7;
}

// ============================================================================
// Room presets (scale the full ISO 2969 X-curve).
// ============================================================================
struct EqPreset
{
    const char* name;
    const char* humanName;
    double roomSize; // curve scale
};

inline const EqPreset* getEqPresets()
{
    static const EqPreset presets[] = {
        { "LargeRoom",    "Large room / IMAX — full X-curve", 1.00 },
        { "MediumRoom",   "Medium room",                      0.75 },
        { "SmallRoom",    "Small room / re-eq to home",       0.45 },
        { "Flat",         "Flat — studio monitoring",         0.00 },
    };

    return presets;
}

inline int getNumEqPresets()
{
    return 4;
}

// ============================================================================
// Aplica un preset de escena a los parámetros del módulo de escena.
// ============================================================================
inline void applyScenePresetToParams (juce::AudioProcessorValueTreeState& apvts, int index)
{
    if (index < 0 || index >= getNumScenePresets())
        return;

    const ScenePreset& p = getScenePresets()[index];

    apvts.getParameter (IDs::scene)->beginChangeGesture();
    apvts.getParameter (IDs::scene)->setValueNotifyingHost (apvts.getParameterRange (IDs::scene).convertTo0to1 ((float) index));
    apvts.getParameter (IDs::scene)->endChangeGesture();

    struct { const juce::String& id; double value; } writes[] = {
        { IDs::sceneHp,       p.hp },
        { IDs::sceneBodyFreq, p.bodyF },
        { IDs::sceneBodyGain, p.bodyG },
        { IDs::scenePresFreq, p.presF },
        { IDs::scenePresGain, p.presG },
        { IDs::sceneAirGain,  p.airG },
        { IDs::sceneDeEss,    p.deEss },
        { IDs::sceneComp,     p.comp },
        { IDs::sceneWidth,    p.width },
    };

    for (auto& w : writes)
    {
        auto* param = apvts.getParameter (w.id);
        if (param == nullptr) continue;
        const auto range = apvts.getParameterRange (w.id);
        param->beginChangeGesture();
        param->setValueNotifyingHost (range.convertTo0to1 (juce::jlimit (range.start, range.end, (float) w.value)));
        param->endChangeGesture();
    }
}

// ============================================================================
// Aplica un preset de sala (curva X) a los parámetros del EQ Cinema.
// ============================================================================
inline void applyEqPresetToParams (juce::AudioProcessorValueTreeState& apvts, int index)
{
    if (index < 0 || index >= getNumEqPresets())
        return;

    const EqPreset& p = getEqPresets()[index];

    auto* param = apvts.getParameter (IDs::eqRoomSize);
    const auto range = apvts.getParameterRange (IDs::eqRoomSize);
    param->beginChangeGesture();
    param->setValueNotifyingHost (range.convertTo0to1 ((float) p.roomSize));
    param->endChangeGesture();
}

} // namespace cinelab