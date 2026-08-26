#pragma once

#include "Biquad.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace cinelab
{

// ============================================================================
// Módulo de escena: acondiciona el material según su contenido.
// Cadena por escena:  HP → cuerpo → presencia → de-esser → aire,
// luego compresor (glue estéreo) y anchura mid/side.
//
// Todo es paramétrico; los presets de escena (ver Source/Presets.h) eligen
// los valores. El factor "intensity" de la cara Directora escala las
// cantidades creativas desde el procesador antes de llamar a setParams().
// ============================================================================
class SceneModule
{
public:
    struct Params
    {
        double hpFreq        = 20.0;   // Hz (<=20 → sin filtro)
        double bodyFreq      = 250.0;
        double bodyGainDb    = 0.0;
        double presenceFreq  = 3200.0;
        double presenceGainDb= 0.0;
        double airGainDb     = 0.0;
        double deEss         = 0.0;    // 0..1 drive del de-esser
        double compAmount    = 0.0;    // 0..1 (mapea umbral/ratio)
        double width         = 1.0;    // 0 (mono) .. 1 (normal) .. 2 (ancho)
    };

    void prepare (double sampleRate, int numChannels)
    {
        sr = sampleRate;
        channels = numChannels;

        hpFilters.resize ((size_t) numChannels);
        bodyFilters.resize ((size_t) numChannels);
        presFilters.resize ((size_t) numChannels);
        deEssShelves.resize ((size_t) numChannels);
        airFilters.resize ((size_t) numChannels);
        scFilters.resize ((size_t) numChannels);

        for (int c = 0; c < numChannels; ++c)
        {
            hpFilters[(size_t) c].reset (sr);
            bodyFilters[(size_t) c].reset (sr);
            presFilters[(size_t) c].reset (sr);
            deEssShelves[(size_t) c].reset (sr);
            airFilters[(size_t) c].reset (sr);
            scFilters[(size_t) c].reset (sr);
        }

        resetState();
        params = Params();
        needsUpdate = true;
    }

    void resetState()
    {
        deEssEnv = 0.0;
        compEnv = 0.0;
        for (auto& f : scFilters) f.clear();
    }

    void setParams (const Params& p)
    {
        params = p;
        needsUpdate = true;
    }

    void processBlock (juce::AudioBuffer<float>& buffer) noexcept
    {
        const int nCh = juce::jmin ((int) hpFilters.size(), buffer.getNumChannels());
        const int n   = buffer.getNumSamples();
        if (nCh == 0 || n == 0)
            return;

        // la anchura mid/side solo aplica en estéreo puro (no en 5.1/7.1)
        const bool stereo = (nCh == 2);

        // ---- actualización de coeficientes (por bloque) -------------------
        if (needsUpdate)
        {
            deEssMaxCutDb = params.deEss * 8.0;
            compThresholdDb = 20.0 * (1.0 - params.compAmount) - 30.0; // -30 .. -10
            compRatio = 1.0 + 5.0 * params.compAmount;                  // 1 .. 6

            const auto aCoeff = [this] (double ms) { return ms <= 0.0 ? 1.0 : 1.0 - std::exp (-1.0 / (ms * 0.001 * sr)); };
            compAtt = aCoeff (10.0);
            compRel = aCoeff (220.0);
            deEssAtt = aCoeff (1.0);
            deEssRel = aCoeff (250.0);

            for (int c = 0; c < nCh; ++c)
            {
                Biquad& hp  = hpFilters[(size_t) c];
                Biquad& body = bodyFilters[(size_t) c];
                Biquad& pres = presFilters[(size_t) c];
                Biquad& air = airFilters[(size_t) c];
                Biquad& sc  = scFilters[(size_t) c];

                hp.reset (sr);
                body.reset (sr);
                pres.reset (sr);
                air.reset (sr);
                sc.reset (sr);

                if (params.hpFreq > 20.0) hp.setHighPass (params.hpFreq, 0.707);
                if (std::abs (params.bodyGainDb) > 0.01)
                    body.setPeaking (params.bodyFreq, params.bodyGainDb, 1.0);
                if (std::abs (params.presenceGainDb) > 0.01)
                    pres.setPeaking (params.presenceFreq, params.presenceGainDb, 1.0);
                if (std::abs (params.airGainDb) > 0.01)
                    air.setHighShelf (12000.0, params.airGainDb, 0.707);

                if (params.deEss > 0.001)
                    sc.setHighPass (6500.0, 0.707);

                // el shelf dinámico del de-esser se ajusta por bloque según la reducción
                Biquad& deEss = deEssShelves[(size_t) c];
                deEss.reset (sr);
                deEss.setHighShelf (7000.0, -deEssReductionDb, 0.707);
            }

            needsUpdate = false;
        }

        // ---- procesamiento ------------------------------------------------
        for (int i = 0; i < n; ++i)
        {
            // 1) filtros estáticos por canal
            for (int c = 0; c < nCh; ++c)
            {
                float* d = buffer.getWritePointer (c);
                float x  = d[i];

                if (params.hpFreq > 20.0) x = hpFilters[(size_t) c].process (x);
                if (std::abs (params.bodyGainDb) > 0.01) x = bodyFilters[(size_t) c].process (x);
                if (std::abs (params.presenceGainDb) > 0.01) x = presFilters[(size_t) c].process (x);
                if (std::abs (params.airGainDb) > 0.01) x = airFilters[(size_t) c].process (x);

                d[i] = x;
            }

            // 2) de-esser dinámico (sidechain compartida)
            if (params.deEss > 0.001)
            {
                double sc = 0.0;
                for (int c = 0; c < nCh; ++c)
                    sc = juce::jmax (sc, (double) std::abs (scFilters[(size_t) c].process (buffer.getReadPointer (c)[i])));

                if (sc > deEssEnv) deEssEnv += (sc - deEssEnv) * deEssAtt;
                else               deEssEnv += (sc - deEssEnv) * deEssRel;

                const double envDb = 20.0 * std::log10 (deEssEnv + 1.0e-9);
                const double reduction = juce::jlimit (0.0, 1.0, (envDb - (-24.0)) / 24.0) * deEssMaxCutDb;

                if (std::abs (reduction - deEssReductionDb) > 0.1)
                {
                    deEssReductionDb = reduction;
                    for (int c = 0; c < nCh; ++c)
                    {
                        Biquad& deEss = deEssShelves[(size_t) c];
                        deEss.reset (sr);
                        deEss.setHighShelf (7000.0, -reduction, 0.707);
                    }
                }

                if (reduction > 0.05)
                    for (int c = 0; c < nCh; ++c)
                    {
                        float* d = buffer.getWritePointer (c);
                        d[i] = deEssShelves[(size_t) c].process (d[i]);
                    }
            }

            // 3) compresor (glue estéreo: envolvente del canal más fuerte)
            if (params.compAmount > 0.001)
            {
                double peak = 0.0;
                for (int c = 0; c < nCh; ++c)
                    peak = juce::jmax (peak, (double) std::abs (buffer.getReadPointer (c)[i]));

                if (peak > compEnv) compEnv += (peak - compEnv) * compAtt;
                else                compEnv += (peak - compEnv) * compRel;

                const double envDb = 20.0 * std::log10 (compEnv + 1.0e-9);
                const double over = envDb - compThresholdDb;
                const double gainDb = over > 0.0 ? -over * (1.0 - 1.0 / compRatio) : 0.0;
                const double g = std::pow (10.0, gainDb / 20.0);

                for (int c = 0; c < nCh; ++c)
                {
                    float* d = buffer.getWritePointer (c);
                    d[i] = (float) (d[i] * g);
                }
            }
        }

        // 4) anchura mid/side (solo estéreo)
        if (stereo && std::abs (params.width - 1.0) > 0.005)
        {
            float* l = buffer.getWritePointer (0);
            float* r = buffer.getWritePointer (1);
            for (int i = 0; i < n; ++i)
            {
                const float m = 0.5f * (l[i] + r[i]);
                const float s = 0.5f * (l[i] - r[i]) * (float) params.width;
                l[i] = m + s;
                r[i] = m - s;
            }
        }
    }

private:
    double sr = 48000.0;
    int channels = 1;

    Params params;
    bool needsUpdate = true;

    std::vector<Biquad> hpFilters, bodyFilters, presFilters, deEssShelves, airFilters, scFilters;

    double deEssEnv = 0.0;
    double deEssReductionDb = 0.0;
    double deEssMaxCutDb = 0.0;
    double deEssAtt = 0.0, deEssRel = 0.0;

    double compEnv = 0.0;
    double compThresholdDb = -30.0;
    double compRatio = 1.0;
    double compAtt = 0.0, compRel = 0.0;
};

} // namespace cinelab