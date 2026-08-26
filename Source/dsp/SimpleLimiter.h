#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace cinelab
{

// ============================================================================
// Limitador de picos (glue estéreo/multicanal) con opción de TRUE PEAK.
//
// - Modo por defecto: limita el pico de MUESTRA (ataque instantáneo por
//   bloque, release suave).
// - Modo "true peak" (oversampling 4×): analiza el pico REAL de la señal
//   reconstruida (BS.1770 / EBU R128 TP) y escala el bloque para que el
//   true peak quede bajo el techo. La ganancia se aplica de forma lineal a
//   todo el bloque, así que el pico reconstruido escala exactamente con ella.
//
// Ganancia común para todos los canales (no mueve la imagen estéreo).
// ============================================================================
class SimpleLimiter
{
public:
    enum { kMaxChannels = 8 };

    void prepare (double sampleRate, int maxBlockSize)
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        preparedMaxBlock = juce::jmax (64, maxBlockSize);
        reset();

        oversamplers.clear();
        for (int c = 0; c < kMaxChannels; ++c)
            oversamplers.push_back (std::make_unique<juce::dsp::Oversampling<float>> (
                1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true));

        for (auto& os : oversamplers)
            os->initProcessing ((size_t) preparedMaxBlock);
    }

    void reset()
    {
        env = 1.0;
        for (auto& os : oversamplers)
            if (os != nullptr) os->reset();
    }

    // ceilingDb: techo negativo, p. ej. -1.0 (dBFS).
    void setCeiling (double ceilingDb)
    {
        ceiling = std::pow (10.0, ceilingDb / 20.0);
    }

    void setReleaseMs (double ms)
    {
        releaseSeconds = ms * 0.001;
    }

    void setTruePeakEnabled (bool enabled) noexcept { tpEnabled = enabled; }

    float getGainReductionDb() const noexcept
    {
        return 20.0f * (float) std::log10 (env);
    }

    void processBlock (float* const* data, int numChannels, int numSamples) noexcept
    {
        if (numChannels == 0 || numSamples == 0)
            return;

        double wanted = 1.0;

        if (tpEnabled)
        {
            // --- pico real (oversampling 4×) de todo el bloque -------------
            if (numSamples > preparedMaxBlock)
            {
                preparedMaxBlock = numSamples;
                for (auto& os : oversamplers)
                    os->initProcessing ((size_t) numSamples);
            }

            double tp = 0.0;
            const int n = juce::jmin (numChannels, (int) kMaxChannels);
            for (int c = 0; c < n; ++c)
            {
                auto& os = *oversamplers[(size_t) c];

                const float* chans[1] = { data[c] };
                const juce::dsp::AudioBlock<const float> blockIn (chans, 1, (size_t) numSamples);
                auto up = os.processSamplesUp (blockIn);

                const float* ud = up.getChannelPointer (0);
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                    tp = juce::jmax (tp, (double) std::abs (ud[i]));
            }

            if (tp > 1.0e-7)
            {
                const double g = ceiling / tp;
                wanted = g < 1.0 ? g : 1.0;
            }

            // release por bloque (la ganancia es constante dentro del bloque)
            const double blockTime = (double) numSamples / sr;
            if (wanted < env)      env = wanted;                    // ataque instantáneo
            else                   env += (1.0 - env) * (1.0 - std::exp (-blockTime / releaseSeconds));

            for (int c = 0; c < numChannels; ++c)
            {
                float* d = data[c];
                for (int i = 0; i < numSamples; ++i)
                    d[i] = (float) (d[i] * env);
            }
        }
        else
        {
            // --- pico de muestra (sample-accurate) --------------------------
            const double relCoef = releaseSeconds > 0.0 ? 1.0 - std::exp (-1.0 / (releaseSeconds * sr)) : 1.0;

            for (int i = 0; i < numSamples; ++i)
            {
                double peak = 0.0;
                for (int c = 0; c < numChannels; ++c)
                    peak = juce::jmax (peak, (double) std::abs (data[c][i]));

                double w = 1.0;
                if (peak > 1.0e-7)
                {
                    const double g = ceiling / peak;
                    w = g < 1.0 ? g : 1.0;
                }

                if (w < env)      env = w;                    // ataque instantáneo
                else              env += (1.0 - env) * relCoef;

                for (int c = 0; c < numChannels; ++c)
                    data[c][i] = (float) (data[c][i] * env);
            }
        }
    }

private:
    double sr = 48000.0;
    double ceiling = 0.891; // -1 dBFS
    double releaseSeconds = 0.180;
    double env = 1.0;
    bool tpEnabled = false;
    int preparedMaxBlock = 0;

    std::vector<std::unique_ptr<juce::dsp::Oversampling<float>>> oversamplers;
};

} // namespace cinelab