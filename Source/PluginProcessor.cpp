/*
  ==============================================================================

  LowPassFilter - A low-pass filter VST Plugin

  Edward Storey

  A low-pass filter VST plugin written using the JUCE and VST SDK 2.4 frameworks. Based
  on the filter algorithm found in DAFX - Digital Audio Effects, U. Zolzer. .

  Much of the code here is auto-generated by Projucer, for PluginProcessor.cpp the contents
  of LowPassFilterAudioProcessor and processBlock have been edited, with only 3 lines of
  code in processBlock left.
  The calculateCoefficients and resizeBuffers functions have been added with original content
  and math.h was included.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "math.h"


//==============================================================================
LowPassFilterAudioProcessor::LowPassFilterAudioProcessor()
{
	QFactor = 2.0;
	Frequency = 500.0;
}

LowPassFilterAudioProcessor::~LowPassFilterAudioProcessor()
{
}

//==============================================================================
const String LowPassFilterAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool LowPassFilterAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool LowPassFilterAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

double LowPassFilterAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int LowPassFilterAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int LowPassFilterAudioProcessor::getCurrentProgram()
{
    return 0;
}

void LowPassFilterAudioProcessor::setCurrentProgram (int index)
{
}

const String LowPassFilterAudioProcessor::getProgramName (int index)
{
    return String();
}

void LowPassFilterAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================
void LowPassFilterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
}

void LowPassFilterAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

void LowPassFilterAudioProcessor::calculateCoefficients()
{
	// Here we calculate the filter coeficients based on equations found in:
	// DAFx Chapter 2.2 page 33
	float pi = 3.141592653589793;
	float dampFactor = 1/(2*QFactor);
	float C = 1 / (tan(pi*Frequency / getSampleRate()));
	b0 = 1 / (1 + 2*dampFactor*C + C*C);
	b1 = 2 * b0;
	b2 = b0;
	a0 = 2 * b0*(1 - C*C);
	a1 = b0*(1 - 2*dampFactor*C + C*C);
	QCheck = QFactor;
	FreqCheck = Frequency;
	FS = getSampleRate();
}

void LowPassFilterAudioProcessor::resizeBuffers(AudioSampleBuffer& buffer)
{
	// If the widow size is changed resize the buffers 
	filterBufferL.setSize(1, buffer.getNumSamples()*2, false, true, true);
	filterBufferR.setSize(1, buffer.getNumSamples() * 2, false, true, true);
	unfilterBufferL.setSize(1, buffer.getNumSamples() * 2, false, true, true);
	unfilterBufferR.setSize(1, buffer.getNumSamples() * 2, false, true, true);
	windowSize = buffer.getNumSamples();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool LowPassFilterAudioProcessor::setPreferredBusArrangement (bool isInput, int bus, const AudioChannelSet& preferredSet)
{
    // Reject any bus arrangements that are not compatible with your plugin

    const int numChannels = preferredSet.size();

   #if JucePlugin_IsMidiEffect
    if (numChannels != 0)
        return false;
   #elif JucePlugin_IsSynth
    if (isInput || (numChannels != 1 && numChannels != 2))
        return false;
   #else
    if (numChannels != 1 && numChannels != 2)
        return false;

    if (! AudioProcessor::setPreferredBusArrangement (! isInput, bus, preferredSet))
        return false;
   #endif

    return AudioProcessor::setPreferredBusArrangement (isInput, bus, preferredSet);
}
#endif

void LowPassFilterAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
	// Auto generated code by JUCE used to clear any extra channels not in use
    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

	// All code from here to the end of the processing loop is designed by Ed Storey

	// If the selected filter frequency is above the nyquist frequency force it below the threshold
	if (Frequency >(getSampleRate() / 2))
	{
		Frequency = (getSampleRate() / 2) * 0.999;
		FreqCheck = Frequency;
	}

	// If the window size generated from the DAW changes resize the buffers
	if (windowSize != buffer.getNumSamples())resizeBuffers(buffer);

	// If the sampling rate, Q factor or frequency change re-calculate the filter coeficients
	if (FS != getSampleRate() || QFactor != QCheck || Frequency != FreqCheck)calculateCoefficients();

	// Get current read pointers
	filterBufferLrp = filterBufferL.getReadPointer(0);
	filterBufferRrp = filterBufferR.getReadPointer(0);
	unfilterBufferLrp = unfilterBufferL.getReadPointer(0);
	unfilterBufferRrp = unfilterBufferR.getReadPointer(0);

	// Get current write pointers
	filterBufferLwp = filterBufferL.getWritePointer(0);
	filterBufferRwp = filterBufferR.getWritePointer(0);
	unfilterBufferLwp = unfilterBufferL.getWritePointer(0);
	unfilterBufferRwp = unfilterBufferR.getWritePointer(0);

	// create input and output channels to read from and write to
	const float *inL = buffer.getReadPointer(0);
	const float *inR = buffer.getReadPointer(1);
	float *outL = {};
	outL = buffer.getWritePointer(0);
	float *outR = {};
	outR = buffer.getWritePointer(1);

	// Loop through the current input window
	for (int i = 0; i < buffer.getNumSamples(); i++)
    {
		// The processing buffer is twice the size of the input buffer
		// here we calculate n as being either i + 0 or i + the length of the input buffer
		// the length of the input buffer is equivalent to half the size of the processing buffer.
		int n = i + bidx*buffer.getNumSamples();
		int n_1 = n - 1;
		int n_2 = n - 2;

		// Store unfiltered input.
		unfilterBufferLwp[n] = inL[i];
		unfilterBufferRwp[n] = inR[i];

		// If n < 2 wrap either n-1 or n-2 around the circular buffer.
		if (n == 0)
		{
			n_1 = filterBufferL.getNumSamples() - 1;
			n_2 = filterBufferL.getNumSamples() - 2;
		}
		else if (n == 1)
		{
			n_1 = n - 1;
			n_2 = filterBufferL.getNumSamples() - 1;
		}

		// Store the filtered output in an audio buffer from DAFx equation:
		// y(n) = b0x(n) + b1x(n-1) + b2x(n-2) - a0y(n-1) - a1y(n-2)
		filterBufferLwp[n] = b0*unfilterBufferLrp[n] + b1*unfilterBufferLrp[n_1] + b2*unfilterBufferLrp[n_2] - a0*filterBufferLrp[n_1] - a1*filterBufferLrp[n_2];
		filterBufferRwp[n] = b0*unfilterBufferLrp[n] + b1*unfilterBufferLrp[n_1] + b2*unfilterBufferLrp[n_2] - a0*filterBufferRrp[n_1] - a1*filterBufferRrp[n_2];

		// Output the filtered audio to output buffer
		outL[i] = filterBufferLrp[n];
		outR[i] = filterBufferLrp[n];
    }

	// Increase buffer index by 1, if bidx > 1 set to 0
	bidx++;
	if (bidx > 1)bidx = 0;
	// Store current Q, frequency and sample rate data.
	QCheck = QFactor;
	FreqCheck = Frequency;
	FS = getSampleRate();
}

//==============================================================================
bool LowPassFilterAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* LowPassFilterAudioProcessor::createEditor()
{
    return new LowPassFilterAudioProcessorEditor (*this);
}

//==============================================================================
void LowPassFilterAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void LowPassFilterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LowPassFilterAudioProcessor();
}
