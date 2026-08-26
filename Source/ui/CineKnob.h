#pragma once

#include "CineTheme.h"
#include "../Parameters.h"

namespace cinelab
{

// ============================================================================
// Knob with custom painting + APVTS attachment. Shows the value below and a
// "?" help badge while hovered (tooltip with the control description).
// ============================================================================
class CineKnob : public juce::Slider
{
public:
    CineKnob (const juce::String& title,
              juce::AudioProcessorValueTreeState& apvts,
              const juce::String& paramId,
              const juce::String& help = {},
              bool showValueText = true,
              bool showTitle = true)
        : paramTitle (title),
          showValue (showValueText),
          showTitleLabel (showTitle)
    {
        setSliderStyle (juce::Slider::RotaryVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setColour (juce::Slider::rotarySliderOutlineColourId, theme::border);
        setColour (juce::Slider::rotarySliderFillColourId, theme::accent);

        if (help.isNotEmpty())
            setTooltip (help);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, *this);
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        const float v = (float) getValue();

        // título
        if (showTitleLabel)
        {
            g.setColour (theme::textDim);
            g.setFont (theme::uiFont (11.0f, juce::Font::bold));
            g.drawText (paramTitle, area.withHeight (16.0f), juce::Justification::centred, true);
        }

        auto knobArea = area.withTrimmedTop (showTitleLabel ? 16.0f : 0.0f).withTrimmedBottom (showValue ? 18.0f : 0.0f);
        const float radius = juce::jmin (knobArea.getWidth(), knobArea.getHeight()) * 0.5f;
        auto centre = juce::Point<float> (knobArea.getCentreX(), knobArea.getCentreY());

        const float start = 0.75f * juce::MathConstants<float>::pi;
        const float end   = 2.25f * juce::MathConstants<float>::pi;
        const float norm = (float) (getValue() - getMinimum()) / (float) (getMaximum() - getMinimum());

        // pista
        g.setColour (theme::panelAlt);
        g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));

        // arco
        juce::Path bgPath, valPath;
        bgPath.addCentredArc (centre.x, centre.y, radius * 0.72f, radius * 0.72f, 0.0f, start, end, true);
        valPath.addCentredArc (centre.x, centre.y, radius * 0.72f, radius * 0.72f, 0.0f, start, start + (end - start) * norm, true);

        juce::PathStrokeType stroke (3.0f);
        g.setColour (theme::border);
        g.strokePath (bgPath, stroke);
        g.setColour (theme::accent);
        g.strokePath (valPath, stroke);

        // puntero
        const float angle = start + (end - start) * norm;
        juce::Point<float> tip (centre.x + std::cos (angle) * radius * 0.52f,
                                centre.y + std::sin (angle) * radius * 0.52f);
        g.setColour (theme::text);
        g.drawLine (juce::Line<float> (centre, tip), 2.0f);

        // valor
        if (showValue)
        {
            const juce::String valueText = getTextFromValue (getValue());
            g.setColour (theme::text);
            g.setFont (theme::uiFont (juce::jmin (12.0f, radius * 0.42f), juce::Font::bold));
            g.drawText (valueText, area.withTrimmedTop (knobArea.getHeight()).toFloat().withHeight (18.0f),
                        juce::Justification::centred, true);
        }

        // insignia de ayuda "?" (siguiendo el mouse)
        if (isMouseOverOrDragging() && getTooltip().isNotEmpty())
        {
            const float badge = 12.0f;
            auto pos = juce::Point<float> (area.getX() + badge, area.getY() + badge);
            g.setColour (theme::accent.withAlpha (0.92f));
            g.fillEllipse (juce::Rectangle<float> (badge, badge).withCentre (pos));
            g.setColour (theme::bg);
            g.setFont (theme::uiFont (10.0f, juce::Font::bold));
            g.drawText ("?", juce::Rectangle<float> (badge, badge).withCentre (pos),
                        juce::Justification::centred, true);
        }
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        setValue (getDoubleClickReturnValue(), juce::sendNotificationSync);
    }

private:
    juce::String paramTitle;
    bool showValue = true;
    bool showTitleLabel = true;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// ============================================================================
