#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace cinelab
{

// ============================================================================
// AtmosUpmix — versión básica de "altura" (height) para sonido envolvente.
//
// Genera un contenido decorrelado tipo "techo" a partir de L/R:
//   - cadena de 3 allpass con coeficientes distintos por canal + paso alto
//     (~300 Hz) → solo el "aire" (información de alta/media frecuencia)
//     pierde la localización y gana amplitud espacial.
//   - Sin latencia (allpass de fase, sin retardos).
//
// Rutas (según el layout negociado):
//   - layout con canales de altura (7.1.2, 7.1.4, discretos indicados):
//     el aire decorrelado se escribe (escalado) en los canales TOP.
//   - estéreo (2 canales): mezcla un nivel sutil del aire decorrelado de
//     vuelta en L/R → ensanchamiento psicofísico (sin canales de altura).
//   - 5.1 sin alturas: no añade nada (no hay dónde poner el techo).
// ============================================================================
class AtmosUpmix
{
public:
    struct Params
    {
        bool   enabled = false;
        double amount  = 0.35;   // 0..1
    };

    void prepare (double sampleRate, int numChannels)
    {
        sr = sampleRate;
        channels = numChannels;

        apA.assign ((size_t) numChannels, State());
        apB.assign ((size_t) numChannels, State());
        apC.assign ((size_t) numChannels, State());
        hp.assign ((size_t) numChannels, HP());

        for (int c = 0; c < numChannels; ++c)
        {
            // coeficientes allpass distintos por canal (decorrelación)
            apA[(size_t) c].coef = allpassCoef (c * 0.17 + 0.11);
            apB[(size_t) c].coef = allpassCoef (c * 0.23 + 0.31);
            apC[(size_t) c].coef = allpassCoef (c * 0.29 + 0.47);
            hp[(size_t) c].prepare (sr, 320.0);
        }

        heightIndices.clear();
        params = Params();
    }

    // Índices de los canales de altura del layout actual (si existen)
    void setHeightIndices (const std::vector<int>& idx)
    {
        heightIndices = idx;
    }

    void setParams (const Params& p) { params = p; }

    bool isActive() const noexcept { return params.enabled && (channels == 2 || ! heightIndices.empty()); }

    void process (float* const* data, int numChannels, int numSamples) noexcept
    {
        if (! params.enabled || numChannels < 2 || numSamples == 0)
            return;

        const float amount = (float) params.amount;

        // --- contenido decorrelado de L/R ------------------------------------
        float* l = data[0];
        float* r = data[1];

        // buffers temporales mono
        if ((int) tmpL.size() < numSamples) { tmpL.resize (numSamples); tmpR.resize (numSamples); }

        for (int i = 0; i < numSamples; ++i)
        {
            const float xL = l[i], xR = r[i];

            tmpL[i] = allpassChain (apA[0], apB[0], apC[0], hp[0], xL);
            tmpR[i] = allpassChain (apA[1], apB[1], apC[1], hp[1], xR);
        }

        if (channels == 2)
        {
            // ensanche sutil: mezclar el aire decorrelado de vuelta
            const float mix = 0.15f * amount;
            for (int i = 0; i < numSamples; ++i)
            {
                l[i] += tmpL[i] * mix;
                r[i] += tmpR[i] * mix;
            }
            return;
        }

        // --- escribir en canales de altura ----------------------------------
        const float gain = 0.8f * amount + 0.05f;
        if (heightIndices.size() >= 2 && heightIndices[0] < numChannels && heightIndices[1] < numChannels)
        {
            float* hL = data[heightIndices[0]];
            float* hR = data[heightIndices[1]];
            for (int i = 0; i < numSamples; ++i)
            {
                hL[i] = tmpL[i] * gain;
                hR[i] = tmpR[i] * gain;
            }
        }
        else if (heightIndices.size() == 1 && heightIndices[0] < numChannels)
        {
            float* hM = data[heightIndices[0]];
            for (int i = 0; i < numSamples; ++i)
                hM[i] = (tmpL[i] + tmpR[i]) * 0.5f * gain * 1.41f;
        }
    }

private:
    struct State { float coef = 0.0f; float z = 0.0f; };

    struct HP
    {
        float b0=0,b1=0,b2=0,a1=0,a2=0;
        float z1=0,z2=0;

        void prepare (double sr, double f)
        {
            const double w0 = 2.0 * juce::MathConstants<double>::pi * f / sr;
            const double cw = std::cos (w0), sw = std::sin (w0);
            const double alpha = sw / (2.0 * 0.7071);
            const double a0 = 1.0 + alpha;
            b0 = (float) ((1.0 + cw) / (2.0 * a0));
            b1 = (float) (-(1.0 + cw) / a0);
            b2 = b0;
            a1 = (float) (-2.0 * cw / a0);
            a2 = (float) ((1.0 - alpha) / a0);
        }

        float process (float x) noexcept
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    static float allpassCoef (double ph)
    {
        // ~0.35–0.55, repartido
        return (float) (0.30 + 0.25 * std::abs (std::sin (ph * 12.9898)));
    }

    static float allpass (State& s, float x) noexcept
    {
        const float y = s.coef * x + s.z;
        s.z = x - s.coef * y;
        return y;
    }

    static float allpassChain (State& a, State& b, State& c, HP& hp, float x) noexcept
    {
        return allpass (c, allpass (b, allpass (a, hp.process (x))));
    }

    double sr = 48000.0;
    int channels = 2;
    Params params;
    std::vector<State> apA, apB, apC;
    std::vector<HP> hp;
    std::vector<int> heightIndices;
    std::vector<float> tmpL, tmpR;
};

} // namespace cinelab