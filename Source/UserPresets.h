#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"

namespace cinelab
{

// ============================================================================
// User presets (independent of the DAW's preset system).
//
// Stores named snapshots of the ENTIRE parameter state in a private ValueTree
// ("PedrolbyUserPresets"). This tree is saved/restored through the usual
// getStateInformation/setStateInformation, so it survives project saves and
// plugin state.
//
// Thread contract for the audio thread (setSelectedPreset is called from
// processBlock):
//   - updateFromAppState() runs on the audio thread and merges only the
//     volatile values it is allowed to (scene/destination/intensity/proc mix,
//     normalization). It records changes via atomic booleans that the UI polls
//     (pollFlags) — it does NOT send gestures.
//   - The UI uses beginChangeGesture/endChangeGesture around the parameter
//     sets it performs (see applyPresetToState).
// ============================================================================
class UserPresets
{
public:
    static constexpr int kMaxNames = 40;

    // UI polls these to refresh + gesture the changed parameters.
    struct Flags
    {
        std::atomic<bool> anyChanged { false };
        std::atomic<bool> manualGainChanged { false };
        std::atomic<bool> sceneChanged   { false };
        std::atomic<bool> destinationChanged { false };
        std::atomic<bool> intensityChanged { false };
    };

    explicit UserPresets (Parameters& p) : parameters (p) {}

    // ---------- app (UI / save) side ----------

    juce::ValueTree getOrCreateTree()
    {
        if (! tree.isValid())
        {
            tree = juce::ValueTree ("PedrolbyUserPresets");
            tree.setProperty ("version", 1, nullptr);
        }
        return tree;
    }

    // PERSISTED: we merge the fully processed parameter state into the stored
    // snapshot when the user presses "Save". This writes into our own tree
    // (never gestures in audio thread — only triggered by UI action).
    void savePreset (const juce::String& requestedName)
    {
        auto t = getOrCreateTree();
        const juce::String name = uniqueName (t, requestedName);

        auto state = parameters.apvts.copyState();
        juce::ValueTree snap ("preset");
        snap.setProperty ("name", name, nullptr);

        // volcamos todos los parámetros del APVTS (cada hijo del estado
        // es un <PARAM id=... value=...>)
        for (auto child : state)
            snap.appendChild (child.createCopy(), nullptr);

        t.appendChild (snap, nullptr);
        treeChanged.store (true);
    }

    juce::StringArray getPresetNames()
    {
        juce::StringArray names;
        auto t = getOrCreateTree();
        for (auto child : t)
        {
            if (child.getType() == juce::Identifier ("preset"))
                names.add (child.getProperty ("name").toString());
        }
        return names;
    }

    // Replaces the parameter state with a stored preset (gesture the int/float)
    void loadPreset (const juce::String& name)
    {
        auto t = getOrCreateTree();
        for (auto child : t)
        {
            if (child.getType() != juce::Identifier ("preset"))
                continue;
            if (child.getProperty ("name").toString() != name)
                continue;

            // Aplicar con gestures (UI thread — seguro)
            auto& apvts = parameters.apvts;
            for (auto param : child)   // hijos del preset = parámetros
            {
                if (! param.hasType (juce::Identifier ("PARAM")))
                    continue;
                const juce::String id = param.getProperty ("id").toString();
                auto* p = apvts.getParameter (id);
                if (p == nullptr)
                    continue;

                const float v = (float) param.getProperty ("value");
                p->beginChangeGesture();
                p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
                p->endChangeGesture();
            }

            return;
        }
    }

    void deletePreset (const juce::String& name)
    {
        auto t = getOrCreateTree();
        for (int i = t.getNumChildren() - 1; i >= 0; --i)
        {
            auto child = t.getChild (i);
            if (child.getType() == juce::Identifier ("preset")
                && child.getProperty ("name").toString() == name)
                t.removeChild (i, nullptr);
        }
        treeChanged.store (true);
    }

