/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

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
	void writeSample(float sample)
	{
		m_buffer.setSample(0, m_head, sample);
		if (++m_head >= m_size)
			m_head = 0;
	}
	float Read() const
	{
		return m_buffer.getSample(0, m_head);
	}
	float readDelay(float sample);
	float readFactor(float factor);
	void clear();

protected:
	juce::AudioBuffer<float> m_buffer;
	int m_head = 0;
	int m_size = 0;
};

//==============================================================================
class FirstOrderAllPass
{
public:
	FirstOrderAllPass();

	void init(int sampleRate) { m_SampleRate = sampleRate; }
	void setCoefFrequency(float frequency);
	void setCoef(float coef) { m_a1 = coef; }
	float process(float in);

protected:
	int m_SampleRate;
	float m_a1 = -1.0f; // all pass filter coeficient
	float m_d = 0.0f;   // history d = x[n-1] - a1y[n-1]
};

//==============================================================================
class SimpleDelay
{
public:
	SimpleDelay();

	void init(int size, int sampleRate)
	{
		m_buffer.init(size);

		m_inputFilter.init(sampleRate);
		m_feedbackFilter.init(sampleRate);

		m_inputFilter.setCoefFrequency(size * 0.43f);
		m_feedbackFilter.setCoefFrequency(size);
	}
	float processSample(float inSample);
	void clear() { m_buffer.clear(); }
	void setFeedback(float feedback) { m_feedback = limit(feedback, 0.0f, 1.0f); }
	void setAbsorbtion(float absorbtion)
	{
		const float mel = 100.0f + (1.0f - absorbtion) * 3600.0f;
		const float f = 700.0f * (expf(mel / 1127.0f) - 1.0f);
		m_a0 = limit(powf(f / 20000.0f, 0.6f), 0.0f, 1.0f);
		m_b1 = 1.0f - m_a0;
	}
	void setAtternuation(float attenuation) { m_attenuation = attenuation; }
	void setFactor(float factor) { m_factor = factor; }
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
	float m_feedback = 0.0f;
	float m_attenuation = 1.0f;
	float m_factor = 1.0f;
	CircularBuffer m_buffer = CircularBuffer();
	FirstOrderAllPass m_inputFilter = FirstOrderAllPass();
	FirstOrderAllPass m_feedbackFilter = FirstOrderAllPass();
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
	static const int N_DELAY_LINES = 7;
	static const float m_sizeMin;
	static const float m_sizeMax;


public:
    //==============================================================================
    EarlyReflectionsAudioProcessor();
    ~EarlyReflectionsAudioProcessor() override;

	static const std::string paramsNames[];

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
		for (int i = 0; i < N_DELAY_LINES; i++)
		{
			m_SimpleDelay[0][i].clear();
			m_SimpleDelay[1][i].clear();
		}
	}
	
	//==============================================================================
	std::atomic<float>* sizeParameter = nullptr;
	std::atomic<float>* absorbtionParameter = nullptr;
	std::atomic<float>* attenuationParameter = nullptr;
	std::atomic<float>* resonanceParameter = nullptr;
	std::atomic<float>* bitCrushParameter = nullptr;
	std::atomic<float>* mixParameter = nullptr;
	std::atomic<float>* volumeParameter = nullptr;

	SimpleDelay m_SimpleDelay[2][N_DELAY_LINES] = {};

	float m_Distances[N_DELAY_LINES] = {};
	float m_Reflections[N_DELAY_LINES] = {1, 1, 1, 3, 3, 3, 3};
	float m_Attenuations[N_DELAY_LINES] = {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EarlyReflectionsAudioProcessor)
};
