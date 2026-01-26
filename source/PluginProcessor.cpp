#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PluginProcessor::PluginProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                       .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

PluginProcessor::~PluginProcessor()
{
}

//==============================================================================
const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "crossfade", 1 },
                                                            "Crossfade",
                                                            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),
                                                            0.5f));

    return layout;
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    juce::ignoreUnused (sampleRate, samplesPerBlock);
}

void PluginProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    // Sidechain must be mono or stereo (or disabled)
    if (layouts.inputBuses.size() > 1)
    {
        const auto& sidechain = layouts.inputBuses.getReference (1);

        // For AU, we might need to be more permissive or strictly match expectations
        if (sidechain != juce::AudioChannelSet::mono()
         && sidechain != juce::AudioChannelSet::stereo()
         && ! sidechain.isDisabled())
            return false;
    }

    // Logic Pro / AU specific: Some versions expect the sidechain to be able to match the main input
    // and if we are too strict, it might not show up.
    // Also, ensure we don't return false for layouts Logic Pro likes to test.
    
    // Explicitly support AU's common sidechain configurations
    #if JucePlugin_Build_AU
    if (layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() 
     && (layouts.inputBuses.size() > 1 && layouts.inputBuses.getReference(1) == juce::AudioChannelSet::stereo()))
        return true;
    #endif

    return true;
  #endif
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    auto mainInput = getBusBuffer (buffer, true, 0);
    auto sidechainInput = getBusBuffer (buffer, true, 1);

    // Update levels
    auto updateLevels = [] (const juce::AudioBuffer<float>& busBuffer, std::atomic<float>& left, std::atomic<float>& right) {
        if (busBuffer.getNumChannels() > 0)
        {
            left.store (busBuffer.getMagnitude (0, 0, busBuffer.getNumSamples()));
            if (busBuffer.getNumChannels() > 1)
                right.store (busBuffer.getMagnitude (1, 0, busBuffer.getNumSamples()));
            else
                right.store (left.load());
        }
        else
        {
            left.store (0.0f);
            right.store (0.0f);
        }
    };

    updateLevels (mainInput, mainLevelLeft, mainLevelRight);
    updateLevels (sidechainInput, sidechainLevelLeft, sidechainLevelRight);

    float crossfadeValue = 0.5f;
    if (auto* param = apvts.getParameter ("crossfade"))
        crossfadeValue = param->getValue();

    float mainGain = std::cos (crossfadeValue * juce::MathConstants<float>::halfPi);
    float sidechainGain = std::sin (crossfadeValue * juce::MathConstants<float>::halfPi);

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* out = buffer.getWritePointer (channel);
        auto* main = mainInput.getReadPointer (juce::jmin (channel, mainInput.getNumChannels() - 1));

        if (sidechainInput.getNumChannels() > 0)
        {
            auto* side = sidechainInput.getReadPointer (juce::jmin (channel, sidechainInput.getNumChannels() - 1));

            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                out[sample] = main[sample] * mainGain + side[sample] * sidechainGain;
            }
        }
        else
        {
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                out[sample] = main[sample] * mainGain;
            }
        }
    }
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
