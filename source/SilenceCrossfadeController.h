#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class SilenceCrossfadeController
{
public:
    SilenceCrossfadeController (juce::AudioProcessorValueTreeState& vts)
        : apvts (vts)
    {
        crossfadeParam = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("crossfade"));
        enabledParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("autoXFadeEnabled"));
        silenceTimeParam = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("silenceThresholdTime"));
        xfadeTimeParam = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("autoXFadeDuration"));
        thresholdParam = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("silenceThreshold"));
    }

    void update (float mainLevel, float sidechainLevel, double sampleRate, int numSamples)
    {
        if (enabledParam == nullptr || ! enabledParam->get())
        {
            reset();
            return;
        }

        float deltaTime = (float) numSamples / (float) sampleRate;
        float currentPos = crossfadeParam->get();

        // Detect user manual override
        if (std::abs (currentPos - lastSetPos) > 0.001f && ! isAutomating)
        {
            reset();
        }
        
        if (isAutomating)
        {
            updateAutomation (deltaTime);
            return;
        }

        // Determine which side is active based on crossfader
        // 0.0 is Main 100%, 1.0 is Sidechain 100%
        bool monitoringMain = currentPos < 0.5f;
        float activeLevel = monitoringMain ? mainLevel : sidechainLevel;
        float targetLevel = monitoringMain ? sidechainLevel : mainLevel;
        float threshold = juce::Decibels::decibelsToGain (thresholdParam->get());

        if (activeLevel < threshold)
        {
            // Only trigger if the side we would switch to is NOT silent
            if (targetLevel >= threshold)
            {
                silenceTimer += deltaTime;
                if (silenceTimer >= silenceTimeParam->get())
                {
                    startAutomation (currentPos);
                }
            }
            else
            {
                silenceTimer = 0.0f; // Reset if the target is also silent
            }
        }
        else
        {
            silenceTimer = 0.0f;
        }
        
        lastSetPos = currentPos;
    }

    void reset()
    {
        silenceTimer = 0.0f;
        isAutomating = false;
        lastSetPos = crossfadeParam->get();
    }

private:
    void startAutomation (float startPos)
    {
        isAutomating = true;
        automationProgress = 0.0f;
        automationStartPos = startPos;
        // Bi-directional: if at 0.3 (left-ish), go to 0.7 (right-ish)
        // Formula: target = 1.0 - startPos
        automationTargetPos = 1.0f - startPos;
    }

    void updateAutomation (float deltaTime)
    {
        float duration = xfadeTimeParam->get();
        if (duration <= 0.0f)
        {
            automationProgress = 1.0f;
        }
        else
        {
            automationProgress += deltaTime / duration;
        }

        if (automationProgress >= 1.0f)
        {
            automationProgress = 1.0f;
            isAutomating = false;
        }

        float newPos = automationStartPos + (automationTargetPos - automationStartPos) * automationProgress;
        crossfadeParam->beginChangeGesture();
        crossfadeParam->setValueNotifyingHost (newPos);
        crossfadeParam->endChangeGesture();
        lastSetPos = newPos;
        
        if (!isAutomating) {
            silenceTimer = 0.0f;
        }
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::AudioParameterFloat* crossfadeParam = nullptr;
    juce::AudioParameterBool* enabledParam = nullptr;
    juce::AudioParameterFloat* silenceTimeParam = nullptr;
    juce::AudioParameterFloat* xfadeTimeParam = nullptr;
    juce::AudioParameterFloat* thresholdParam = nullptr;

    float silenceTimer = 0.0f;
    bool isAutomating = false;
    float automationProgress = 0.0f;
    float automationStartPos = 0.0f;
    float automationTargetPos = 0.0f;
    float lastSetPos = 0.5f;
};
