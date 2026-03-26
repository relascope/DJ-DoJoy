#include "helpers/test_helpers.h"
#include <PluginProcessor.h>
#include <Parameters.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/catch_approx.hpp>

TEST_CASE ("one is equal to one", "[dummy]")
{
    REQUIRE (1 == 1);
}

TEST_CASE ("Plugin instance", "[instance]")
{
    PluginProcessor testPlugin;

    SECTION ("name")
    {
        CHECK_THAT (testPlugin.getName().toStdString(),
            Catch::Matchers::Equals ("DJ-DoJoy"));
    }

    SECTION ("Sidechain bus")
    {
        auto* bus = testPlugin.getBus (true, 1);
        REQUIRE (bus != nullptr);
        CHECK (bus->getName() == "Sidechain");
        CHECK (bus->getDefaultLayout() == juce::AudioChannelSet::stereo());
    }

    SECTION ("Crossfader Summing")
    {
        testPlugin.prepareToPlay (44100.0, 512);
        juce::AudioBuffer<float> buffer (2, 512);
        buffer.clear();

        // Main input (bus 0) - all 1.0s
        // Sidechain input (bus 1) - all 0.5s
        // We need to set these in a way that processBlock can access them.
        // JUCE's processBlock for a plugin with multiple input buses expects
        // the buffer to contain all channels from all buses if they are interleaved,
        // but PluginProcessor::processBlock uses getBusBuffer.

        // Actually, getBusBuffer(buffer, true, 0) refers to the channels in 'buffer'.
        // If the plugin has 2 stereo inputs, totalNumInputChannels is 4.
        // But the buffer passed to processBlock might only have 2 channels (the output channels).
        // JUCE handles this by mapping.

        // Let's create a buffer that has enough channels for both inputs.
        juce::AudioBuffer<float> fullBuffer (4, 512);
        for (int i = 0; i < 512; ++i)
        {
            fullBuffer.setSample (0, i, 1.0f); // Main Left
            fullBuffer.setSample (1, i, 1.0f); // Main Right
            fullBuffer.setSample (2, i, 0.5f); // Sidechain Left
            fullBuffer.setSample (3, i, 0.5f); // Sidechain Right
        }

        juce::MidiBuffer midi;

        // Test at 0.0 (Only Main)
        testPlugin.apvts.getParameter (Parameters::crossfade.getParamID())->setValueNotifyingHost (0.0f);
        testPlugin.processBlock (fullBuffer, midi);
        // At 0.0, mainGain = cos(0) = 1.0, sidechainGain = sin(0) = 0.0
        CHECK (fullBuffer.getSample (0, 0) == Catch::Approx (1.0f));

        // Test at 1.0 (Only Sidechain)
        fullBuffer.clear();
        for (int i = 0; i < 512; ++i) {
            fullBuffer.setSample (0, i, 1.0f);
            fullBuffer.setSample (1, i, 1.0f);
            fullBuffer.setSample (2, i, 0.5f);
            fullBuffer.setSample (3, i, 0.5f);
        }
        testPlugin.apvts.getParameter (Parameters::crossfade.getParamID())->setValueNotifyingHost (1.0f);
        testPlugin.processBlock (fullBuffer, midi);
        // At 1.0, mainGain = cos(pi/2) = 0.0, sidechainGain = sin(pi/2) = 1.0
        // Result should be Sidechain * 1.0 = 0.5
        CHECK (fullBuffer.getSample (0, 0) == Catch::Approx (0.5f));

        // Test at 0.5 (Half way)
        fullBuffer.clear();
        for (int i = 0; i < 512; ++i) {
            fullBuffer.setSample (0, i, 1.0f);
            fullBuffer.setSample (2, i, 1.0f);
        }
        testPlugin.apvts.getParameter (Parameters::crossfade.getParamID())->setValueNotifyingHost (0.5f);
        testPlugin.processBlock (fullBuffer, midi);
        // mainGain = cos(pi/4) = 0.7071
        // sideGain = sin(pi/4) = 0.7071
        // Result = 1.0 * 0.7071 + 1.0 * 0.7071 = 1.4142
        CHECK (fullBuffer.getSample (0, 0) == Catch::Approx (1.41421356f));
    }

    SECTION ("Auto-XFade on Silence")
    {
        testPlugin.prepareToPlay (44100.0, 512);
        juce::MidiBuffer midi;

        // Enable auto-xfade
        testPlugin.apvts.getParameter (Parameters::autoXFadeEnabled.getParamID())->setValueNotifyingHost (1.0f);
        // Set silence detection to 0.1s
        testPlugin.apvts.getParameter (Parameters::silenceThresholdTime.getParamID())->setValueNotifyingHost (0.1f / 10.0f); // 0.1s in range 0.1 to 10.0
        // Set crossfade duration to 0.1s
        testPlugin.apvts.getParameter (Parameters::autoXFadeDuration.getParamID())->setValueNotifyingHost (0.1f / 10.0f);
        // Set threshold to -20dB (high for easy testing)
        testPlugin.apvts.getParameter (Parameters::silenceThreshold.getParamID())->setValueNotifyingHost ((-20.0f + 100.0f) / 80.0f);

        // Start at 0.0 (Main)
        testPlugin.apvts.getParameter (Parameters::crossfade.getParamID())->setValueNotifyingHost (0.0f);

        // Process silence for 0.15s (should NOT trigger because Sidechain is also silent)
        int numBlocks = (int) (0.15 * 44100.0 / 512.0) + 1;
        juce::AudioBuffer<float> buffer (4, 512);
        buffer.clear();

        for (int i = 0; i < numBlocks; ++i)
            testPlugin.processBlock (buffer, midi);

        // Should still be at 0.0
        CHECK (testPlugin.apvts.getParameter (Parameters::crossfade.getParamID())->getValue() == Catch::Approx (0.0f));

        // Now give Sidechain some signal
        for (int i = 0; i < 512; ++i) {
            buffer.setSample (2, i, 1.0f); // Sidechain Left
            buffer.setSample (3, i, 1.0f); // Sidechain Right
        }

        // Process another 0.15s (should trigger)
        for (int i = 0; i < numBlocks; ++i)
            testPlugin.processBlock (buffer, midi);

        // Should be automating now. Wait 0.2s more for automation to complete.
        numBlocks = (int) (0.2 * 44100.0 / 512.0) + 1;
        for (int i = 0; i < numBlocks; ++i)
            testPlugin.processBlock (buffer, midi);

        // Target should be 1.0 (Sidechain)
        CHECK (testPlugin.apvts.getParameter (Parameters::crossfade.getParamID())->getValue() == Catch::Approx (1.0f));

        // --- NEW TEST: Both sides silent ---
        // Sidechain is currently at 1.0 (active), but both inputs are silent.
        // It should NOT switch back to Main because Main is also silent.
        
        // Wait another 0.5s with both silent
        numBlocks = (int) (0.5 * 44100.0 / 512.0) + 1;
        for (int i = 0; i < numBlocks; ++i)
            testPlugin.processBlock (buffer, midi);

        // Should still be at 1.0
        CHECK (testPlugin.apvts.getParameter (Parameters::crossfade.getParamID())->getValue() == Catch::Approx (1.0f));

        // Now give Main some signal
        for (int i = 0; i < 512; ++i) {
            buffer.setSample (0, i, 1.0f); // Main Left
            buffer.setSample (1, i, 1.0f); // Main Right
        }

        // Process another 0.15s (should trigger back to Main)
        numBlocks = (int) (0.15 * 44100.0 / 512.0) + 1;
        for (int i = 0; i < numBlocks; ++i)
            testPlugin.processBlock (buffer, midi);

        // Wait for automation
        numBlocks = (int) (0.2 * 44100.0 / 512.0) + 1;
        for (int i = 0; i < numBlocks; ++i)
            testPlugin.processBlock (buffer, midi);

        // Target should be 0.0 (Main)
        CHECK (testPlugin.apvts.getParameter (Parameters::crossfade.getParamID())->getValue() == Catch::Approx (0.0f));
    }
}


#ifdef PAMPLEJUCE_IPP
    #include <ipp.h>

TEST_CASE ("IPP version", "[ipp]")
{
    #if defined(__APPLE__)
        // macOS uses 2021.9.1 from pip wheel (only x86_64 version available)
        CHECK_THAT (ippsGetLibVersion()->Version, Catch::Matchers::Equals ("2021.9.1 (r0x7e208212)"));
    #else
        CHECK_THAT (ippsGetLibVersion()->Version, Catch::Matchers::Equals ("2022.3.0 (r0x0fc08bb1)"));
    #endif
}
#endif
