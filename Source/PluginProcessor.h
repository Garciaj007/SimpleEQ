/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

enum Slope
{
  Slope_12,
  Slope_24,
  Slope_36,
  Slope_48
};

struct ChainSettings
{
  float peakFreq{0}, peakGain{0}, peakQuality{1.f};
  float lowFreq{0}, highFreq{0};
  int lowSlope{Slope::Slope_12}, highSlope{Slope::Slope_12};
};

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState &aptvs);
int getOrderFromSlope(int slope);

//==============================================================================
/**
*/
class SimpleEQAudioProcessor : public juce::AudioProcessor
{
public:
  //==============================================================================
  SimpleEQAudioProcessor();
  ~SimpleEQAudioProcessor() override;

  //==============================================================================
  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
  bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
#endif

  void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

  //==============================================================================
  juce::AudioProcessorEditor *createEditor() override;
  bool hasEditor() const override;

  //==============================================================================
  const juce::String getName() const override;

  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  double getTailLengthSeconds() const override;

  //==============================================================================
  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String &newName) override;

  //==============================================================================
  void getStateInformation(juce::MemoryBlock &destData) override;
  void setStateInformation(const void *data, int sizeInBytes) override;

  static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

  juce::AudioProcessorValueTreeState apvts{*this, nullptr, "Params", createParameterLayout()};

private:
  using Filter = juce::dsp::IIR::Filter<float>;
  using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;
  using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter>;
  using Coefficients = Filter::CoefficientsPtr;

  MonoChain leftChain, rightChain;

  enum ChainPositions
  {
    Low,
    Peak,
    High
  };

  void updatePeakFilter(const ChainSettings &chainSettings);
  static void updateCoefficients(Coefficients &old, const Coefficients &updated);

  template <int Index, typename ChainType, typename CoefficientType>
  void update(ChainType &chain, const CoefficientType &coefficients)
  {
    updateCoefficients(chain.template get<Index>().coefficients, coefficients[Index]);
    chain.template setBypassed<Index>(false);
  }

  template <typename ChainType, typename CoefficientType>
  void updateCutFilter(ChainType &chain, const CoefficientType &coefficients, const Slope &lowSlope)
  {
    chain.template setBypassed<0>(true);
    chain.template setBypassed<1>(true);
    chain.template setBypassed<2>(true);
    chain.template setBypassed<3>(true);

    switch (lowSlope)
    {
    case Slope_48:
      update<3>(chain, coefficients);
    case Slope_36:
      update<2>(chain, coefficients);
    case Slope_24:
      update<1>(chain, coefficients);
    case Slope_12:
      update<0>(chain, coefficients);
    }
  }

  //==============================================================================
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimpleEQAudioProcessor)
};
