/*
  ==============================================================================

    Room times calculation:

	// Room Delay
	const float speedOfSound = 343.0f;

	// Max dimensions
	const float h = pow(EarlyReflectionsAudioProcessor::m_roomSizeMax / 3.65f, 1.0f / 3.0f);
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

	Room gains calculation:

	gain = 0.1224249 + 0.8558602 * exp(-40.69983 * time);

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
class CircularBuffer
{
public:
	CircularBuffer();

	void init(int size);
	void clear();
	void writeSample(float sample)
	{
		m_buffer[m_head] = sample;
		if (++m_head >= m_size)
			m_head = 0;
	}
	float read() const
	{
		return m_buffer[m_head];
	}
	float readDelay(int sample);
	float readFactor(float factor);

protected:
	float *m_buffer;
	int m_head = 0;
	int m_size = 0;
};

//==============================================================================
class SimpleDelay
{
public:
	SimpleDelay();

	void init(int size, int sampleRate)
	{
		m_buffer.init(size);
	}
	float process(float in);
	void clear() { m_buffer.clear(); }
	void setAbsorbtion(float absorbtion)
	{
		const float mel = 100.0f + (1.0f - absorbtion) * 3600.0f;
		const float f = 700.0f * (expf(mel / 1127.0f) - 1.0f);
		m_a0 = limit(powf(f / 20000.0f, 0.6f), 0.0f, 1.0f);
		m_b1 = 1.0f - m_a0;
	}
	void set(float factor, float absorbtion, float attenuation, float feedback)
	{
		m_factor = factor;
		setAbsorbtion(absorbtion);
		m_attenuation = attenuation;
		m_feedback = feedback;
	}
	float limit(float a, float min, float max)
	{
		return std::fmaxf(std::fminf(a, max), min);
	}

private:
	CircularBuffer m_buffer = CircularBuffer();

	float m_feedback = 0.0f;
	float m_attenuation = 1.0f;
	float m_factor = 1.0f;
	
	float m_last = 0.0f;
	float m_a0 = 1.0f;
	float m_b1 = 0.0f;
};

//==============================================================================
class EarlyReflectionsAudioProcessor  : public juce::AudioProcessor
                            #if JucePlugin_Enable_ARA
                             , public juce::AudioProcessorARAExtension
                            #endif
{
public:
    //==============================================================================
    EarlyReflectionsAudioProcessor();
    ~EarlyReflectionsAudioProcessor() override;

	static const std::string paramsNames[];
	static const int N_ROOM_DELAY_LINES = 7;
	static const int N_HALL_ECO_DELAY_LINES = 6;
	static const int N_HALL_DELAY_LINES = 18;
	static const int STEREO_ADDITION = 30;
	static const int MINIMUM_BUFFER_SIZE = 10;
	
	static const int ROOM_SIZE_MAX = 2;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
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
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

	using APVTS = juce::AudioProcessorValueTreeState;
	static APVTS::ParameterLayout createParameterLayout();

	APVTS apvts{ *this, nullptr, "Parameters", createParameterLayout() };

private:
    //==============================================================================
	void clearCircularBuffers()
	{
		for (int i = 0; i < N_HALL_DELAY_LINES; i++)
		{
			m_delayLine[0][i].clear();
			m_delayLine[1][i].clear();
		}
	}
	
	//==============================================================================
	std::atomic<float>* sizeParameter = nullptr;
	std::atomic<float>* absorbtionParameter = nullptr;
	std::atomic<float>* attenuationParameter = nullptr;
	std::atomic<float>* resonanceParameter = nullptr;
	std::atomic<float>* mixParameter = nullptr;
	std::atomic<float>* volumeParameter = nullptr;

	juce::AudioParameterBool* buttonAParameter = nullptr;
	juce::AudioParameterBool* buttonBParameter = nullptr;
	juce::AudioParameterBool* buttonCParameter = nullptr;

	SimpleDelay m_delayLine[2][N_HALL_DELAY_LINES] = {};

	const float m_roomDelayTimes[N_ROOM_DELAY_LINES] = {
													0.0145f,
													0.0187f,
													0.0233f,
													0.0242f,
													0.0387f,
													0.0303f,
													0.0405f,
	};

	const float m_roomDelayGains[N_ROOM_DELAY_LINES] = {
													0.5968f,
													0.5228f,
													0.4540f,
													0.4421f,
													0.2996f,
													0.3718f,
													0.2871f
	};

	const float m_hallEcoDelayTimes[N_HALL_ECO_DELAY_LINES] = {
													0.0199f,
													0.0354f,
													0.0389f,
													0.0414f,
													0.0699f,
													0.0796f,
	};

	const float m_hallEcoDelayGains[N_HALL_ECO_DELAY_LINES] = {
													1.200f,
													0.818f,
													0.635f,
													0.719f,
													0.267f,
													0.242f
	};

	const float m_hallDelayTimes[N_HALL_DELAY_LINES] = {
													0.0043f,
													0.0215f,
													0.0225f,
													0.0268f,
													0.0270f,
													0.0298f,
													0.0458f,
													0.0485f,
													0.0572f,
													0.0587f,
													0.0595f,
													0.0612f,
													0.0707f,
													0.0708f,
													0.0726f,
													0.0741f,
													0.0753f,
													0.0797f
	};

	const float m_hallDelayGains[N_HALL_DELAY_LINES] = {
													0.841f,
													0.504f,
													0.491f,
													0.379f,
													0.380f,
													0.346f,
													0.289f,
													0.272f,
													0.192f,
													0.193f,
													0.217f,
													0.181f,
													0.180f,
													0.181f,
													0.176f,
													0.142f,
													0.167f,
													0.134f
	};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EarlyReflectionsAudioProcessor)
};
