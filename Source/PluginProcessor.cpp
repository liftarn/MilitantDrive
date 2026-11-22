#include "PluginProcessor.h"
#include "PluginEditor.h"

MilitantDriveAudioProcessor::MilitantDriveAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "MILITANT_DRIVE_STATE", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout MilitantDriveAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("driveDb", 1), "Drive",
        juce::NormalisableRange<float>(0.0f, 30.0f, 0.1f), 15.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("dcBias", 1), "DC Bias",
        juce::NormalisableRange<float>(-0.80f, 0.80f, 0.01f), 0.28f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("diodeBlend", 1), "Ge / Si Blend",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.25f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tiltDb", 1), "Tilt EQ",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 3.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("pivotHz", 1), "Pivot Freq",
        juce::NormalisableRange<float>(300.0f, 3000.0f, 1.0f, 0.45f), 950.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("boxCutDb", 1), "Box Cut 280Hz",
        juce::NormalisableRange<float>(0.0f, 12.0f, 0.1f), 3.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("mix", 1), "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.90f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("outputDb", 1), "Output",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), -3.5f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("oversampling", 1), "Oversampling",
        juce::StringArray { "1x", "2x", "4x" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("eqRouting", 1), "EQ Routing",
        juce::StringArray { "Pre", "Post" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("softVoiceBoost", 1), "Soft-Voice Boost", true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("bypass", 1), "Bypass", false));

    return { params.begin(), params.end() };
}

void MilitantDriveAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    const auto numChannels = static_cast<size_t>(juce::jmax(1, getTotalNumOutputChannels()));

    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32>(samplesPerBlock),
                                   static_cast<juce::uint32>(numChannels) };

    tiltLowShelf.prepare(spec);
    tiltHighShelf.prepare(spec);
    boxCutFilter.prepare(spec);

    for (int i = 0; i < 3; ++i)
    {
        // Factor exponents: 0 -> 1x, 1 -> 2x, 2 -> 4x
        oversamplers[i] = std::make_unique<juce::dsp::Oversampling<float>>(
            numChannels, static_cast<size_t>(i),
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        oversamplers[i]->initProcessing(static_cast<size_t>(samplesPerBlock));
    }

    dcBlockerX1.fill(0.0f);
    dcBlockerY1.fill(0.0f);

    smoothDriveGain.reset(sampleRate, 0.02);
    smoothDcBias.reset(sampleRate, 0.02);
    smoothDiodeBlend.reset(sampleRate, 0.02);
    smoothMix.reset(sampleRate, 0.02);
    smoothOutputGain.reset(sampleRate, 0.02);

    dryBuffer.setSize(static_cast<int>(numChannels), samplesPerBlock);
    updateTiltFilters();
}

void MilitantDriveAudioProcessor::releaseResources()
{
    for (auto& os : oversamplers)
        if (os) os->reset();
}

bool MilitantDriveAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto outSet = layouts.getMainOutputChannelSet();
    const auto inSet  = layouts.getMainInputChannelSet();

    if (outSet != juce::AudioChannelSet::mono() && outSet != juce::AudioChannelSet::stereo())
        return false;

    return inSet == outSet
        || (inSet == juce::AudioChannelSet::mono() && outSet == juce::AudioChannelSet::stereo());
}

void MilitantDriveAudioProcessor::updateTiltFilters()
{
    const float tiltDb   = apvts.getRawParameterValue("tiltDb")->load();
    const float pivotHz  = apvts.getRawParameterValue("pivotHz")->load();
    const float boxCutDb = apvts.getRawParameterValue("boxCutDb")->load();

    auto tiltCoeffs = MilitantTiltShelf::makeTiltShelf(currentSampleRate, pivotHz, tiltDb);
    *tiltLowShelf.state  = *tiltCoeffs.lowShelf;
    *tiltHighShelf.state = *tiltCoeffs.highShelf;

    *boxCutFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        currentSampleRate, 280.0f, 1.35f, juce::Decibels::decibelsToGain(-boxCutDb));
}

float MilitantDriveAudioProcessor::shapeAsymmetricalDiodeSample(
    float x, float driveGain, float dcBias, float diodeBlend, bool softVoiceBoost) noexcept
{
    // Upward soft-level sensitivity so quiet spoken words bloom into warm crunch
    const float softBoost = softVoiceBoost
        ? (1.0f + 2.4f / (1.0f + 10.0f * std::abs(x)))
        : 1.0f;

    const float u = x * driveGain * softBoost;

    // Germanium stage: soft knee with asymmetrical forward/reverse conduction
    constexpr float kGe = 1.45f;
    const float geAsym = (u >= 0.0f) ? 1.0f : 0.82f;
    const float yGe = std::tanh(kGe * geAsym * (u + dcBias)) - std::tanh(kGe * dcBias);

    // Silicon stage: tighter, harder diode knee
    constexpr float kSi = 3.4f;
    const float siBias = dcBias * 0.6f;
    const float ySi = std::tanh(kSi * (u + siBias)) - std::tanh(kSi * siBias);

    return yGe * (1.0f - diodeBlend) + ySi * diodeBlend;
}

void MilitantDriveAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();
    if (numChannels == 0 || numSamples == 0)
        return;

    // If mono input on a stereo bus, copy L -> R before processing
    if (totalNumInputChannels == 1 && totalNumOutputChannels == 2 && numChannels >= 2)
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);

    // Measure stereo input peak & RMS for GUI VU meter
    float inPeak = 0.0f;
    float inRmsMax = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        inPeak   = juce::jmax(inPeak, buffer.getMagnitude(ch, 0, numSamples));
        inRmsMax = juce::jmax(inRmsMax, buffer.getRMSLevel(ch, 0, numSamples));
    }
    inputLevelDb.store(juce::Decibels::gainToDecibels(inPeak, -60.0f));
    inputRmsDb.store(juce::Decibels::gainToDecibels(inRmsMax, -60.0f));

    if (apvts.getRawParameterValue("bypass")->load() > 0.5f)
    {
        outputLevelDb.store(inputLevelDb.load());
        outputRmsDb.store(inputRmsDb.load());
        outputClipped.store(inPeak >= 0.999f);
        return;
    }

    dryBuffer.makeCopyOf(buffer, true);

    updateTiltFilters();

    const float driveDb = apvts.getRawParameterValue("driveDb")->load();
    smoothDriveGain.setTargetValue(juce::Decibels::decibelsToGain(driveDb));
    smoothDcBias.setTargetValue(apvts.getRawParameterValue("dcBias")->load());
    smoothDiodeBlend.setTargetValue(apvts.getRawParameterValue("diodeBlend")->load());
    smoothMix.setTargetValue(apvts.getRawParameterValue("mix")->load());
    smoothOutputGain.setTargetValue(juce::Decibels::decibelsToGain(
        apvts.getRawParameterValue("outputDb")->load()));

    const int osChoice = juce::jlimit(0, 2, static_cast<int>(apvts.getRawParameterValue("oversampling")->load()));
    const int eqRoute  = static_cast<int>(apvts.getRawParameterValue("eqRouting")->load());
    const bool softBoost = apvts.getRawParameterValue("softVoiceBoost")->load() > 0.5f;

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);

    // Pre-clipping Tilt-EQ & 280 Hz boxy-mid carve
    if (eqRoute == 0)
    {
        tiltLowShelf.process(context);
        tiltHighShelf.process(context);
        boxCutFilter.process(context);
    }

    // Oversampled non-linear waveshaping stage
    auto& activeOs = oversamplers[osChoice];
    auto osBlock = activeOs->processSamplesUp(block);
    const size_t osFactor = size_t { 1 } << osChoice;
    const double osSampleRate = currentSampleRate * static_cast<double>(osFactor);
    const float dcR = 1.0f - static_cast<float>((juce::MathConstants<double>::twoPi * 15.0) / osSampleRate);

    const size_t osNumSamples = osBlock.getNumSamples();
    const size_t chCount = juce::jmin<size_t>(2, osBlock.getNumChannels());

    for (size_t i = 0; i < osNumSamples; ++i)
    {
        // Advance smoothers once per base-rate sample step
        if (i % osFactor == 0)
        {
            smoothDriveGain.getNextValue();
            smoothDcBias.getNextValue();
            smoothDiodeBlend.getNextValue();
        }

        const float dGain = smoothDriveGain.getCurrentValue();
        const float bias  = smoothDcBias.getCurrentValue();
        const float blend = smoothDiodeBlend.getCurrentValue();

        for (size_t ch = 0; ch < chCount; ++ch)
        {
            float* data = osBlock.getChannelPointer(ch);
            const float shaped = shapeAsymmetricalDiodeSample(data[i], dGain, bias, blend, softBoost);

            // 15 Hz DC-blocking high-pass filter removes residual asymmetrical offset
            const float dcFiltered = shaped - dcBlockerX1[ch] + dcR * dcBlockerY1[ch];
            dcBlockerX1[ch] = shaped;
            dcBlockerY1[ch] = dcFiltered;
            data[i] = dcFiltered;
        }
    }

    activeOs->processSamplesDown(block);

    // Post-clipping Tilt-EQ option
    if (eqRoute == 1)
    {
        tiltLowShelf.process(context);
        tiltHighShelf.process(context);
        boxCutFilter.process(context);
    }

    // Parallel Dry/Wet Mix & Output Trim
    for (int i = 0; i < numSamples; ++i)
    {
        const float mix = smoothMix.getNextValue();
        const float outGain = smoothOutputGain.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float dry = dryBuffer.getSample(ch, i);
            const float wet = buffer.getSample(ch, i);
            buffer.setSample(ch, i, (dry * (1.0f - mix) + wet * mix) * outGain);
        }
    }

    float outPeak = 0.0f;
    float outRmsMax = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        outPeak   = juce::jmax(outPeak, buffer.getMagnitude(ch, 0, numSamples));
        outRmsMax = juce::jmax(outRmsMax, buffer.getRMSLevel(ch, 0, numSamples));
    }
    outputLevelDb.store(juce::Decibels::gainToDecibels(outPeak, -60.0f));
    outputRmsDb.store(juce::Decibels::gainToDecibels(outRmsMax, -60.0f));
    outputClipped.store(outPeak >= 0.999f);
}

void MilitantDriveAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void MilitantDriveAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorEditor* MilitantDriveAudioProcessor::createEditor()
{
    return new MilitantDriveAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MilitantDriveAudioProcessor();
}
