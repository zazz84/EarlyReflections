/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
CircularBuffer::CircularBuffer()
{
}

void CircularBuffer::init(int size)
{
	m_head = 0;
	m_size = size;

	m_buffer = NULL;
	m_buffer = new float[size];
	memset(m_buffer, 0, size * sizeof(float));
}

void CircularBuffer::clear()
{
	m_head = 0;
	memset(m_buffer, 0, sizeof(m_buffer));
}

float CircularBuffer::readDelay(int sample)
{
	const int bufferSize = m_size;
	int readIdx = m_head + bufferSize - sample;

	if (readIdx >= bufferSize)
		readIdx = readIdx - bufferSize;

	return m_buffer[readIdx];
}

float CircularBuffer::readFactor(float factor)
{
	float sample = (int)(2.0f + m_size * factor * 0.98f);
	return readDelay(sample);
}

//==============================================================================
SimpleDelay::SimpleDelay()
{
}

float SimpleDelay::process(float in)
{
	float bufferOut = m_buffer.readFactor(m_factor);
	m_last = m_a0 * (in + bufferOut * m_feedback) + m_b1 * m_last;	
	m_buffer.writeSample(m_last);
	
	return m_attenuation * bufferOut;
}

//==============================================================================
const std::string EarlyReflectionsAudioProcessor::paramsNames[] = { "Size", "Absorbtion", "Attenuation", "Resonance", "Mix", "Volume" };

EarlyReflectionsAudioProcessor::EarlyReflectionsAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
	sizeParameter		 = apvts.getRawParameterValue(paramsNames[0]);
	absorbtionParameter  = apvts.getRawParameterValue(paramsNames[1]);
	attenuationParameter = apvts.getRawParameterValue(paramsNames[2]);
	resonanceParameter   = apvts.getRawParameterValue(paramsNames[3]);
	mixParameter         = apvts.getRawParameterValue(paramsNames[4]);
	volumeParameter      = apvts.getRawParameterValue(paramsNames[5]);

	buttonAParameter = static_cast<juce::AudioParameterBool*>(apvts.getParameter("ButtonA"));
	buttonBParameter = static_cast<juce::AudioParameterBool*>(apvts.getParameter("ButtonB"));
	buttonCParameter = static_cast<juce::AudioParameterBool*>(apvts.getParameter("ButtonC"));
}

EarlyReflectionsAudioProcessor::~EarlyReflectionsAudioProcessor()
{
}

//==============================================================================
const juce::String EarlyReflectionsAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool EarlyReflectionsAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool EarlyReflectionsAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool EarlyReflectionsAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double EarlyReflectionsAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int EarlyReflectionsAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int EarlyReflectionsAudioProcessor::getCurrentProgram()
{
    return 0;
}

void EarlyReflectionsAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String EarlyReflectionsAudioProcessor::getProgramName (int index)
{
    return {};
}

void EarlyReflectionsAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void EarlyReflectionsAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
	int samplesMax = MINIMUM_BUFFER_SIZE + int(m_hallDelayTimes[N_HALL_DELAY_LINES - 1] * ROOM_SIZE_MAX * sampleRate);

	// TO DO: Optimize delay lines sizes
	for (int i = 0; i < N_HALL_DELAY_LINES; i++)
	{
		m_delayLine[0][i].init(samplesMax, (int)sampleRate);
		m_delayLine[1][i].init(samplesMax + STEREO_ADDITION, (int)sampleRate);
	}
	
	clearCircularBuffers();
}

