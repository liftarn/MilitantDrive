#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>

/**
 * Custom Tilt-Shelf helper extending juce::dsp::IIR::Coefficients.
 * Pairs inverse-gain LowShelf + HighShelf biquad stages around pivotHz.
 */
struct MilitantTiltShelf
{
    struct TiltPair
    {
        juce::dsp::IIR::Coefficients<float>::Ptr lowShelf;
        juce::dsp::IIR::Coefficients<float>::Ptr highShelf;
    };

    static TiltPair makeTiltShelf(double sampleRate, float pivotFrequencyHz, float tiltGainDb)
    {
        const float halfTiltDb = tiltGainDb * 0.5f;
        const float lowGainLinear  = juce::Decibels::decibelsToGain(-halfTiltDb);
        const float highGainLinear = juce::Decibels::decibelsToGain(halfTiltDb);
        constexpr float shelfQ = 0.55f;

        return {
            juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, pivotFrequencyHz, shelfQ, lowGainLinear),
            juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, pivotFrequencyHz, shelfQ, highGainLinear)
        };
    }
};

class MilitantDriveAudioProcessor final : public juce::AudioProcessor
{
public:
    MilitantDriveAudioProcessor();
    ~MilitantDriveAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    std::atomic<float> inputLevelDb   { -60.0f };
    std::atomic<float> inputRmsDb     { -60.0f };
    std::atomic<float> outputLevelDb  { -60.0f };
    std::atomic<float> outputRmsDb    { -60.0f };
    std::atomic<bool>  outputClipped  { false };

private:
    static float shapeAsymmetricalDiodeSample(float x, float driveGain, float dcBias,
                                              float diodeBlend, bool softVoiceBoost) noexcept;

    void updateTiltFilters();

    using IIRFilter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                     juce::dsp::IIR::Coefficients<float>>;

    IIRFilter tiltLowShelf;
    IIRFilter tiltHighShelf;
    IIRFilter boxCutFilter;

    // Selectable 1x (index 0), 2x (index 1), 4x (index 2) oversamplers
    std::unique_ptr<juce::dsp::Oversampling<float>> oversamplers[3];

    // 1st-order 15 Hz DC blocker state (stereo)
    std::array<float, 2> dcBlockerX1 { 0.0f, 0.0f };
    std::array<float, 2> dcBlockerY1 { 0.0f, 0.0f };

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothDriveGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothDcBias;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothDiodeBlend;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothMix;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothOutputGain;

    double currentSampleRate = 48000.0;
    juce::AudioBuffer<float> dryBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MilitantDriveAudioProcessor)
};
