#pragma once

#include <juce_core/juce_core.h>

namespace cinelab
{

// ============================================================================
// Biquad genérico de doble precisión (cookbook RBJ, forma directa I).
// Se usa para: K-weighting (BS.1770), curva X del EQ Cinema,
// EQs del módulo de escena y el de-esser dinámico.
// ============================================================================
class Biquad
{
public:
    enum class Type { lowPass, highPass, lowShelf, highShelf, peaking };

    Biquad() = default;

    void reset (double sampleRate)
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        clear();
    }

    void clear() noexcept
    {
        z1 = 0.0;
        z2 = 0.0;
    }

    void setCoefficients (double b0_, double b1_, double b2_, double a1_, double a2_) noexcept
    {
        b0 = b0_; b1 = b1_; b2 = b2_; a1 = a1_; a2 = a2_;
    }

    void setIdentity() noexcept
    {
        b0 = 1.0; b1 = 0.0; b2 = 0.0; a1 = 0.0; a2 = 0.0;
    }

    // --- Diseño de coeficientes (RBJ audio EQ cookbook) ------------------

    void setPeaking (double frequency, double gainDb, double q)
    {
        const double A  = std::pow (10.0, gainDb / 40.0);
        const double w0 = twoPi * frequency / sr;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * q);

        const double a0 = 1.0 + alpha / A;
        double b0v = 1.0 + alpha * A, b1v = -2.0 * cw, b2v = 1.0 - alpha * A;
        double a1v = -2.0 * cw, a2v = 1.0 - alpha / A;
        normalize (a0, b0v, b1v, b2v, a1v, a2v);
    }

    void setLowShelf (double frequency, double gainDb, double q)
    {
        const double A  = std::pow (10.0, gainDb / 40.0);
        const double w0 = twoPi * frequency / sr;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * q);

        const double twoSqrtAalpha = 2.0 * std::sqrt (A) * alpha;
        const double a0 = (A + 1.0) + (A - 1.0) * cw + twoSqrtAalpha;
        double b0v = A * ((A + 1.0) - (A - 1.0) * cw + twoSqrtAalpha);
        double b1v = 2.0 * A * ((A - 1.0) - (A + 1.0) * cw);
        double b2v = A * ((A + 1.0) - (A - 1.0) * cw - twoSqrtAalpha);
        double a1v = -2.0 * ((A - 1.0) + (A + 1.0) * cw);
        double a2v = (A + 1.0) + (A - 1.0) * cw - twoSqrtAalpha;
        normalize (a0, b0v, b1v, b2v, a1v, a2v);
    }

    void setHighShelf (double frequency, double gainDb, double q)
    {
        const double A  = std::pow (10.0, gainDb / 40.0);
        const double w0 = twoPi * frequency / sr;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * q);

        const double twoSqrtAalpha = 2.0 * std::sqrt (A) * alpha;
        const double a0 = (A + 1.0) - (A - 1.0) * cw + twoSqrtAalpha;
        double b0v = A * ((A + 1.0) + (A - 1.0) * cw + twoSqrtAalpha);
        double b1v = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw);
        double b2v = A * ((A + 1.0) + (A - 1.0) * cw - twoSqrtAalpha);
        double a1v = 2.0 * ((A - 1.0) - (A + 1.0) * cw);
        double a2v = (A + 1.0) - (A - 1.0) * cw - twoSqrtAalpha;
        normalize (a0, b0v, b1v, b2v, a1v, a2v);
    }

    void setHighPass (double frequency, double q)
    {
        const double w0 = twoPi * frequency / sr;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * q);

        const double a0 = 1.0 + alpha;
        double b0v = (1.0 + cw) / 2.0, b1v = -(1.0 + cw), b2v = b0v;
        double a1v = -2.0 * cw, a2v = 1.0 - alpha;
        normalize (a0, b0v, b1v, b2v, a1v, a2v);
    }

    void setLowPass (double frequency, double q)
    {
        const double w0 = twoPi * frequency / sr;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * q);

        const double a0 = 1.0 + alpha;
        double b0v = (1.0 - cw) / 2.0, b1v = 1.0 - cw, b2v = b0v;
        double a1v = -2.0 * cw, a2v = 1.0 - alpha;
        normalize (a0, b0v, b1v, b2v, a1v, a2v);
    }

    float process (float sample) noexcept
    {
        const double x = static_cast<double> (sample);
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return static_cast<float> (y);
    }

    double getSampleRate() const noexcept { return sr; }

private:
    static constexpr double twoPi = 6.28318530717958647693;

    void normalize (double a0, double& b0v, double& b1v, double& b2v, double& a1v, double& a2v) noexcept
    {
        setCoefficients (b0v / a0, b1v / a0, b2v / a0, a1v / a0, a2v / a0);
    }

    double sr = 48000.0;
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;
};

} // namespace cinelab