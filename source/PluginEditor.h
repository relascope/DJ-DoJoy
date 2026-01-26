#pragma once

#include "PluginProcessor.h"
#include "BinaryData.h"
#include "MeterWidget.h"

//==============================================================================
class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    PluginProcessor& processorRef;

    MeterWidget meterWidget;
    juce::Slider crossfader;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> crossfaderAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
