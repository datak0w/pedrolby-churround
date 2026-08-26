#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace cinelab
{

// ============================================================================
// Downmixer 5.1 / 7.1 → estéreo (convención ITU-R BS.775).
//
//   L' = L + 0.707·C + 0.707·Ls + 0.5·Lb  (+ 0.5·LFE)
//   R' = R + 0.707·C + 0.707·Rs + 0.5·Rb  (+ 0.5·LFE)
//
// Mapeo por índice (layout estándar de JUCE):
//   0 L · 1 R · 2 C · 3 LFE · 4 Ls · 5 Rs · 6 Lb · 7 Rb
// Los canales restantes (2..7) se ponen a silencio: la entrega "print" es
// estéreo en L/R para rutear a un track estéreo.
// ============================================================================
class Downmixer
{
public:
    void process (float* const* data, int numChannels, int numSamples) noexcept
    {
        if (numChannels < 4 || numSamples == 0)
            return;

        const float* L  = data[0];
        const float* R  = data[1];
        const float* C  = data[2];
        const float* LFE = numChannels > 3 ? data[3] : nullptr;
        const float* Ls = numChannels > 4 ? data[4] : nullptr;
        const float* Rs = numChannels > 5 ? data[5] : nullptr;
        const float* Lb = numChannels > 6 ? data[6] : nullptr;
        const float* Rb = numChannels > 7 ? data[7] : nullptr;

        float* l = data[0];
        float* r = data[1];

        for (int i = 0; i < numSamples; ++i)
        {
            const double c = 0.707 * (C != nullptr ? C[i] : 0.0);
            const double lfe = 0.5 * (LFE != nullptr ? LFE[i] : 0.0);
            const double ls = 0.707 * (Ls != nullptr ? Ls[i] : 0.0) + 0.5 * (Lb != nullptr ? Lb[i] : 0.0);
            const double rs = 0.707 * (Rs != nullptr ? Rs[i] : 0.0) + 0.5 * (Rb != nullptr ? Rb[i] : 0.0);

            l[i] = (float) (L[i] + c + ls + lfe);
            r[i] = (float) (R[i] + c + rs + lfe);
        }

        // los canales de surround/central quedan en silencio (print estéreo)
        for (int c = 2; c < numChannels; ++c)
        {
            float* d = data[c];
            for (int i = 0; i < numSamples; ++i)
                d[i] = 0.0f;
        }
    }
};

} // namespace cinelab