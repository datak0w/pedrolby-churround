#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace cinelab
{

// ============================================================================
// Normalizador de sonoridad (entrega por estándar).
//
// Dos componentes de ganancia:
//   - autoDb: corrección automática hacia el target, actualizada en cada
//     ventana de medición (leída del LoudnessMeter). Con histéresis y
//     suavizado (ataque 1 s / release 2 s) para no "bombear".
//   - manualDb: offset del usuario (Pro).
//
// La ganancia total se aplica muestra a muestra con un one-pole corto
// (constante ~50 ms) para evitar clics cuando el valor cambia.
// ============================================================================
class LoudnessNormalizer
{
public:
    void reset (double sampleRate)
    {
        sr = sampleRate;
        autoDb = 0.0;
        smoothDb = 0.0;
        prevShortTerm = -1000.0;
    }

    void setTarget (double targetLufs) { target = targetLufs; }
    void setEnabled (bool enabled) { autoEnabled = enabled; }
    void setManualGain (double db)
    {
        if (db != manualDb)
        {
            manualDb = db;
            manualGainChanged.store (true);
        }
    }
    void setMaxGainDb (double db) { maxGainDb = db; }

    // Llamar una vez por ventana de medición con la sonoridad short-term actual.
    void update (float shortTermLufs)
    {
        if (! autoEnabled) return;

        if (shortTermLufs < -900.0f) // silencio: no hacemos nada
            return;

        if (prevShortTerm > -900.0f)
        {
            const double change = std::abs ((double) shortTermLufs - prevShortTerm);
            if (change > 2.0) // salto grande (corte/play): adaptación rápida
                fastTrack = 1.0;
        }
        prevShortTerm = shortTermLufs;

        double desired = target - (double) shortTermLufs;
        desired = juce::jlimit (-maxGainDb, maxGainDb, desired);

        if (fastTrack > 0.0)
            autoDb = desired; // alinea de inmediato tras un salto
        else
            autoDb += (desired - autoDb) * (desired > autoDb ? attackCoef : releaseCoef);

        if (std::abs (desired - autoDb) < 0.25)
            autoDb = desired;

        fastTrack *= 0.5f; // decae en pocas ventanas
    }

    // Ganancia total actual en dB (para mostrar en UI).
    double getGainDb() const noexcept { return autoDb + manualDb; }
    double getAutoGainDb() const noexcept { return autoDb; }
    double getManualGainDb() const noexcept { return manualDb; }

    // Aplica la ganancia a un bloque (todos los canales).

    void processBlock (float* const* data, int numChannels, int numSamples) noexcept
    {
        const double targetDb = getGainDb();
        const double coef = 1.0 - std::exp (-1.0 / (0.050 * sr)); // ~50 ms

        for (int i = 0; i < numSamples; ++i)
        {
            smoothDb += (targetDb - smoothDb) * coef;
            const float g = (float) std::pow (10.0, smoothDb / 20.0);
            for (int c = 0; c < numChannels; ++c)
                data[c][i] *= g;
        }
    }

private:
    double sr = 48000.0;
    double target = -24.0;
    double maxGainDb = 24.0;

    bool autoEnabled = true;
    double autoDb = 0.0;
    double manualDb = 0.0;
    double smoothDb = 0.0;
    std::atomic<bool> manualGainChanged { false };

    // update() se llama una vez por ventana de medición (≈0.4 s)
    static constexpr double kFrameSeconds = 0.4;
    double attackCoef  = 1.0 - std::exp (-kFrameSeconds / 1.0);  // ataque ~1 s
    double releaseCoef = 1.0 - std::exp (-kFrameSeconds / 2.0);  // release ~2 s
    float fastTrack = 0.0f;
    float prevShortTerm = -1000.0f;
};

} // namespace cinelab