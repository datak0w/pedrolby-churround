#include "PluginProcessor.h"
#include "Presets.h"
#include "PluginEditor.h"
#include "dsp/DeliveryStandards.h"

#include <juce_audio_utils/juce_audio_utils.h>

using namespace juce;
using namespace cinelab;

CineLabAudioProcessor::CineLabAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  AudioChannelSet::stereo(), true)
                        .withOutput ("Output", AudioChannelSet::stereo(), true)),
      parameters (*this), userPresets (parameters)
{
}

// ============================================================================

namespace
{
    // Pesos y tipos por canal según BS.1770-4 / necesidades de mastering.
    float ituWeightFor (AudioChannelSet::ChannelType type)
    {
        switch (type)
        {
            case AudioChannelSet::LFE:
            case AudioChannelSet::LFE2:
                return 0.0f;
            case AudioChannelSet::leftSurround:
            case AudioChannelSet::rightSurround:
            case AudioChannelSet::centreSurround:
            case AudioChannelSet::leftSurroundSide:
            case AudioChannelSet::rightSurroundSide:
            case AudioChannelSet::leftSurroundRear:
            case AudioChannelSet::rightSurroundRear:
                return 1.41f;
            default:
                return 1.0f;
        }
    }

    bool isSurroundChannel (AudioChannelSet::ChannelType type)
    {
        switch (type)
        {
            case AudioChannelSet::leftSurround:
            case AudioChannelSet::rightSurround:
            case AudioChannelSet::centreSurround:
            case AudioChannelSet::leftSurroundSide:
            case AudioChannelSet::rightSurroundSide:
            case AudioChannelSet::leftSurroundRear:
            case AudioChannelSet::rightSurroundRear:
                return true;
            default:
                return false;
        }
    }

    bool isLfeChannel (AudioChannelSet::ChannelType type)
    {
        return type == AudioChannelSet::LFE || type == AudioChannelSet::LFE2;
    }
}

// ============================================================================

void CineLabAudioProcessor::reconfigureChannels (int numChannels)
{
    expectedChannels = juce::jmax (1, numChannels);
    const int n = expectedChannels;

    meter.prepare (expectedSampleRate, n);
    cinemaEq.prepare (expectedSampleRate, n);
    sceneModule.prepare (expectedSampleRate, n);
    roomSim.prepare (expectedSampleRate, n);
    noiseReduction.prepare (expectedSampleRate, n);
    atmosUpmix.prepare (expectedSampleRate, n);

    // pesos de medición + ganancias de mastering por canal
    auto layout = getChannelLayoutOfBus (true, 0);
    std::vector<float> weights ((size_t) n, 1.0f);
    channelGains.assign ((size_t) n, 1.0f);

    // canales de altura (para Atmos upmix)
    std::vector<int> heightIdx;
    for (int c = 0; c < n; ++c)
    {
        auto t = layout.getTypeOfChannel (c);
        if (t == AudioChannelSet::topSideLeft  || t == AudioChannelSet::topSideRight
         || t == AudioChannelSet::topFrontLeft || t == AudioChannelSet::topFrontRight
         || t == AudioChannelSet::topRearLeft  || t == AudioChannelSet::topRearRight
         || t == AudioChannelSet::topMiddle)
            heightIdx.push_back (c);
    }
    atmosUpmix.setHeightIndices (heightIdx);

    const float rearDb  = parameters.getFloat (IDs::surroundRear);
    const float lfeDb   = parameters.getFloat (IDs::lfeLevel);

    for (int c = 0; c < n; ++c)
    {
        auto type = layout.getTypeOfChannel (c);

        if ((size_t) c < weights.size())
            weights[(size_t) c] = ituWeightFor (type);

        float g = 1.0f;
        if (isSurroundChannel (type)) g *= (float) std::pow (10.0, rearDb / 20.0);
        if (isLfeChannel (type))      g *= (float) std::pow (10.0, lfeDb / 20.0);
        channelGains[(size_t) c] = g;
    }

    meter.setChannelWeights (weights);

    // localiza el canal LFE del layout actual (para bass management)
    lfeChannelIndex = -1;
    for (int c = 0; c < n; ++c)
        if (isLfeChannel (layout.getTypeOfChannel (c)))
        {
            lfeChannelIndex = c;
            break;
        }

    bassManager.prepare (expectedSampleRate, n, lfeChannelIndex);
}

void CineLabAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    expectedSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    const int numCh = juce::jmax (1, getTotalNumInputChannels());

    reconfigureChannels (numCh);

    normalizer.reset (expectedSampleRate);
    limiter.prepare (expectedSampleRate, juce::jmax (64, samplesPerBlock));

    updateDspParameters (numCh);

    juce::ignoreUnused (samplesPerBlock);
}

void CineLabAudioProcessor::releaseResources()
{
    meter.resetAll();
}

// ============================================================================

