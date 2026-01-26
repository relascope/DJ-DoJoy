#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class MeterWidget : public juce::Component, public juce::Timer
{
public:
    MeterWidget (PluginProcessor& p) : processor (p)
    {
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().reduced (2);
        
        g.setColour (juce::Colours::black.withAlpha (0.2f));
        g.fillRoundedRectangle (area.toFloat(), 4.0f);

        auto contentArea = area.reduced (4);
        
        auto mainArea = contentArea.removeFromTop (contentArea.getHeight() / 2).reduced (0, 2);
        auto scArea = contentArea.reduced (0, 2);

        auto mainBus = processor.getBus (true, 0);
        auto scBus = processor.getBus (true, 1);
        
        int mainChannels = mainBus != nullptr ? mainBus->getLastEnabledLayout().size() : 0;
        int scChannels = scBus != nullptr ? scBus->getLastEnabledLayout().size() : 0;

        drawBusMeter (g, mainArea, "Main", processor.getMainLevelLeft(), processor.getMainLevelRight(), mainChannels);
        drawBusMeter (g, scArea, "Sidechain", processor.getSidechainLevelLeft(), processor.getSidechainLevelRight(), scChannels);
    }

    void timerCallback() override
    {
        repaint();
    }

private:
    void drawBusMeter (juce::Graphics& g, juce::Rectangle<int> area, juce::String label, float levelL, float levelR, int numChannels)
    {
        auto labelArea = area.removeFromLeft (70);
        g.setColour (juce::Colours::white);
        g.setFont (12.0f);
        g.drawText (label + " (" + juce::String (numChannels) + ")", labelArea, juce::Justification::centredLeft);

        auto meterArea = area.reduced (0, 4);
        g.setColour (juce::Colours::black);
        g.fillRect (meterArea);

        auto levelToWidth = [&] (float level) {
            auto db = juce::Decibels::gainToDecibels (level);
            auto normalized = juce::jmap (db, -60.0f, 0.0f, 0.0f, 1.0f);
            return juce::jlimit (0.0f, 1.0f, normalized);
        };

        auto drawBar = [&] (juce::Rectangle<int> barArea, float level) {
            g.setColour (juce::Colours::darkgrey);
            g.drawRect (barArea);
            
            auto w = static_cast<int> (barArea.getWidth() * levelToWidth (level));
            g.setColour (juce::Colours::green.withMultipliedSaturation (0.8f));
            g.fillRect (barArea.withWidth (w).reduced (1));
        };

        if (numChannels >= 2)
        {
            auto h = meterArea.getHeight() / 2;
            drawBar (meterArea.removeFromTop (h), levelL);
            drawBar (meterArea, levelR);
        }
        else if (numChannels == 1)
        {
            drawBar (meterArea, levelL);
        }
    }

    PluginProcessor& processor;
};
