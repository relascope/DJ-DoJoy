#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

class AutoXFadeWidget : public juce::Component
{
public:
    AutoXFadeWidget (juce::AudioProcessorValueTreeState& vts)
        : apvts (vts)
    {
        addAndMakeVisible (enabledButton);
        enabledButton.setButtonText ("Auto Switch");
        enabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, "autoXFadeEnabled", enabledButton);

        setupSlider (silenceTimeSlider, silenceTimeLabel, "Silence (s)", "silenceThresholdTime");
        setupSlider (xfadeTimeSlider, xfadeTimeLabel, "Fade (s)", "autoXFadeDuration");
        setupSlider (thresholdSlider, thresholdLabel, "Threshold (dB)", "silenceThreshold");
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (getLookAndFeel().findColour (juce::GroupComponent::outlineColourId));
        g.drawRoundedRectangle (bounds.reduced (1.0f), 5.0f, 1.0f);
        
        g.setColour (getLookAndFeel().findColour (juce::Label::textColourId));
        g.setFont (juce::FontOptions (12.0f));
        g.drawText ("Auto X-Fade on Silence", getLocalBounds().removeFromTop (20), juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10);
        area.removeFromTop (20); // Title space

        enabledButton.setBounds (area.removeFromTop (25));
        area.removeFromTop (10);

        auto rowHeight = 40;
        
        auto drawRow = [&] (juce::Label& label, juce::Slider& slider) {
            auto row = area.removeFromTop (rowHeight);
            label.setBounds (row.removeFromLeft (80));
            slider.setBounds (row);
            area.removeFromTop (5);
        };

        drawRow (silenceTimeLabel, silenceTimeSlider);
        drawRow (xfadeTimeLabel, xfadeTimeSlider);
        drawRow (thresholdLabel, thresholdSlider);
    }

private:
    void setupSlider (juce::Slider& slider, juce::Label& label, const juce::String& name, const juce::String& paramID)
    {
        addAndMakeVisible (slider);
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 20);
        
        addAndMakeVisible (label);
        label.setText (name, juce::dontSendNotification);
        label.setFont (juce::FontOptions (10.0f));
        label.setJustificationType (juce::Justification::centredLeft);

        attachments.add (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramID, slider));
    }

    juce::AudioProcessorValueTreeState& apvts;

    juce::ToggleButton enabledButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment;

    juce::Slider silenceTimeSlider, xfadeTimeSlider, thresholdSlider;
    juce::Label silenceTimeLabel, xfadeTimeLabel, thresholdLabel;

    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> attachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoXFadeWidget)
};