void CineLabAudioProcessor::updateDspParameters (int)
{
    auto& vts = parameters.apvts;

    // --- Modo Directora: intensidad escala las cantidades creativas -------
    const bool directorMode = vts.getRawParameterValue (IDs::mode)->load() < 0.5f;
    const float intensity = vts.getRawParameterValue (IDs::intensity)->load();
    const double sceneScale = directorMode ? (0.10 + 0.90 * (double) intensity) : 1.0;

    // --- EQ Cinema ---------------------------------------------------------
    CinemaEQ::Params eq;
    eq.roomSize    = vts.getRawParameterValue (IDs::eqRoomSize)->load();
    eq.lowGainDb   = vts.getRawParameterValue (IDs::eqLowGain)->load()  * sceneScale;
    eq.lowFreq     = vts.getRawParameterValue (IDs::eqLowFreq)->load();
    eq.g2kDb       = vts.getRawParameterValue (IDs::eqG2k)->load()      * sceneScale;
    eq.g4kDb       = vts.getRawParameterValue (IDs::eqG4k)->load()      * sceneScale;
    eq.g8kDb       = vts.getRawParameterValue (IDs::eqG8k)->load()      * sceneScale;
    eq.airGainDb   = vts.getRawParameterValue (IDs::eqAir)->load()      * sceneScale;
    eq.airFreq     = vts.getRawParameterValue (IDs::eqAirFreq)->load();
    cinemaEq.setParams (eq);

    // --- Módulo de escena --------------------------------------------------
    SceneModule::Params scene;
    scene.hpFreq         = vts.getRawParameterValue (IDs::sceneHp)->load();
    scene.bodyFreq       = vts.getRawParameterValue (IDs::sceneBodyFreq)->load();
    scene.bodyGainDb     = vts.getRawParameterValue (IDs::sceneBodyGain)->load() * sceneScale;
    scene.presenceFreq   = vts.getRawParameterValue (IDs::scenePresFreq)->load();
    scene.presenceGainDb = vts.getRawParameterValue (IDs::scenePresGain)->load() * sceneScale;
    scene.airGainDb      = vts.getRawParameterValue (IDs::sceneAirGain)->load()  * sceneScale;
    scene.deEss          = vts.getRawParameterValue (IDs::sceneDeEss)->load()    * sceneScale;
    scene.compAmount     = vts.getRawParameterValue (IDs::sceneComp)->load()     * sceneScale;
    scene.width          = vts.getRawParameterValue (IDs::sceneWidth)->load();
    sceneModule.setParams (scene);

    // --- Normalización / destino -------------------------------------------
    const int destIndex = juce::jlimit (0, 5, (int) vts.getRawParameterValue (IDs::destination)->load());
    const auto dest = static_cast<Destination> (destIndex);
    const DeliveryStandard& std = deliveryStandardFor (dest);

    const double target = dest == Destination::manual
                            ? vts.getRawParameterValue (IDs::targetLufs)->load()
                            : std.targetLufs;

    normalizer.setTarget (target);
    normalizer.setEnabled (vts.getRawParameterValue (IDs::normEnable)->load() > 0.5f);
    normalizer.setManualGain (vts.getRawParameterValue (IDs::manualGain)->load());

    // --- Limitador ----------------------------------------------------------
    const double ceiling = dest == Destination::manual
                             ? vts.getRawParameterValue (IDs::limCeiling)->load()
                             : std.ceilingDb;
    limiter.setCeiling (ceiling);
    limiter.setReleaseMs (180.0);
    limiter.setTruePeakEnabled (vts.getRawParameterValue (IDs::truePeakEnable)->load() > 0.5f);

    // --- Simulador de sala (usa el mismo tamaño de sala de la curva X) ------
    RoomSimulator::Params sim;
    sim.roomAmount = vts.getRawParameterValue (IDs::eqRoomSize)->load();
    roomSim.setParams (sim);

    // --- Noise reduction (espectral, diálogos de exterior) ------------------
    NoiseReduction::Params nr;
    nr.enabled   = vts.getRawParameterValue (IDs::nrEnable)->load() > 0.5f;
    nr.amount    = vts.getRawParameterValue (IDs::nrAmount)->load();
    nr.floorDb   = vts.getRawParameterValue (IDs::nrFloor)->load();
    noiseReduction.setParams (nr);

    // latencia dinámica (el host la compensa al cambiar)
    const int lat = noiseReduction.isActive() ? noiseReduction.getLatencySamples() : 0;
    if (lat != reportedLatency)
    {
        reportedLatency = lat;
        setLatencySamples (lat);
    }

    // --- Atmos upmix (alturas) ----------------------------------------------
    AtmosUpmix::Params atm;
    atm.enabled = vts.getRawParameterValue (IDs::atmosEnable)->load() > 0.5f;
    atm.amount  = vts.getRawParameterValue (IDs::atmosAmount)->load();
    atmosUpmix.setParams (atm);

    // --- Bass management -----------------------------------------------------
    BassManager::Params bm;
    bm.crossoverHz = vts.getRawParameterValue (IDs::bmCrossover)->load();
    bm.lfeGainDb   = vts.getRawParameterValue (IDs::bmLfeGain)->load();
    bm.sendToLfe   = vts.getRawParameterValue (IDs::bmSendToLfe)->load() > 0.5f;
    bm.hpMain      = vts.getRawParameterValue (IDs::bmHpMain)->load() > 0.5f;
    bassManager.setParams (bm);
}

