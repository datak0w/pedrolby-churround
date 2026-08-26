#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Parameters.h"
#include "Presets.h"
#include "UserPresets.h"
#include "dsp/BassManager.h"
#include "dsp/CinemaEQ.h"
#include "dsp/Downmixer.h"
#include "dsp/LoudnessMeter.h"
#include "dsp/LoudnessNormalizer.h"
#include "dsp/RoomSimulator.h"
#include "dsp/SceneModule.h"
#include "dsp/SimpleLimiter.h"

// ============================================================================
// PeDROLBY Surround — suite de producción y postproducción de audio para cine
// y video. Estéreo y surround (5.1 / 7.1).
//
// Cadena:  Medidor (LUFS/BS.1770, pesos ITU) → [A/B] → EQ Cinema (curva X)
//          → Módulo de escena → Ganancia surround (traseros/LFE)
//          → Normalizador de sonoridad → Limitador → [Simulador de sala]
// ============================================================================
class CineLabAudioProcessor : public juce::AudioProcessor
{
public:
    CineLabAudioProcessor();
    ~CineLabAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "PeDROLBY Churround"; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Un bus por lado (estéreo por defecto). Aceptamos los layouts de
    // masterización multicanal y los discretos de hasta 8 canales.
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        for (auto& set : layouts.inputBuses)
            if (! isSupportedChannelSet (set)) return false;
        for (auto& set : layouts.outputBuses)
            if (! isSupportedChannelSet (set)) return false;
        return true;
    }

    static bool isSupportedChannelSet (const juce::AudioChannelSet& s)
    {
        if (s.isDisabled() || s.isDiscreteLayout() && s.size() <= 8)
            return true;
        return s == juce::AudioChannelSet::mono()
            || s == juce::AudioChannelSet::stereo()
            || s == juce::AudioChannelSet::create5point1()
            || s == juce::AudioChannelSet::create7point1()
            || s == juce::AudioChannelSet::create7point1SDDS();
    }

    // --- API para la UI (sin bloqueos en el hilo de audio) ----------------
    cinelab::Parameters& getParameters() { return parameters; }
    const cinelab::LoudnessMeter::Readouts& getMeterReadouts() const { return meter.readouts; }
    float getLimiterReductionDb() const noexcept { return limiter.getGainReductionDb(); }
    double getNormGainDb() const noexcept { return normalizer.getGainDb(); }
    double getNormAutoDb() const noexcept { return normalizer.getAutoGainDb(); }
    void resetIntegratedMeter() { meter.requestIntegratedReset(); }

    // Aplicar presets (desde la UI); escribe parámetros con gestures.
    void applyScenePreset (int index) { cinelab::applyScenePresetToParams (parameters.apvts, index); }
    void applyEqPreset (int index)   { cinelab::applyEqPresetToParams (parameters.apvts, index); }

    // --- presets de usuario (independientes del DAW) -----------------------
    cinelab::UserPresets& getUserPresets() { return userPresets; }

    bool isSurround() const noexcept { return expectedChannels > 2; }

private:
    void reconfigureChannels (int numChannels);
    void updateDspParameters (int numChannels);

    cinelab::Parameters parameters;
    cinelab::UserPresets userPresets;

    cinelab::LoudnessMeter       meter;
    cinelab::CinemaEQ            cinemaEq;
    cinelab::SceneModule         sceneModule;
    cinelab::LoudnessNormalizer  normalizer;
    cinelab::SimpleLimiter       limiter;
    cinelab::RoomSimulator       roomSim;
    cinelab::Downmixer           downmixer;
    cinelab::BassManager         bassManager;
    int lfeChannelIndex = -1;

    // ganancia de salida por canal (surround trasero y LFE)
    std::vector<float> channelGains;

    double expectedSampleRate = 48000.0;
    int    expectedChannels   = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CineLabAudioProcessor)
};