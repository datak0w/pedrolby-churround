#pragma once

#include "CineTheme.h"
#include "../dsp/LoudnessMeter.h"

namespace cinelab
{

// ============================================================================
// Medidores sin números para humanos: barra LUFS vertical con la marca del
// objetivo, barra de pico y etiqueta de estado en lenguaje natural.
// ============================================================================
class MeterPanel : public juce::Component
{
public:
    MeterPanel (const LoudnessMeter::Readouts& r, double initialTarget)
        : readouts (r), targetLufs (initialTarget)
    {
    }

    void setTarget (double t) { targetLufs = t; }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat();
        const float w = area.getWidth(), h = area.getHeight();

        theme::drawPanel (g, area, theme::panel, 12.0f);

        // ---- sección LUFS (2/3 superior) ----
        const float barW = juce::jmin (w * 0.30f, 90.0f);
        const auto lufsArea = area.reduced (14.0f, 12.0f).withWidth (barW).withHeight (h * 0.58f);
        const float barTop = lufsArea.getY();
        const float barH   = lufsArea.getHeight();

        const float shortTerm = readouts.shortTerm.load (std::memory_order_relaxed);
        const float integrated = readouts.integrated.load (std::memory_order_relaxed);

        const float lo = -40.0f, hi = 0.0f;

        // fondo de la barra
        g.setColour (theme::panelAlt);
        g.fillRoundedRectangle (lufsArea, 5.0f);

        auto yFor = [&] (float lufs)
        {
            const float t = juce::jlimit (lo, hi, lufs);
            return barTop + barH * (1.0f - (t - lo) / (hi - lo));
        };

        // relleno (nivel short-term)
        if (shortTerm > -900.0f)
        {
            const float sy = yFor (shortTerm);
            auto fill = juce::Rectangle<float> (lufsArea.getX(), sy, lufsArea.getWidth(), lufsArea.getBottom() - sy);
            g.setColour (fillColourFor (shortTerm));
            g.fillRoundedRectangle (fill, 5.0f);
        }

        // marca del objetivo
        const float ty = yFor ((float) targetLufs);
        g.setColour (theme::text);
        g.drawLine (lufsArea.getX() - 3.0f, ty, lufsArea.getRight() + 3.0f, ty, 1.5f);

        g.setColour (theme::textDim);
        g.setFont (theme::uiFont (9.0f));
        g.drawText (juce::String (juce::roundToInt (targetLufs)) + " LUFS", lufsArea.withBottom (ty).translated (0.0f, -8.0f),
                    juce::Justification::centred, true);

        // escala + valores
        for (int mark = -40; mark <= 0; mark += 10)
        {
            const float my = yFor ((float) mark);
            g.setColour (theme::border);
            g.drawHorizontalLine (juce::roundToInt (my), lufsArea.getX() + 6.0f, lufsArea.getRight() - 6.0f);
        }

        // ---- textos de valores (derecha de la barra) ----
        const auto txtArea = lufsArea.withX (lufsArea.getRight() + 12.0f).withWidth (w - barW - 40.0f);
        g.setFont (theme::uiFont (11.0f, juce::Font::bold));

        g.setColour (theme::text);
        g.drawText ("Momentary  " + lufsText (readouts.momentary), txtArea, juce::Justification::topLeft, true);
        g.setColour (theme::textDim);
        g.drawText ("Short term " + lufsText (readouts.shortTerm), txtArea.translated (0.0f, 16.0f), juce::Justification::topLeft, true);
        g.drawText ("Integrated " + lufsText (readouts.integrated), txtArea.translated (0.0f, 32.0f), juce::Justification::topLeft, true);

        // ---- estado humano ----
        const juce::String status = humanStatus (readouts.shortTerm);
        const juce::Colour sc = statusColour (readouts.shortTerm);
        if (! status.isEmpty())
        {
            g.setColour (sc);
            g.setFont (theme::uiFont (15.0f, juce::Font::bold));
            g.drawText (status, lufsArea.withY (lufsArea.getBottom() + 6.0f).withHeight (22.0f),
                        juce::Justification::centred, true);
        }

        // ---- pico (tira inferior) ----
        const auto peakArea = juce::Rectangle<float> (lufsArea.getX(), lufsArea.getBottom() + 34.0f,
                                                      w - 28.0f, 10.0f);
        g.setColour (theme::panelAlt);
        g.fillRoundedRectangle (peakArea, 5.0f);

        const float peak = readouts.peakDb.load (std::memory_order_relaxed);
        const float pt = juce::jmap<float> (peak, -60.0f, 0.0f, 0.0f, 1.0f);
        auto peakFill = juce::Rectangle<float> (peakArea.getX(), peakArea.getY(),
                                                peakArea.getWidth() * juce::jlimit (0.0f, 1.0f, pt), peakArea.getHeight());
        g.setColour (peak > -6.0f ? theme::bad : (peak > -18.0f ? theme::warn : theme::ok));
        g.fillRoundedRectangle (peakFill, 5.0f);

        g.setColour (theme::textDim);
        g.setFont (theme::uiFont (9.0f));
        g.drawText ("PEAK  " + (peak > -90.0f ? juce::String (peak, 1) + " dBFS" : "--"),
                    peakArea.withY (peakArea.getBottom() + 2.0f).withHeight (14.0f),
                    juce::Justification::centred, true);

        // true peak (BS.1770)
        const float tp = readouts.truePeakDb.load (std::memory_order_relaxed);
        g.setColour (tp > -90.0f ? theme::accent : theme::textFaint);
        g.drawText ("TRUE PEAK " + (tp > -90.0f ? juce::String (tp, 1) + " dBFS" : "--"),
                    peakArea.withY (peakArea.getBottom() + 16.0f).withHeight (14.0f),
                    juce::Justification::centred, true);
    }

private:
    static juce::String lufsText (const std::atomic<float>& v)
    {
        const float x = v.load (std::memory_order_relaxed);
        if (x < -900.0f) return "--";
        if (x < -60.0f)  return "< -60";
        return juce::String (x, 1) + " LUFS";
    }

    juce::Colour fillColourFor (float lufs) const
    {
        const double t = targetLufs;
        if (lufs > t + 4.0) return theme::bad;
        if (lufs > t + 1.5) return theme::warn;
        if (lufs > t - 1.5) return theme::ok;
        return theme::cyan.withAlpha (0.55f);
    }

    juce::Colour statusColour (const std::atomic<float>& v) const
    {
        const float x = v.load (std::memory_order_relaxed);
        if (x < -900.0f) return theme::textFaint;
        if (x > (float) targetLufs + 4.0) return theme::bad;
        if (x > (float) targetLufs + 1.5) return theme::warn;
        if (x > (float) targetLufs - 1.5) return theme::ok;
        return theme::cyan;
    }

    juce::String humanStatus (const std::atomic<float>& v) const
    {
        const float x = v.load (std::memory_order_relaxed);
        if (x < -900.0f) return "--";

        if (x > (float) targetLufs + 6.0) return "TOO LOUD";
        if (x > (float) targetLufs + 1.5) return "A LITTLE LOUD";
        if (x > (float) targetLufs - 1.5) return "SOUNDS GOOD";
        if (x > (float) targetLufs - 6.0) return "A LITTLE QUIET";
        return "TOO QUIET";
    }

    const LoudnessMeter::Readouts& readouts;
    double targetLufs = -24.0;
};

} // namespace cinelab