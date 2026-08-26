#pragma once

#include "Biquad.h"

namespace cinelab
{

// ============================================================================
// EQ Cinema — curva X (ISO 2969) para salas grandes.
//
// Las salas de cine absorben los agudos (público, alfombras, aire acondicionado)
// y acumulan graves. La curva X compensa eso: subida de ~+3 dB/octava por
// encima de 2 kHz y atenuación de graves por debajo de la zona plana.
//
// Implementación con filtros reales (shelves + peaking):
//   - shelf de graves      @ fLow (100 Hz por defecto, −4 dB)
//   - peaking ~2 kHz, ~4 kHz, ~8 kHz  (+1, +3.5, +6 dB → ≈ +3 dB/oct)
//   - shelf de aire        @ fAir (10 kHz) regulable
//
// El parámetro "roomSize" escala TODA la curva entre 0 (plano) y 1 (curva X
// completa de sala grande). Es la forma de simular sala pequeña → IMAX sin
// tocar nada más.
// ============================================================================
class CinemaEQ
{
public:
    struct Params
    {
        double roomSize    = 1.0;  // 0..1 escala general de la curva
        double lowGainDb   = -4.0; // shelf de graves
        double lowFreq     = 100.0;
        double g2kDb       = 1.0;
        double g4kDb       = 3.5;
        double g8kDb       = 6.0;
        double airGainDb   = 1.0;
        double airFreq     = 10000.0;
    };

    void prepare (double sampleRate, int numChannels)
    {
        sr = sampleRate;
        filters.assign ((size_t) numChannels, Channel());
        resetCachedParams();
        for (auto& ch : filters)
            ch.setup (sr, params);
    }

    void setParams (const Params& p)
    {
        params = p;
        needsUpdate = true;
    }

    void process (float* const* data, int numChannels, int numSamples) noexcept
    {
        if (needsUpdate)
        {
            for (auto& ch : filters)
                ch.setup (sr, params);
            needsUpdate = false;
        }

        const int n = juce::jmin ((int) filters.size(), numChannels);
        for (int c = 0; c < n; ++c)
        {
            float* d = data[c];
            auto& ch = filters[(size_t) c];

            for (int i = 0; i < numSamples; ++i)
                d[i] = ch.process (d[i]);
        }
    }

    bool isActive() const noexcept
    {
        return params.roomSize > 0.001;
    }

private:
    struct Channel
    {
        Biquad low, p2k, p4k, p8k, air;

        void process (float* d, int n) noexcept
        {
            for (int i = 0; i < n; ++i)
                d[i] = air.process (p8k.process (p4k.process (p2k.process (low.process (d[i])))));
        }

        float process (float x) noexcept
        {
            return air.process (p8k.process (p4k.process (p2k.process (low.process (x)))));
        }

        void setup (double sr, const Params& p)
        {
            low.reset (sr);  p2k.reset (sr); p4k.reset (sr); p8k.reset (sr); air.reset (sr);

            const double scale = juce::jlimit (0.0, 1.0, p.roomSize);

            if (scale < 0.001)
            {
                // curva apagada → paso directo
                low.setIdentity(); p2k.setIdentity(); p4k.setIdentity(); p8k.setIdentity(); air.setIdentity();
                return;
            }

            const double lowG  = p.lowGainDb * scale;
            const double g2    = p.g2kDb     * scale;
            const double g4    = p.g4kDb     * scale;
            const double g8    = p.g8kDb     * scale;
            const double airG  = p.airGainDb * scale;

            low.setLowShelf  (p.lowFreq, lowG, 0.707);
            p2k.setPeaking   (2000.0,    g2,  1.0);
            p4k.setPeaking   (4000.0,    g4,  1.0);
            p8k.setPeaking   (8000.0,    g8,  1.0);
            air.setHighShelf (p.airFreq, airG, 0.707);
        }
    };

    void resetCachedParams()
    {
        cached = params;
        needsUpdate = false;
    }

    double sr = 48000.0;
    Params params;
    Params cached;
    bool needsUpdate = true;
    std::vector<Channel> filters;
};

} // namespace cinelab