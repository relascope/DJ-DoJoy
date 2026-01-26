#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), meterWidget (p)
{
    addAndMakeVisible (meterWidget);

    crossfader.setSliderStyle (juce::Slider::LinearHorizontal);
    crossfader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (crossfader);

    crossfaderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.apvts, "crossfade", crossfader);

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (400, 300);
}

PluginEditor::~PluginEditor()
{
}

void PluginEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    
    // Meter widget at the top
    meterWidget.setBounds (area.removeFromTop (100));
    
    // Crossfader in the remaining area
    area.removeFromTop (20); // gap
    crossfader.setBounds (area.removeFromTop (50).withSizeKeepingCentre (200, 50));
}