// Botón de elección (escena / destino / sala): pastilla con texto y subtítulo.
// ============================================================================
class ChoiceButton : public juce::Button
{
public:
    ChoiceButton (const juce::String& text, const juce::String& sub = {}, bool emphasize = false)
        : juce::Button (text), subtitle (sub), emphasized (emphasize)
    {
        setClickingTogglesState (true);
        setWantsKeyboardFocus (false);
    }

    void paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto r = getLocalBounds().toFloat();
        const bool on = getToggleState();

        juce::Colour fill = on ? (emphasized ? theme::accent : theme::accentDark)
                               : (shouldDrawButtonAsHighlighted ? theme::panelAlt : theme::panel);
        juce::Colour line = on ? (emphasized ? theme::accent : theme::accentDark) : theme::borderSoft;
        juce::Colour txtCol = on ? theme::bg : theme::textDim;

        g.setColour (fill);
        g.fillRoundedRectangle (r, 9.0f);
        g.setColour (line);
        g.drawRoundedRectangle (r, 9.0f, shouldDrawButtonAsDown ? 2.0f : 1.0f);

        g.setColour (txtCol);
        g.setFont (theme::uiFont (emphasized ? 12.5f : 11.5f, juce::Font::bold));

        if (subtitle.isEmpty())
        {
            g.drawText (getButtonText(), r, juce::Justification::centred, true);
        }
        else
        {
            g.drawText (getButtonText(), r.withTrimmedTop (r.getHeight() * 0.18f), juce::Justification::centredTop, true);
            g.setFont (theme::uiFont (9.0f));
            g.setColour (on ? theme::bg.withAlpha (0.7f) : theme::textFaint);
            g.drawText (subtitle, r.withTrimmedTop (r.getHeight() * 0.52f), juce::Justification::centredTop, true);
        }
    }

private:
    juce::String subtitle;
    bool emphasized = false;
};

// ============================================================================
// Puente bidireccional entre un grupo de ChoiceButton y un parámetro choice.
// (Click → parámetro; cambio externo/automatización → botones.)
// ============================================================================
class ChoiceToButtons : private juce::AudioProcessorValueTreeState::Listener
{
public:
    ChoiceToButtons (juce::AudioProcessorValueTreeState& vts, const juce::String& paramId,
                     const std::vector<ChoiceButton*>& buttons)
        : apvts (vts), id (paramId), buttonList (buttons)
    {
        apvts.addParameterListener (id, this);
        syncFromParam();
    }

    ~ChoiceToButtons() override
    {
        apvts.removeParameterListener (id, this);
    }

    void parameterChanged (const juce::String&, float) override
    {
        syncFromParam();
    }

    int getSelectedIndex() const noexcept { return selected; }

    void setSelected (int index, bool notifyHost = true)
    {
        if (index < 0 || index >= (int) buttonList.size())
            return;

        guard = true;
        for (int i = 0; i < (int) buttonList.size(); ++i)
            buttonList[(size_t) i]->setToggleState (i == index, juce::dontSendNotification);
        guard = false;
        selected = index;

        if (notifyHost)
        {
            if (auto* p = apvts.getParameter (id))
            {
                const auto range = apvts.getParameterRange (id);
                p->beginChangeGesture();
                p->setValueNotifyingHost (range.convertTo0to1 ((float) index));
                p->endChangeGesture();
            }
        }
    }

private:
    void syncFromParam()
    {
        if (guard) return;
        const auto range = apvts.getParameterRange (id);
        if (auto* p = apvts.getParameter (id))
        {
            const int idx = juce::roundToInt (range.convertFrom0to1 (p->getValue()));
            if (idx != selected)
            {
                guard = true;
                selected = idx;
                for (int i = 0; i < (int) buttonList.size(); ++i)
                    buttonList[(size_t) i]->setToggleState (i == idx, juce::dontSendNotification);
                guard = false;
            }
        }
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::String id;
    std::vector<ChoiceButton*> buttonList;
    int selected = -1;
    bool guard = false;
};

} // namespace cinelab