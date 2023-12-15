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
	m_buffer.setSize(1, size);
}

void CircularBuffer::clear()
{
	m_head = 0;
	m_buffer.clear();
}

float CircularBuffer::readDelay(float sample)
{
	const int bufferSize = m_size;
	const float readIdx = m_head + bufferSize - sample;

	const int flr = static_cast<int>(readIdx);
	const int iPrev = flr < bufferSize ? flr : flr - bufferSize;
	int iNext = flr + 1;
	iNext = iNext < bufferSize ? iNext : iNext - bufferSize;

	const float weight = readIdx - flr;
	return m_buffer.getSample(0, iPrev) * (1.f - weight) + m_buffer.getSample(0, iNext) * weight;
}

float CircularBuffer::readFactor(float factor)
{
	float sample = 2.0f + m_size * factor * 0.98f;
	return readDelay(sample);
}

//==============================================================================
FirstOrderAllPass::FirstOrderAllPass()
{
}

void FirstOrderAllPass::setCoefFrequency(float frequency)
{
	if (m_SampleRate == 0)
	{
		return;
	}

	const float tmp = tanf(3.14 * frequency / m_SampleRate);
	m_a1 = (tmp - 1.0f) / (tmp + 1.0f);
}

float FirstOrderAllPass::process(float in)
{
	const float tmp = m_a1 * in + m_d;
	m_d = in - m_a1 * tmp;
	return tmp;
}

//==============================================================================
SimpleDelay::SimpleDelay()
{
}

float SimpleDelay::processSample(float inSample)
{
	float bufferOut = m_buffer.readFactor(m_factor);
	m_last = m_attenuation * (m_a0 * (m_inputFilter.process(inSample) + m_feedbackFilter.process(bufferOut) * m_feedback * m_attenuation) + m_b1 * m_last);
	m_buffer.writeSample(m_last);
	
	return bufferOut;
}

