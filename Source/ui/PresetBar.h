#pragma once

#include "CineTheme.h"
#include "../PluginProcessor.h"

namespace cinelab
{

// ============================================================================
// User presets bar (header). Independent of the DAW preset system:
// named snapshots of the full parameter state, saved/loaded/deleted here.
// ============================================================================
class PresetBar : public juce::Component
{
public:
    PresetBar (CineLabAudioProcessor& proc, Parameters& params)
        : processor (proc), parameters (params)
    {
        nameEditor.setColour (juce::TextEditor::textColourId, theme::text);
        nameEditor.setColour (juce::TextEditor::backgroundColourId, theme::panel);
        nameEditor.setColour (juce::TextEditor::outlineColourId, theme::border);
        nameEditor.setText ("My preset", juce::dontSendNotification);
        nameEditor.setTooltip ("Name for the new preset.");
        addAndMakeVisible (nameEditor);

        saveButton.setButtonText ("Save");
        saveButton.setTooltip ("Save the current settings as a user preset (stored with the plugin state, independent of the DAW).");
        saveButton.setColour (juce::TextButton::buttonColourId, theme::panelAlt);
        saveButton.setColour (juce::TextButton::textColourOffId, theme::textDim);
        saveButton.onClick = [this] { onSave(); };
        addAndMakeVisible (saveButton);

        combo.setColour (juce::ComboBox::backgroundColourId, theme::panel);
        combo.setColour (juce::ComboBox::textColourId, theme::text);
        combo.setColour (juce::ComboBox::outlineColourId, theme::border);
        combo.setColour (juce::ComboBox::arrowColourId, theme::textDim);
        combo.setTooltip ("User presets. Select one and press Load.");
        combo.onChange = [this] { onLoad(); };
        addAndMakeVisible (combo);

        loadButton.setButtonText ("Load");
        loadButton.setTooltip ("Apply the selected preset (all parameters).");
        loadButton.setColour (juce::TextButton::buttonColourId, theme::panelAlt);
        loadButton.setColour (juce::TextButton::textColourOffId, theme::textDim);
        loadButton.onClick = [this] { onLoad(); };
        addAndMakeVisible (loadButton);

        deleteButton.setButtonText ("Del");
        deleteButton.setTooltip ("Delete the selected preset.");
        deleteButton.setColour (juce::TextButton::buttonColourId, theme::panelAlt);
        deleteButton.setColour (juce::TextButton::textColourOffId, theme::bad);
        deleteButton.onClick = [this] { onDelete(); };
        addAndMakeVisible (deleteButton);

        refresh();
    }

    void refresh()
    {
        auto& up = processor.getUserPresets();

        if (up.consumeTreeChanged())
            repopulate();

        repopulate();
    }

    void resized() override
    {
        const int gap = 6;
        int x = 0;

        nameEditor.setBounds (x, 2, 120, 24);                    x += 120 + gap;
        saveButton .setBounds (x, 2, 46, 24);                    x += 46 + gap;
        combo      .setBounds (x, 2, 170, 24);                   x += 170 + gap;
        loadButton .setBounds (x, 2, 50, 24);                    x += 50 + gap;
        deleteButton.setBounds (x, 2, 44, 24);
    }

private:
    void repopulate()
    {
        const juce::String selected = combo.getText();
        combo.clear (juce::dontSendNotification);
        combo.addItemList (processor.getUserPresets().getPresetNames(), 1);
        if (selected.isNotEmpty())
            combo.setText (selected, juce::dontSendNotification);
    }

    void onSave()
    {
        processor.getUserPresets().savePreset (nameEditor.getText());
        refresh();
    }

    void onLoad()
    {
        const juce::String name = combo.getText();
        if (name.isEmpty())
            return;
        processor.getUserPresets().loadPreset (name);
    }

    void onDelete()
    {
        const juce::String name = combo.getText();
        if (name.isEmpty())
            return;
        processor.getUserPresets().deletePreset (name);
        repopulate();
    }

    CineLabAudioProcessor& processor;
    Parameters& parameters;

    juce::TextEditor nameEditor;
    juce::TextButton saveButton;
    juce::ComboBox combo;
    juce::TextButton loadButton;
    juce::TextButton deleteButton;
};

} // namespace cinelab