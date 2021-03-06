/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SimpleEQAudioProcessor::SimpleEQAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
      )
#endif
{
}

SimpleEQAudioProcessor::~SimpleEQAudioProcessor()
{
}

//==============================================================================
const juce::String SimpleEQAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SimpleEQAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool SimpleEQAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool SimpleEQAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double SimpleEQAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SimpleEQAudioProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
              // so this should be at least 1, even if you're not really implementing programs.
}

int SimpleEQAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SimpleEQAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String SimpleEQAudioProcessor::getProgramName(int index)
{
    return {};
}

void SimpleEQAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
}

//==============================================================================
int getOrderFromSlope(int order)
{
    switch (order)
    {
    case 0:
        return 2;
    case 1:
        return 4;
    case 2:
        return 6;
    case 3:
        return 8;
    }
}

void SimpleEQAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..

    juce::dsp::ProcessSpec spec;

    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;
    spec.sampleRate = sampleRate;

    leftChain.prepare(spec);
    rightChain.prepare(spec);

    auto chainSettings = getChainSettings(apvts);

    updatePeakFilter(chainSettings);

    auto lowCoefficients = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowFreq,
                                                                                                       sampleRate,
                                                                                                       getOrderFromSlope(chainSettings.lowSlope));

    auto &leftLow = leftChain.get<ChainPositions::Low>();
    updateCutFilter(leftLow, lowCoefficients, static_cast<Slope>(chainSettings.lowSlope));

    auto &rightLow = rightChain.get<ChainPositions::Low>();
    updateCutFilter(rightLow, lowCoefficients, static_cast<Slope>(chainSettings.lowSlope));


    auto highCoefficients = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(chainSettings.highFreq,
                                                                                                       sampleRate,
                                                                                                       getOrderFromSlope(chainSettings.lowSlope));

    auto &leftHigh = leftChain.get<ChainPositions::High>();
    updateCutFilter(leftHigh, highCoefficients, static_cast<Slope>(chainSettings.highSlope));

    auto &rightHigh = rightChain.get<ChainPositions::High>();
    updateCutFilter(rightHigh, highCoefficients, static_cast<Slope>(chainSettings.highSlope));
}

void SimpleEQAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SimpleEQAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

        // This checks if the input layout matches the output layout
#if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif

void SimpleEQAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Update Parameters before Processing Audio!

    auto chainSettings = getChainSettings(apvts);

    updatePeakFilter(chainSettings);

    auto lowCoefficients = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowFreq,
                                                                                                       getSampleRate(),
                                                                                                       getOrderFromSlope(chainSettings.lowSlope));

    auto &leftLow = leftChain.get<ChainPositions::Low>();
    updateCutFilter(leftLow, lowCoefficients, static_cast<Slope>(chainSettings.lowSlope));

    auto &rightLow = rightChain.get<ChainPositions::Low>();
    updateCutFilter(rightLow, lowCoefficients, static_cast<Slope>(chainSettings.lowSlope));

    
    auto highCoefficients = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(chainSettings.highFreq,
                                                                                                       getSampleRate(),
                                                                                                       getOrderFromSlope(chainSettings.lowSlope));

    auto &leftHigh = leftChain.get<ChainPositions::High>();
    updateCutFilter(leftHigh, highCoefficients, static_cast<Slope>(chainSettings.highSlope));

    auto &rightHigh = rightChain.get<ChainPositions::High>();
    updateCutFilter(rightHigh, highCoefficients, static_cast<Slope>(chainSettings.highSlope));

    //Process Audio

    juce::dsp::AudioBlock<float> block(buffer);

    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);

    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);

    leftChain.process(leftContext);
    rightChain.process(rightContext);
}

//==============================================================================
bool SimpleEQAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor *SimpleEQAudioProcessor::createEditor()
{
    //return new SimpleEQAudioProcessorEditor(*this);
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void SimpleEQAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void SimpleEQAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState &aptvs)
{
    ChainSettings settings;

    settings.lowFreq = aptvs.getRawParameterValue("Low Freq")->load();
    settings.highFreq = aptvs.getRawParameterValue("High Freq")->load();
    settings.peakFreq = aptvs.getRawParameterValue("Peak Freq")->load();
    settings.peakGain = aptvs.getRawParameterValue("Peak Gain")->load();
    settings.peakQuality = aptvs.getRawParameterValue("Q")->load();
    settings.lowSlope = static_cast<Slope>(aptvs.getRawParameterValue("Low Slope")->load());
    settings.highSlope = static_cast<Slope>(aptvs.getRawParameterValue("High Slope")->load());

    return settings;
}

void SimpleEQAudioProcessor::updatePeakFilter(const ChainSettings &chainSettings)
{
    auto peakCoefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(getSampleRate(), chainSettings.peakFreq,
                                                                                chainSettings.peakQuality,
                                                                                juce::Decibels::decibelsToGain(chainSettings.peakGain));

    updateCoefficients(leftChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
    updateCoefficients(rightChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
}

void SimpleEQAudioProcessor::updateCoefficients(Coefficients &old, const Coefficients &updated)
{
    *old = *updated;
}

juce::AudioProcessorValueTreeState::ParameterLayout
SimpleEQAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>("Low Freq",
                                                           "Low Freq",
                                                           juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f),
                                                           20.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("High Freq",
                                                           "High Freq",
                                                           juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f),
                                                           20000.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Freq",
                                                           "Peak Freq",
                                                           juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f),
                                                           750.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Gain",
                                                           "Peak Gain",
                                                           juce::NormalisableRange<float>(-24.f, 24.f, 0.5f, 1.f),
                                                           0.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Q",
                                                           "Q",
                                                           juce::NormalisableRange<float>(0.1f, 10.f, 0.05f, 1.f),
                                                           1.f));

    juce::StringArray slopes;
    slopes.add("12 db/oct");
    slopes.add("24 db/oct");
    slopes.add("36 db/oct");
    slopes.add("48 db/oct");

    layout.add(std::make_unique<juce::AudioParameterChoice>("Low Slope", "Low Slope", slopes, 0));

    layout.add(std::make_unique<juce::AudioParameterChoice>("High Slope", "High Slope", slopes, 0));

    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleEQAudioProcessor();
}