// ============================================================================

void CineLabAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int numSmp = buffer.getNumSamples();

    if (numCh == 0 || numSmp == 0)
        return;

    // Cambio de layout multicanal (REAPER re-negocia buses)
    if (numCh != expectedChannels)
        reconfigureChannels (numCh);

    // 1) Medición (siempre sobre la entrada)
    meter.processBlock (buffer);

    // 2) Aplicar "carga" audio-safe de un preset de usuario pendiente
    userPresets.updateFromAppState();

    // 3) A/B: modo "original" = paso directo (salida = entrada, ya en el buffer)
    if (parameters.apvts.getRawParameterValue (IDs::abBypass)->load() > 0.5f)
        return;

    // 4) Parámetros → DSP
    updateDspParameters (numCh);

    float* const* data = buffer.getArrayOfWritePointers();

    // 4b) Noise reduction espectral (diálogos de exterior) — lo primero,
    //     antes de cualquier ecualización
    if (parameters.apvts.getRawParameterValue (IDs::nrEnable)->load() > 0.5f
        && noiseReduction.isActive())
        noiseReduction.processBlock (buffer);

    // 5) EQ Cinema (curva X)
    if (cinemaEq.isActive())
        cinemaEq.process (data, numCh, numSmp);

    // 5) Módulo de escena
    sceneModule.processBlock (buffer);

    // 6) Ganancia multicanal (rear surrounds / LFE) — "mastering"
    if ((int) channelGains.size() >= numCh)
    {
        for (int c = 0; c < numCh; ++c)
        {
            const float g = channelGains[(size_t) c];
            if (g == 1.0f)
                continue;
            float* d = data[c];
            for (int i = 0; i < numSmp; ++i)
                d[i] *= g;
        }
    }

    // 6b) Bass management (gestión de graves / LFE) — antes del downmix
    if (parameters.apvts.getRawParameterValue (IDs::bmEnable)->load() > 0.5f && bassManager.isActive())
        bassManager.process (data, numSmp);

    // 7) Downmix 5.1/7.1 → estéreo (print de entrega; antes del limitador
    //    para que la protección cubra también la suma)
    if (parameters.apvts.getRawParameterValue (IDs::downmixEnable)->load() > 0.5f)
        downmixer.process (data, numCh, numSmp);

    // 8) Normalizador de sonoridad
    normalizer.update (meter.readouts.shortTerm.load (std::memory_order_relaxed));
    normalizer.processBlock (data, numCh, numSmp);

    // 9) Limitador de picos (con true-peak si está activado)
    if (parameters.apvts.getRawParameterValue (IDs::limEnable)->load() > 0.5f)
        limiter.processBlock (data, numCh, numSmp);

    // 10) Atmos upmix (alturas decorreladas desde L/R) — después del
    //     limitador para no disparar el techo; sin latencia
    if (parameters.apvts.getRawParameterValue (IDs::atmosEnable)->load() > 0.5f
        && atmosUpmix.isActive())
        atmosUpmix.process (data, numCh, numSmp);

    // 11) Simulador de sala (monitoring aid, after the limiter)
    if (parameters.apvts.getRawParameterValue (IDs::simEnable)->load() > 0.5f && roomSim.isActive())
        roomSim.process (data, numCh, numSmp);
}

// ============================================================================

juce::AudioProcessorEditor* CineLabAudioProcessor::createEditor()
{
    return new CineLabAudioProcessorEditor (*this, parameters);
}

// ============================================================================

void CineLabAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.apvts.copyState();
    state.setProperty ("userPresets", userPresets.getOrCreateTree().toXmlString(), nullptr);
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void CineLabAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (parameters.apvts.state.getType()))
    {
        auto tree = juce::ValueTree::fromXml (*xml);

        // restaurar presets de usuario (propiedad serializada en el root)
        if (tree.hasProperty ("userPresets"))
        {
            auto presetsXml = juce::XmlDocument::parse (tree.getProperty ("userPresets").toString());
            if (presetsXml != nullptr && presetsXml->hasTagName ("PedrolbyUserPresets"))
            {
                auto t = juce::ValueTree::fromXml (*presetsXml);
                userPresets.setTree (t);
            }
            tree.removeProperty ("userPresets", nullptr);
        }

        parameters.apvts.replaceState (tree);
    }
}

// ============================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CineLabAudioProcessor();
}