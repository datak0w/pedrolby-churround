#pragma once

#include "Biquad.h"

namespace cinelab
{

// ============================================================================
// Simulador de sala — PRE-ESCUCHA.
//
// Mientras mezclas escuchas el material "plano" (estudio). En una sala de
// cine real el audio pierde agudos (público, alfombras, aire acondicionado)
// y acumula un poco de graves. Este módulo aplica ESA pérdida acústica a la
// salida para que oigas cómo sonará el material en la sala destino:
//
//   - acento de graves (buildup)      @ 120 Hz,  hasta +3 dB
//   - absorción de agudos             @  4 kHz,  hasta −6 dB
//   - aire apagado                    @ 10 kHz,  hasta −3 dB
//
// La cantidad se escala con "roomSize" (0=plano → 1=sala grande/IMAX), el
// mismo control del tamaño de sala de la curva X. Es una AYUDA DE MONITOREO:
// al apagarlo, la salida vuelve al procesado normal sin ningún cambio.
//
// (La curva X del EQ Cinema es lo contrario: COMPENSA la sala; el simulador
//  es la sala misma. Úsalos juntos para decidir cuánto compensar.)
// ============================================================================
class RoomSimulator
{
public:
    struct Params
    {
        double roomAmount = 1.0; // 0..1 (deriva de eqRoomSize)
    };

    void prepare (double sampleRate, int numChannels)
    {
        sr = sampleRate;
        filters.assign ((size_t) numChannels, Channel());
        for (auto& ch : filters)
            ch.setup (sr, params);
        needsUpdate = true;
    }

    void setParams (const Params& p)
    {
        params = p;
        needsUpdate = true;
    }

    void process (float* const* data, int numChannels, int numSamples) noexcept
    {
        if (params.roomAmount < 0.001)
            return;

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

    bool isActive() const noexcept { return params.roomAmount > 0.001; }

private:
    struct Channel
    {
        Biquad low, hi, air;

        void setup (double sr, const Params& p)
        {
            low.reset (sr); hi.reset (sr); air.reset (sr);

            const double s = juce::jlimit (0.0, 1.0, p.roomAmount);
            if (s < 0.001)
            {
                low.setIdentity(); hi.setIdentity(); air.setIdentity();
                return;
            }

            low.setLowShelf  (120.0,  +3.0 * s, 0.8);
            hi.setHighShelf  (4000.0, -6.0 * s, 0.7);
            air.setHighShelf (10000.0, -3.0 * s, 0.7);
        }

        float process (float x) noexcept
        {
            return air.process (hi.process (low.process (x)));
        }
    };

    double sr = 48000.0;
    Params params;
    bool needsUpdate = true;
    std::vector<Channel> filters;
};

} // namespace cinelab