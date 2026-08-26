#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace cinelab
{

// ============================================================================
// Noise Reduction espectral — para diálogos grabados en exteriores.
//
// STFT (FFT 2048, hop 512 = solapamiento 75%, ventana Hann) con
// reconstrucción overlap-add (WOLA) y **sustracción espectral**:
//   1) Perfil de ruido adaptativo por bin: min-track (baja rápido al suelo
//      de ruido, sube lentísimo) + suavizado entre bins vecinos.
//   2) Ganancia por bin: g = max(0, 1 − K·N/M) con K = 1 + 2·amount,
//      suavizada temporalmente (ataque ~6 ms, release ~200 ms), piso
//      configurable (floor).
//   3) amount = 0 → reconstrucción perfecta (PR) con la misma latencia.
//      Latencia = fftSize (~43 ms @48 kHz); el host la compensa.
// ============================================================================
class NoiseReduction
{
public:
    static constexpr int fftOrder = 11;         // 2048
    static constexpr int fftSize  = 1 << fftOrder;
    static constexpr int hop      = 512;        // 75% overlap
    static constexpr int numBins  = fftSize / 2 + 1;

    struct Params
    {
        bool   enabled   = false;
        double amount    = 1.0;
        double floorDb   = -40.0;
    };

    void prepare (double sampleRate, int numChannels)
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        channels = juce::jmax (1, numChannels);

        fft = std::make_unique<juce::dsp::FFT> (fftOrder);

        window.resize (fftSize);
        for (int i = 0; i < fftSize; ++i)
            window[(size_t) i] = 0.5f * (1.0f - std::cos (2.0 * juce::MathConstants<float>::pi * (float) i / (float) (fftSize - 1)));

        hist.assign ((size_t) channels * fftSize, 0.0f);
        histIdx.assign ((size_t) channels, 0);
        hopCount.assign ((size_t) channels, 0);
        emit.assign ((size_t) channels * hop, 0.0f);
        emitRead.assign ((size_t) channels, 0);
        ola.assign ((size_t) channels * fftSize, 0.0f);
        frame.assign ((size_t) channels * fftSize, 0.0f);
        spec.assign ((size_t) channels * fftSize, std::complex<float> (0.0f, 0.0f));
        timeC.assign ((size_t) channels * fftSize, std::complex<float> (0.0f, 0.0f));
        noise.assign ((size_t) channels * numBins, 0.0f);
        gSmooth.assign ((size_t) channels * numBins, 1.0f);
        haveNoise.assign ((size_t) channels, 0);

        params = Params();
    }

    void setParams (const Params& p)   { params = p; }
    void requestProfileReset()         { resetRequested.store (true); }

    int getLatencySamples() const noexcept { return fftSize; }
    bool isActive() const noexcept { return params.enabled && channels > 0; }

    void processBlock (juce::AudioBuffer<float>& buffer) noexcept
    {
        const int nCh = juce::jmin (channels, buffer.getNumChannels());
        const int n   = buffer.getNumSamples();

        if (! params.enabled || nCh == 0 || n == 0)
            return;

        if (resetRequested.exchange (false))
        {
            for (int c = 0; c < channels; ++c)
            {
                std::fill (noise.data()    + (size_t) c * numBins, noise.data()    + (size_t) c * numBins + numBins, 0.0f);
                std::fill (gSmooth.data()  + (size_t) c * numBins, gSmooth.data()  + (size_t) c * numBins + numBins, 1.0f);
                haveNoise[(size_t) c] = 0;
            }
        }

        const float floorLin = (float) std::pow (10.0, params.floorDb / 20.0);
        const double K = 1.0 + 2.0 * params.amount;

        const double hopTime = (double) hop / sr;
        const float attCoeff = (float) (1.0 - std::exp (-hopTime / 0.006));
        const float relCoeff = (float) (1.0 - std::exp (-hopTime / 0.200));
        constexpr float olaScale = 2.0f / 3.0f;

        for (int c = 0; c < nCh; ++c)
        {
            const float* in  = buffer.getReadPointer (c);
            float*       out = buffer.getWritePointer (c);

            float* histRaw  = hist.data()     + (size_t) c * fftSize;
            int&   hIdx     = histIdx[(size_t) c];
            int&   hCount   = hopCount[(size_t) c];
            float* emitBuf  = emit.data()     + (size_t) c * hop;
            int&   eRead    = emitRead[(size_t) c];
            float* olaBuf   = ola.data()      + (size_t) c * fftSize;
            float* frameBuf = frame.data()    + (size_t) c * fftSize;
            std::complex<float>* specBuf = spec.data()   + (size_t) c * fftSize;
            std::complex<float>* timeBuf = timeC.data()  + (size_t) c * fftSize;
            float* noiseBuf = noise.data()   + (size_t) c * numBins;
            float* gSm      = gSmooth.data() + (size_t) c * numBins;
            int&   hNoise   = haveNoise[(size_t) c];

            for (int i = 0; i < n; ++i)
            {
                const float x = in[i];

                histRaw[hIdx] = x;
                hIdx = (hIdx + 1) % fftSize;
                ++hCount;

                float y = 0.0f;
                if (eRead < hop)
                    y = emitBuf[eRead];

                if (hCount == hop)
                {
                    hCount = 0;

                    // ---- frame (últimos fftSize en orden) → complejo ----
                    for (int k = 0; k < fftSize; ++k)
                        frameBuf[k] = histRaw[(hIdx + k) % fftSize] * window[(size_t) k];

                    for (int k = 0; k < fftSize; ++k)
                        timeBuf[k] = std::complex<float> (frameBuf[k], 0.0f);

                    fft->perform (timeBuf, specBuf, false);   // forward complejo

                    // ---- perfil de ruido: aprender SOLO en ventanas silenciosas ----------
                    // (si la energía del frame está cerca del suelo aprendido
                    //  se actualiza el min-track; si hay diálogo/tone, no se
                    //  toca — así el habla continua no "entra" en el perfil)
                    float frameEnergy = 1.0e-12f;
                    for (int b = 1; b < numBins - 1; ++b)
                    {
                        const float re = specBuf[b].real(), im = specBuf[b].imag();
                        frameEnergy += re * re + im * im;
                    }

                    if (hNoise == 0)
                    {
                        for (int b = 1; b < numBins; ++b)
                        {
                            const float re = specBuf[b].real(), im = specBuf[b].imag();
                            noiseBuf[b] = re * re + im * im;
                        }
                        noiseBuf[0] = noiseBuf[1];
                        hNoise = 1;
                    }
                    else
                    {
                        float profEnergy = 1.0e-12f;
                        for (int b = 1; b < numBins - 1; ++b)
                            profEnergy += noiseBuf[b];

                        const bool quietFrame = frameEnergy < profEnergy * 4.0f; // ≈ ≤ 6 dB sobre el suelo
                        if (quietFrame)
                        {
                            for (int b = 1; b < numBins; ++b)
                            {
                                const float re = specBuf[b].real(), im = specBuf[b].imag();
                                const float power = re * re + im * im;
                                if (power < noiseBuf[b])
                                    noiseBuf[b] = power;
                                else
                                    noiseBuf[b] *= 1.0004f;
                            }
                        }
                    }

                    // suavizado espectral del perfil (3 taps)
                    for (int b = 1; b < numBins - 1; ++b)
                        noiseBuf[b] = 0.25f * noiseBuf[b - 1] + 0.5f * noiseBuf[b] + 0.25f * noiseBuf[b + 1];

                    // ---- ganancia por bin (simétrica) -----------------------
                    const float noiseFloor = 1.0e-10f;
                    for (int b = 0; b <= fftSize / 2; ++b)
                    {
                        const int b2 = (b == 0 || b == fftSize / 2) ? b : fftSize - b;

                        const float re = specBuf[b].real(), im = specBuf[b].imag();
                        const float mag = std::sqrt (re * re + im * im + 1.0e-12f);
                        const float nv  = juce::jmax (noiseBuf[b], noiseFloor);

                        float g = 1.0f - (float) K * nv / (mag + 1.0e-9f);
                        g = juce::jmax (0.0f, g);
                        g = 1.0f - (1.0f - g) * (float) params.amount;
                        g = juce::jmax (g, floorLin);

                        if (g < gSm[b]) gSm[b] = gSm[b] + (g - gSm[b]) * attCoeff;
                        else            gSm[b] = gSm[b] + (g - gSm[b]) * relCoeff;
                        g = gSm[b];

                        specBuf[b]  *= g;
                        specBuf[b2] *= g;
                    }

                    fft->perform (specBuf, timeBuf, true);     // inverse complejo

                    // ---- síntesis + overlap-add -----------------------------
                    for (int k = 0; k < fftSize; ++k)
                        olaBuf[k] += timeBuf[k].real() * window[(size_t) k] * olaScale;

                    for (int k = 0; k < hop; ++k)
                        emitBuf[k] = olaBuf[k];
                    eRead = 0;

                    std::memmove (olaBuf, olaBuf + hop, (size_t) (fftSize - hop) * sizeof (float));
                    std::fill (olaBuf + (fftSize - hop), olaBuf + fftSize, 0.0f);
                }

                if (eRead < hop)
                {
                    y = emitBuf[eRead];
                    ++eRead;
                }

                out[i] = y;
            }
        }
    }

private:
    double sr = 48000.0;
    int channels = 1;
    Params params;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window;

    std::vector<float> hist, ola, frame, emit, noise, gSmooth;
    std::vector<std::complex<float>> spec, timeC;
    std::vector<int>   histIdx, hopCount, emitRead, haveNoise;
    std::atomic<bool>  resetRequested { false };
};

} // namespace cinelab