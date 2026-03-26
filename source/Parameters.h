#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace Parameters
{
    static const juce::ParameterID crossfade { "crossfade", 1 };
    static const juce::ParameterID autoXFadeEnabled { "autoXFadeEnabled", 1 };
    static const juce::ParameterID silenceThresholdTime { "silenceThresholdTime", 1 };
    static const juce::ParameterID autoXFadeDuration { "autoXFadeDuration", 1 };
    static const juce::ParameterID silenceThreshold { "silenceThreshold", 1 };
}