void EarlyReflectionsAudioProcessor::releaseResources()
{
	clearCircularBuffers();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool EarlyReflectionsAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void EarlyReflectionsAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
	// Get params
	const float size = 0.01f + 0.99f * sizeParameter->load();
	const float absorbtion = absorbtionParameter->load();
	const float attenuation = attenuationParameter->load();
	const float resonance = resonanceParameter->load();
	const float mix = mixParameter->load();
	const float volume = juce::Decibels::decibelsToGain(volumeParameter->load());

	// Buttons
	const auto buttonA = buttonAParameter->get();
	const auto buttonB = buttonBParameter->get();
	const auto buttonC = buttonCParameter->get();

	// Constants
	const int channels = getTotalNumOutputChannels();
	const int samples = buffer.getNumSamples();
	const float mixInverse = 1.0f - mix;
	
	// Early reflection setup
	int delaLinesCount = 0;
	float volumeCompensation = 1.0f;
	const float timeMax = m_hallDelayTimes[N_HALL_DELAY_LINES - 1];
	const float attenuationInverse = 1.0f - attenuation;
	const float *times;
	const float *gains;

	if (buttonA)
	{
		delaLinesCount = N_ROOM_DELAY_LINES;
		times = m_roomDelayTimes;
		gains = m_roomDelayGains;
	}
	else if(buttonB)
	{
		delaLinesCount = N_HALL_DELAY_LINES;
		volumeCompensation = 0.75;
		times = m_hallDelayTimes;
		gains = m_hallDelayGains;
	}
	else
	{
		delaLinesCount = N_HALL_ECO_DELAY_LINES;
		volumeCompensation = 0.6;
		times = m_hallEcoDelayTimes;
		gains = m_hallEcoDelayGains;
	}

	for (int i = 0; i < delaLinesCount; i++)
	{
		const float gain = volumeCompensation * (gains[i] + (1.0f - gains[i]) * attenuationInverse);
		const float factor = size * times[i] / timeMax;

		m_delayLine[0][i].set(factor, absorbtion, gain, resonance);
		m_delayLine[1][i].set(factor, absorbtion, gain, resonance);
	}

	// Process samples
	for (int channel = 0; channel < channels; ++channel)
	{
		auto* channelBuffer = buffer.getWritePointer(channel);

		for (int sample = 0; sample < samples; ++sample)
		{
			const float in = channelBuffer[sample];

			float out = 0.0f;

			for (int i = 0; i < delaLinesCount; i++)
			{
				out += m_delayLine[channel][i].process(in);
			}

			channelBuffer[sample] = volume * (mix * out + mixInverse * in);
		}
	}
}

//==============================================================================
bool EarlyReflectionsAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* EarlyReflectionsAudioProcessor::createEditor()
{
    return new EarlyReflectionsAudioProcessorEditor (*this, apvts);
}

//==============================================================================
void EarlyReflectionsAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
	auto state = apvts.copyState();
	std::unique_ptr<juce::XmlElement> xml(state.createXml());
	copyXmlToBinary(*xml, destData);
}

void EarlyReflectionsAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
	std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

	if (xmlState.get() != nullptr)
		if (xmlState->hasTagName(apvts.state.getType()))
			apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorValueTreeState::ParameterLayout EarlyReflectionsAudioProcessor::createParameterLayout()
{
	APVTS::ParameterLayout layout;

	using namespace juce;

	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[0], paramsNames[0], NormalisableRange<float>(  0.0f, 1.0f,  0.01f, 1.0f), 0.5f));
	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[1], paramsNames[1], NormalisableRange<float>(  0.0f,  1.0f, 0.01f, 1.0f), 0.0f));
	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[2], paramsNames[2], NormalisableRange<float>(  0.0f,  1.0f, 0.01f, 1.0f), 1.0f));
	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[3], paramsNames[3], NormalisableRange<float>(  0.0f,  1.0f, 0.01f, 1.0f), 0.0f));
	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[4], paramsNames[4], NormalisableRange<float>(  0.0f,  1.0f, 0.01f, 1.0f), 0.5f));
	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[5], paramsNames[5], NormalisableRange<float>(-12.0f, 12.0f,  0.1f, 1.0f), 0.0f));

	layout.add(std::make_unique<juce::AudioParameterBool>("ButtonA", "ButtonA", true));
	layout.add(std::make_unique<juce::AudioParameterBool>("ButtonB", "ButtonB", false));
	layout.add(std::make_unique<juce::AudioParameterBool>("ButtonC", "ButtonC", false));

	return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EarlyReflectionsAudioProcessor();
}
