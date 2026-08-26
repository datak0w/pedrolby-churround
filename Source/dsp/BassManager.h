#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace cinelab
{

// ============================================================================
// Biquad de 2º orden (usado como célula del crossover Linkwitz-Riley).
// ============================================================================
class BM_Biquad
{
public:
    void reset (double sampleRate)
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        clear();
    }

    void clear() noexcept
    {
        z1 = 0.0; z2 = 0.0;
    }

    void makeLowPass (double freq)
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi * freq / sr;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * 0.7071); // Butterworth Q
        const double b0 = (1.0 - cw) / 2.0;
        const double b1 = 1.0 - cw;
        const double b2 = b0;
        const double a0 = 1.0 + alpha;
        const double a1 = -2.0 * cw;
        const double a2 = 1.0 - alpha;
        setCoeffs (b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
    }

    void makeHighPass (double freq)
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi * freq / sr;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * 0.7071);
        const double b0 = (1.0 + cw) / 2.0;
        const double b1 = -(1.0 + cw);
        const double b2 = b0;
        const double a0 = 1.0 + alpha;
        const double a1 = -2.0 * cw;
        const double a2 = 1.0 - alpha;
        setCoeffs (b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
    }

    float process (float x) noexcept
    {
        const double y = b0 * (double) x + z1;
        z1 = b1 * (double) x - a1 * y + z2;
        z2 = b2 * (double) x - a2 * y;
        return (float) y;
    }

private:
    void setCoeffs (double b0_, double b1_, double b2_, double a1_, double a2_) noexcept
    {
        b0 = b0_; b1 = b1_; b2 = b2_; a1 = a1_; a2 = a2_;
    }

    double sr = 48000.0;
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;
};

// ============================================================================
// Bass management / gestión de graves-LFE.
//
// Cuando el material tiene un canal LFE (5.1/7.1), el bajo suele necesitar
// manejo: el crossover acumula los graves de todos los canales principales
// y los envía al LFE, y opcionalmente baja el "grueso" a los canales
// principales para que cada canal trabaje sin esfuerzo por debajo del
// crossover.
//
//   - crossoverHz : frecuencia del crossover (Linkwitz-Riley 4º orden,
//                   dos células de 2º orden en cascada).
//   - lfeGainDb   : ganancia del LFE (además del nivel normal).
//   - sendToLfe   : si true, suma los graves por debajo del crossover de
//                   todos los canales principales al LFE.
//   - hpMain      : si true, quita los graves de los canales principales
//                   (el bajo sale por el LFE; útil con subwoofer).
//
// En estéreo/mono (sin LFE) el módulo se desactiva solo: nada que hacer.
// ============================================================================
class BassManager
{
public:
    struct Params
    {
        double crossoverHz = 80.0;
        double lfeGainDb   = 0.0;
        bool   sendToLfe   = false;
        bool   hpMain      = false;
    };

    void prepare (double sampleRate, int numChannels, int lfeIndex = -1)
    {
        sr = sampleRate;
        channels = numChannels;
        lfeChannel = lfeIndex;

        lpHpFilters.assign ((size_t) numChannels, Cell());
        for (auto& c : lpHpFilters)
            c.prepare (sr);

        lfeSum.assign ((size_t) numChannels, 0.0);
        params = Params();
        needsUpdate = true;
    }

    void setParams (const Params& p)
    {
        params = p;
        needsUpdate = true;
    }

    bool isActive() const noexcept
    {
        return lfeChannel >= 0 && lfeChannel < channels && params.sendToLfe;
    }

    // process: procesa el bloque multicanal in-place.
    //   lfeGain y sendToLfe se aplican al canal LFE.
    void process (float* const* data, int numSamples) noexcept
    {
        if (! isActive())
            return;

        if (needsUpdate)
        {
            const double co = juce::jlimit (40.0, 300.0, params.crossoverHz);
            for (auto& cell : lpHpFilters)
                cell.update (co);
            lfeScale = (float) std::pow (10.0, params.lfeGainDb / 20.0);
            needsUpdate = false;
        }

        const int nMain = juce::jmin (channels, lfeChannel); // canales antes del LFE
        const int n = channels;

        // 1) paso de análisis: acumular graves de los canales principales
        for (int c = 0; c < (int) lpHpFilters.size() && c < n; ++c)
        {
            if (c == lfeChannel)
                continue;
            float* d = data[c];
            // escaneo de IDENTIFICACIÓN: no se reescribe aquí; el filtro se
            // aplica a la suma en el segundo paso.
            lfeSum[(size_t) c] = 0.0;
        }

        // 2) procesar cada canal: filtrar y acumular transversales
        {
            // acumulador mono del contenido graves de todos los principales
            juce::AudioBuffer<float> bassAcc (1, numSamples);
            bassAcc.clear();

            for (int c = 0; c < n; ++c)
            {
                if (c == lfeChannel)
                    continue;
                auto& cell = lpHpFilters[(size_t) c];
                float* d = data[c];

                for (int i = 0; i < numSamples; ++i)
                {
                    const float x = d[i];
                    // LP (para el LFE) y HP (para los principales) independientes:
                    // x − LP NO es un HP válido (la fase del LP4 ya gira −90° a
                    // una octava del crossover), así que usamos filtros HP reales.
                    const float lp = cell.lp.process (cell.lp2.process (x));
                    bassAcc.addSample (0, i, lp);
                    if (params.hpMain)
                        d[i] = cell.hp.process (cell.hp2.process (x));
                }
            }

            // 3) agregar al LFE
            float* lfe = data[lfeChannel];
            for (int i = 0; i < numSamples; ++i)
            {
                const float sum = bassAcc.getSample (0, i);
                lfe[i] = (float) (lfe[i] * lfeScale + sum);
            }
        }
    }

private:
    struct Cell
    {
        BM_Biquad lp, lp2, hp, hp2;

        void prepare (double sr)
        {
            lp.reset (sr); lp2.reset (sr); hp.reset (sr); hp2.reset (sr);
        }

        void update (double freq)
        {
            lp.makeLowPass (freq);  lp2.makeLowPass (freq);
            hp.makeHighPass (freq); hp2.makeHighPass (freq);
        }
    };

    double sr = 48000.0;
    int channels = 2;
    int lfeChannel = -1;

    Params params;
    bool needsUpdate = true;
    float lfeScale = 1.0f;

    std::vector<Cell> lpHpFilters;
    std::vector<float> lfeSum;
};

} // namespace cinelab