//==============================================================================
const float EarlyReflectionsAudioProcessor::m_sizeMin = 10.0f;
const float EarlyReflectionsAudioProcessor::m_sizeMax = 1000.0f;
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
	const float speedOfSound = 343.0f;
	
	// Max dimensions
	const float h = pow(EarlyReflectionsAudioProcessor::m_sizeMax / 3.65f, 1.0f / 3.0f);
	const float w = 1.6f * h;
	const float d = 2.66f * h;

	// Axial
	const float axialHeightTime = h / speedOfSound;
	const float axialWidthTime = w / speedOfSound;
	const float axialDepthTime = d / speedOfSound;

	// Tangential
	const float tangentialHorizontal1Time = sqrt(d * d + 4.0f * w * w) / speedOfSound;
	const float tangentialHorizontal2Time = sqrt(w * w + 4.0f * d * d) / speedOfSound;

	const float tangentialVertical1Time = sqrt(d * d + 4.0f * h * h) / speedOfSound;
	const float tangentialVertical2Time = sqrt(w * w + 4.0f * h * h) / speedOfSound;

	// Set distances
	// TODO: Calculate only once
	m_Distances[0] = h;
	m_Distances[1] = w;
	m_Distances[2] = d;
	m_Distances[3] = sqrt(d * d + 4.0f * w * w);
	m_Distances[4] = sqrt(w * w + 4.0f * d * d);
	m_Distances[5] = sqrt(d * d + 4.0f * h * h);
	m_Distances[6] = sqrt(w * w + 4.0f * h * h);
	
	// Circular buffer init
	// Left channel
	m_SimpleDelay[0][0].init((int)(sampleRate * axialHeightTime), sampleRate);
	m_SimpleDelay[0][1].init((int)(sampleRate * axialWidthTime), sampleRate);
	m_SimpleDelay[0][2].init((int)(sampleRate * axialDepthTime), sampleRate);

	m_SimpleDelay[0][3].init((int)(sampleRate * tangentialHorizontal1Time), sampleRate);
	m_SimpleDelay[0][4].init((int)(sampleRate * tangentialHorizontal2Time), sampleRate);
	m_SimpleDelay[0][5].init((int)(sampleRate * tangentialVertical1Time), sampleRate);
	m_SimpleDelay[0][6].init((int)(sampleRate * tangentialVertical2Time), sampleRate);

	// Right channel
	const float factor = 0.99f;

	m_SimpleDelay[1][0].init((int)(sampleRate * axialHeightTime * factor), sampleRate);
	m_SimpleDelay[1][1].init((int)(sampleRate * axialWidthTime * factor), sampleRate);
	m_SimpleDelay[1][2].init((int)(sampleRate * axialDepthTime * factor), sampleRate);

	m_SimpleDelay[1][3].init((int)(sampleRate * tangentialHorizontal1Time * factor), sampleRate);
	m_SimpleDelay[1][4].init((int)(sampleRate * tangentialHorizontal2Time * factor), sampleRate);
	m_SimpleDelay[1][5].init((int)(sampleRate * tangentialVertical1Time * factor), sampleRate);
	m_SimpleDelay[1][6].init((int)(sampleRate * tangentialVertical2Time * factor), sampleRate);

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
	const float size = sizeParameter->load() / EarlyReflectionsAudioProcessor::m_sizeMax;
	const float absorbtion = absorbtionParameter->load();
	const float attenuation = attenuationParameter->load();
	const float resonance = powf(resonanceParameter->load(), 0.5f);
	const float mix = mixParameter->load();
	const float volume = juce::Decibels::decibelsToGain(volumeParameter->load());

	// Distance attenuation
	const float factor = 0.1f + (1.0f - attenuation) * 100.0f;
	for (int i = 0; i < N_DELAY_LINES; i++)
	{
		m_Attenuations[i] = factor / (m_Distances[i] + factor);
	}

	// Normalize based on m_Attenuations[0]
	for (int i = 0; i < N_DELAY_LINES; i++)
	{
		m_Attenuations[i] /= m_Attenuations[0];
	}

	// Early reflection setup
	for (int i = 0; i < N_DELAY_LINES; i++)
	{
		m_SimpleDelay[0][i].set(size, absorbtion * m_Reflections[i], m_Attenuations[i], powf(resonance, m_Reflections[i]));
		m_SimpleDelay[1][i].set(size, absorbtion * m_Reflections[i], m_Attenuations[i], powf(resonance, m_Reflections[i]));
	}
	
	// Constants
	float mixInverse = 1.0f - mix;
	const int channels = getTotalNumOutputChannels();
	const int samples = buffer.getNumSamples();
	
	for (int channel = 0; channel < channels; ++channel)
	{
		auto* channelBuffer = buffer.getWritePointer(channel);

		for (int sample = 0; sample < samples; ++sample)
		{
			const float in = channelBuffer[sample];

			float out = 0.0f;

			for (int i = 0; i < N_DELAY_LINES; i++)
			{
				out += m_SimpleDelay[channel][i].processSample(in);
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

	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[0], paramsNames[0], NormalisableRange<float>(EarlyReflectionsAudioProcessor::m_sizeMin, EarlyReflectionsAudioProcessor::m_sizeMax, 10.0f, 1.0f), 500.0f));
	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[1], paramsNames[1], NormalisableRange<float>(  0.0f,  1.0f, 0.01f, 1.0f), 0.5f));
	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[2], paramsNames[2], NormalisableRange<float>(  0.0f,  1.0f, 0.01f, 1.0f), 0.5f));
	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[3], paramsNames[3], NormalisableRange<float>(  0.0f,  1.0f, 0.01f, 1.0f), 0.5f));
	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[4], paramsNames[4], NormalisableRange<float>(  0.0f,  1.0f, 0.01f, 1.0f), 0.5f));
	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[5], paramsNames[5], NormalisableRange<float>(-12.0f, 12.0f,  0.1f, 1.0f), 0.0f));

	return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EarlyReflectionsAudioProcessor();
}
