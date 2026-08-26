#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace cinelab
{

// ============================================================================
// Paleta "sala de proyección": fondo oscuro, acento ámbar de la lámpara de
// proyección, verde/ámbar/rojo para estados.
// ============================================================================
namespace theme
{
    inline const juce::Colour bg         (0xFF0D1117);
    inline const juce::Colour panel      (0xFF151C25);
    inline const juce::Colour panelAlt   (0xFF1B2430);
    inline const juce::Colour border     (0xFF2A3644);
    inline const juce::Colour borderSoft (0xFF22303E);
    inline const juce::Colour text       (0xFFE8EEF5);
    inline const juce::Colour textDim    (0xFF93A1B1);
    inline const juce::Colour textFaint  (0xFF5D6B7A);
    inline const juce::Colour accent     (0xFFF0A24B);
    inline const juce::Colour accentDark (0xFFC77F2E);
    inline const juce::Colour ok         (0xFF4FD08C);
    inline const juce::Colour warn       (0xFFF0C94B);
    inline const juce::Colour bad        (0xFFEF5B5B);
    inline const juce::Colour cyan       (0xFF5BC8E2);

    inline juce::Font uiFont (float height, int styleFlags = juce::Font::plain)
    {
        return juce::Font (height, styleFlags);
    }

    inline void drawPanel (juce::Graphics& g, const juce::Rectangle<float>& r, juce::Colour fill,
                           float corner = 10.0f, juce::Colour outline = border)
    {
        g.setColour (fill);
        g.fillRoundedRectangle (r, corner);
        g.setColour (outline);
        g.drawRoundedRectangle (r, corner, 1.0f);
    }

    inline void drawSectionTitle (juce::Graphics& g, const juce::String& title,
                                  const juce::Rectangle<int>& area)
    {
        g.setColour (textDim);
        g.setFont (uiFont (11.0f, juce::Font::bold));
        g.drawText (title.toUpperCase(), area, juce::Justification::left, true);
    }
}

// ============================================================================
// LookAndFeel global: botones de 3 estados, toggles tipo "pill", labels.
// ============================================================================
class CineLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CineLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, theme::text);
        setColour (juce::Slider::textBoxBackgroundColourId, theme::panel);
        setColour (juce::Slider::textBoxOutlineColourId, theme::border);
        setColour (juce::TextButton::textColourOffId, theme::text);
        setColour (juce::TextButton::textColourOnId, theme::bg);
        setColour (juce::TooltipWindow::textColourId, theme::text);
        setColour (juce::TooltipWindow::backgroundColourId, theme::panelAlt);
    }

    // Botones de texto (acciones secundarias, presets)
    void drawButtonBackground (juce::Graphics& g, juce::Button& b,
                               const juce::Colour&, bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        auto r = b.getLocalBounds().toFloat();

        juce::Colour fill = theme::panelAlt;
        if (b.getToggleState())       fill = theme::accentDark;
        else if (shouldDrawButtonAsDown) fill = theme::border;
        else if (shouldDrawButtonAsHighlighted) fill = theme::borderSoft;

        g.setColour (fill);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (theme::border);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto area = b.getLocalBounds().toFloat();
        const float h = juce::jmin (area.getHeight(), 22.0f);
        auto track = area.withSizeKeepingCentre (40.0f, h).reduced (0.0f, (area.getHeight() - h) / 2.0f);
        const bool on = b.getToggleState();

        g.setColour (on ? theme::accent : theme::panelAlt);
        g.fillRoundedRectangle (track, track.getHeight() / 2.0f);
        g.setColour (on ? theme::accentDark : theme::border);
        g.drawRoundedRectangle (track, track.getHeight() / 2.0f, 1.0f);

        const float knobR = h * 0.34f;
        auto knob = juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f);
        const float x = on ? track.getRight() - knob.getWidth() - 3.0f : track.getX() + 3.0f;
        knob.setCentre (juce::Point<float> (x + knobR, track.getCentreY()));
        g.setColour (theme::bg);
        g.fillEllipse (knob);

        g.setColour (on ? theme::accent : theme::textDim);
        g.setFont (theme::uiFont (11.0f, juce::Font::bold));
        g.drawText (b.getButtonText(), area.withLeft (track.getRight() + 10.0f).toFloat(),
                    juce::Justification::left, true);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& s) override
    {
        const float radius = (float) juce::jmin (w, h) * 0.5f;
        auto centre = juce::Point<float> ((float) x + w * 0.5f, (float) y + h * 0.5f);

        // pista
        g.setColour (theme::panelAlt);
        g.fillEllipse (juce::Rectangle<float> (w, h).toFloat().reduced (radius * 0.08f).translated ((float) x, (float) y));

        const float start = rotaryStartAngle + 0.25f * 3.14159f * 0.44f;
        const float end   = rotaryEndAngle - 0.25f * 3.14159f * 0.44f;

        // arco de fondo
        juce::Path bgArc;
        bgArc.addCentredArc (centre.x, centre.y, radius * 0.62f, radius * 0.62f, 0.0f, start, end, true);
        juce::PathStrokeType stroke (3.0f);
        g.setColour (theme::border);
        g.strokePath (bgArc, stroke);

        // arco de valor
        if (sliderPos > 0.001f)
        {
            juce::Path valArc;
            valArc.addCentredArc (centre.x, centre.y, radius * 0.62f, radius * 0.62f, 0.0f, start, start + (end - start) * sliderPos, true);
            g.setColour (theme::accent);
            g.strokePath (valArc, stroke);
        }

        // puntero
        const float angle = start + (end - start) * sliderPos;
        juce::Point<float> tip (centre.x + std::cos (angle) * radius * 0.45f,
                                centre.y + std::sin (angle) * radius * 0.45f);
        g.setColour (theme::text);
        g.drawLine (juce::Line<float> (centre, tip), 2.0f);
    }
};

} // namespace cinelab