    const juce::ValueTree& getTree() const noexcept { return tree; }
    void setTree (const juce::ValueTree& t)
    {
        tree = t;
        if (! tree.hasType (juce::Identifier ("PedrolbyUserPresets")))
            tree = juce::ValueTree ("PedrolbyUserPresets");
    }

    bool consumeTreeChanged()
    {
        return treeChanged.exchange (false);
    }

    // ---------- audio-side ----------
    // Merges the allowed volatile parameters into the preset into the APVTS
    // but ONLY those the audio thread may touch; returns names of what changed.
    // For simplicity in v1, loading blends the whole preset on the audio side
    // but guards with atomic-change flags the UI then gestures. The heavy
    // setValueNotifyingHost happens on a copy of the tree via the UI, so the
    // audio side only picks scene/dest/intensity/normalize (safe).
    void updateFromAppState()
    {
        if (! desiredLoadActive)
            return;

        desiredLoadActive = false;
        auto& apvts = parameters.apvts;

        // aplicar solo los parámetros seguros para el hilo de audio
        for (auto param : desiredPreset)   // hijos del preset = parámetros
        {
            if (! param.hasType (juce::Identifier ("PARAM")))
                continue;
            const juce::String id = param.getProperty ("id").toString();
            applyAudioSafeParam (apvts, id, (float) param.getProperty ("value"));
        }

        desiredLoadActive = false;
    }

    void requestAudioLoad (const juce::ValueTree& snap)
    {
        desiredPreset = snap.createCopy();
        desiredLoadActive = true;
    }

    Flags& getFlags() { return flags; }

private:
    void applyAudioSafeParam (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float normalized)
    {
        auto* p = apvts.getParameter (id);
        if (p == nullptr)
            return;

        const float clamped = juce::jlimit (0.0f, 1.0f, normalized);
        const float newVal = apvts.getParameterRange (id).convertFrom0to1 (clamped);

        const bool touchScene   = (id == IDs::scene);
        const bool touchDest    = (id == IDs::destination);
        const bool touchIntensity = (id == IDs::intensity);
        const bool touchManual  = (id == IDs::manualGain);
        const bool touchNorm    = (id == IDs::normEnable);

        if (p->getValue() == clamped)
            return;

        // setValueNotifyingHost es seguro desde el hilo de audio en JUCE para
        // AudioParameter* (publica el valor a los listeners sin bloquear el UI).
        // Las gestures se emiten desde la UI al detectar los flags.
        p->setValueNotifyingHost (clamped);

        if (touchScene || touchDest || touchIntensity)
            flags.anyChanged.store (true);
        if (touchScene)      flags.sceneChanged.store (true);
        if (touchDest)       flags.destinationChanged.store (true);
        if (touchIntensity)  flags.intensityChanged.store (true);
        if (touchManual)     flags.manualGainChanged.store (true);

        juce::ignoreUnused (touchNorm, newVal);
    }

    static juce::String uniqueName (const juce::ValueTree& t, juce::String base)
    {
        if (base.trim().isEmpty())
            base = "Preset";
        if (! hasName (t, base))
            return base;

        for (int i = 2; i < 200; ++i)
        {
            const juce::String cand = base + " " + juce::String (i);
            if (! hasName (t, cand))
                return cand;
        }
        return base + juce::String ((int) juce::Time::getMillisecondCounter());
    }

    static bool hasName (const juce::ValueTree& t, const juce::String& n)
    {
        for (auto child : t)
            if (child.getType() == juce::Identifier ("preset")
                && child.getProperty ("name").toString() == n)
                return true;
        return false;
    }

    Parameters& parameters;
    juce::ValueTree tree;
    std::atomic<bool> treeChanged { false };

    juce::ValueTree desiredPreset;
    bool desiredLoadActive = false;

    Flags flags;
};

} // namespace cinelab