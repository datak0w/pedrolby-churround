#pragma once

#include "Biquad.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace cinelab
{

// ============================================================================
// Filtro de ponderación K (ITU-R BS.1770-4): HP 38 Hz + shelf +3.998 dB @ 1.5 kHz
// Coeficientes de diseño tomados del estándar (diseño en tiempo real por
// transformación bilineal, válido para 44.1/48/88.2/96 kHz).
// ============================================================================
class KWeighting
{
public:
    KWeighting()
    {
        hp.reset (48000.0);
        shelf.reset (48000.0);
        hp.setHighPass (38.135470, 0.500327);
        shelf.setHighShelf (1681.974, 3.999844, 0.707175);
    }

    void reset (double sampleRate)
    {
        hp.reset (sampleRate);
        shelf.reset (sampleRate);
        hp.setHighPass (38.135470, 0.500327);
        shelf.setHighShelf (1681.974, 3.999844, 0.707175);
    }

    float process (float x) noexcept
    {
        return shelf.process (hp.process (x));
    }

private:
    Biquad hp, shelf;
};

// ============================================================================
// Medidor de sonoridad LUFS (EBU R128 / ITU BS.1770-4) con ponderación K.
//   - momentáneo:  ventana de 400 ms
//   - corto plazo: media deslizante de ~3 s
//   - integrado:   con gating absoluto (−70 LUFS) y relativo (−10 LUFS),
//                  desde el último reset (máx. 60 min de historia)
//   - pico:        dBFS
// Todo el cómputo ocurre dentro de processBlock(); las lecturas se publican
// en campos atómicos para que la UI las consuma sin bloqueos.
// ============================================================================
class LoudnessMeter
{
public:
    static constexpr double kCalibrationDb = -0.691;
    static constexpr double kAbsoluteGate  = -70.0;
    static constexpr double kRelativeGate  = -10.0;
    static constexpr double kFrameSeconds  = 0.4;
    static constexpr double kShortTermSeconds = 3.2; // 8 ventanas
    static constexpr int    kMaxIntegratedFrames = 9000; // 60 min

    struct Readouts
    {
        std::atomic<float> momentary  { -1000.0f };
        std::atomic<float> shortTerm  { -1000.0f };
        std::atomic<float> integrated { -1000.0f };
        std::atomic<float> peakDb     { -90.0f };
        std::atomic<float> truePeakDb { -1000.0f }; // BS.1770 TP (oversampling 4×)
    };

    void prepare (double sampleRate, int numChannels, int maxBlockSize = 8192)
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        channels = juce::jmax (1, numChannels);
        frameLength = static_cast<double> (kFrameSeconds) * sr;

        filters.resize ((size_t) channels);
        for (auto& k : filters)
            k.reset (sr);

        // pesos BS.1770-4 por defecto (mono/estéreo: todo 1.0).
        // useSurroundWeights() los ajusta según el layout.
        weights.assign ((size_t) channels, 1.0f);

        oversamplers.clear();
        preparedMaxBlock = juce::jmax (64, maxBlockSize);
        for (int c = 0; c < channels; ++c)
            oversamplers.push_back (std::make_unique<juce::dsp::Oversampling<float>> (
                1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true));
        for (auto& os : oversamplers)
            os->initProcessing ((size_t) preparedMaxBlock);

        resetAll();
    }

    // Pesos por canal según ITU-R BS.1770-4: L/R/C = 1.0, surrounds = 1.41,
    // LFE = 0 (excluido). Llamar después de preparar con el layout real.
    void setChannelWeights (const std::vector<float>& w)
    {
        if (w.empty())
            return;
        weights.assign (w.begin(), w.end());
        if ((int) weights.size() < channels)
            weights.resize ((size_t) channels, 1.0f);
    }

    void resetAll()
    {
        for (auto& k : filters)
            k.reset (sr);

        sums.assign ((size_t) channels, 0.0);
        framePos = 0.0;
        peakAbs  = 0.0;
        momentaryRing.clear();
        integratedHist.clear();
        publish (true);
    }

    // Pide que la medición integrada se reinicie en el siguiente bloque.
    void requestIntegratedReset() { resetRequested.store (true); }

    void processBlock (const juce::AudioBuffer<float>& buffer)
    {
        const int numCh  = juce::jmin (channels, buffer.getNumChannels());
        const int numSmp = buffer.getNumSamples();

        if (numCh == 0 || numSmp == 0)
            return;

        if (resetRequested.exchange (false))
        {
            integratedHist.clear();
            momentaryRing.clear();
        }

        // --- true peak (oversampling 4×) del bloque ------------------------
        if (numSmp > preparedMaxBlock)
        {
            preparedMaxBlock = numSmp;
            for (auto& os : oversamplers)
                os->initProcessing ((size_t) numSmp);
        }
        {
            const int n = juce::jmin (numCh, (int) oversamplers.size());
            for (int c = 0; c < n; ++c)
            {
                auto& os = *oversamplers[(size_t) c];
                const float* chans[1] = { buffer.getReadPointer (c) };
                const juce::dsp::AudioBlock<const float> blockIn (chans, 1, (size_t) numSmp);
                auto up = os.processSamplesUp (blockIn);
                const float* ud = up.getChannelPointer (0);
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                    tpAbs = juce::jmax (tpAbs, (double) std::abs (ud[i]));
            }
        }

        // Dividimos el bloque en segmentos que completan ventanas de 400 ms
        int pos = 0;
        while (pos < numSmp)
        {
            const double remaining = frameLength - framePos;
            const int seg = (int) juce::jmin (remaining, (double) (numSmp - pos));

            for (int c = 0; c < numCh; ++c)
            {
                const float* d  = buffer.getReadPointer (c);
                auto& k         = filters[(size_t) c];
                double sum      = sums[(size_t) c];

                for (int i = pos; i < pos + seg; ++i)
                {
                    const float x = d[i];
                    peakAbs = juce::jmax (peakAbs, (double) std::abs (x));
                    const float y = k.process (x);
                    sum += (double) y * (double) y;
                }

                sums[(size_t) c] = sum;
            }

            pos += seg;
            framePos += (double) seg;

            if (framePos >= frameLength)
                finishFrame (numCh);
        }
    }

private:
    void finishFrame (int numCh)
    {
        double totalEnergy = 0.0;
        int counted = 0;
        for (int c = 0; c < numCh; ++c)
        {
            const double e = sums[(size_t) c] / frameLength;
            const double w = (size_t) c < weights.size() ? weights[(size_t) c] : 1.0f;
            totalEnergy += e * w;   // BS.1770-4: surrounds 1.41, LFE 0
            sums[(size_t) c] = 0.0;
            ++counted;
        }

        const double meanEnergy = totalEnergy / (double) counted;
        const double momentary  = meanEnergy > 1.0e-12
                                    ? kCalibrationDb + 10.0 * std::log10 (meanEnergy)
                                    : -1000.0;

        framePos = 0.0;

        // --- short term (media de las últimas N ventanas) ---
        momentaryRing.push_back ((float) momentary);
        if ((int) momentaryRing.size() > (int) (kShortTermSeconds / kFrameSeconds))
            momentaryRing.erase (momentaryRing.begin());

        double stSum = 0.0; int stCount = 0;
        for (auto v : momentaryRing)
            if (v > -900.0f) { stSum += v; ++stCount; }
        const double shortTerm = stCount > 0 ? stSum / stCount : -1000.0;

        // --- integrado con gating (absoluto + relativo) ---
        if (momentary > -900.0)
        {
            integratedHist.push_back ((float) momentary);
            if ((int) integratedHist.size() > kMaxIntegratedFrames)
                integratedHist.erase (integratedHist.begin(), integratedHist.end() - (kMaxIntegratedFrames / 2));
        }

        double integrated = -1000.0;
        double gateSum = 0.0; int gateCount = 0; double gateMean = 0.0;
        for (auto v : integratedHist)
            if (v > kAbsoluteGate) { gateSum += v; ++gateCount; }

        if (gateCount > 0)
        {
            gateMean = gateSum / (double) gateCount;
            const double relGate = gateMean + kRelativeGate;
            double intSum = 0.0; int intCount = 0;
            for (auto v : integratedHist)
                if (v > kAbsoluteGate && v >= relGate) { intSum += v; ++intCount; }
            integrated = intCount > 0 ? intSum / (double) intCount : -1000.0;
        }

        readouts.momentary .store ((float) momentary,  std::memory_order_relaxed);
        readouts.shortTerm .store ((float) shortTerm,  std::memory_order_relaxed);
        readouts.integrated.store ((float) integrated, std::memory_order_relaxed);
        readouts.peakDb    .store ((float) (20.0 * std::log10 (juce::jmax (peakAbs, 1.0e-5))), std::memory_order_relaxed);
        readouts.truePeakDb.store ((float) (20.0 * std::log10 (juce::jmax (tpAbs, 1.0e-5))), std::memory_order_relaxed);

        // los picos se miden por ventana de 400 ms
        peakAbs = 0.0;
        tpAbs = 0.0;
    }

    void publish (bool silent)
    {
        const float s = silent ? -1000.0f : -90.0f;
        readouts.momentary.store (s, std::memory_order_relaxed);
        readouts.shortTerm .store (s, std::memory_order_relaxed);
        readouts.integrated.store (s, std::memory_order_relaxed);
        readouts.peakDb    .store (-90.0f, std::memory_order_relaxed);
        readouts.truePeakDb.store (-1000.0f, std::memory_order_relaxed);
    }

    double sr = 48000.0;
    int channels = 1;
    double frameLength = 19200.0;
    double framePos = 0.0;
    double peakAbs = 0.0;
    double tpAbs = 0.0;

    std::vector<KWeighting> filters;
    std::vector<double> sums;
    std::vector<float> weights;
    std::vector<std::unique_ptr<juce::dsp::Oversampling<float>>> oversamplers;
    int preparedMaxBlock = 0;

    std::vector<float> momentaryRing;
    std::vector<float> integratedHist;
    std::atomic<bool> resetRequested { false };

public:
    Readouts readouts;
};

} // namespace cinelab