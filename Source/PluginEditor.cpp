#include "PluginEditor.h"
#include <array>
#include <cmath>

namespace
{
struct FactoryPreset
{
    const char* name;
    float driveDb;
    float dcBias;
    float diodeBlend;
    float tiltDb;
    float pivotHz;
    float boxCutDb;
    float mix;
    float outputDb;
    int   oversamplingIndex; // 0 = 1x, 1 = 2x, 2 = 4x
    int   eqRoutingIndex;    // 0 = pre, 1 = post
    bool  softVoiceBoost;
};

constexpr std::array<FactoryPreset, 5> kFactoryPresets {{
    { "Close-Mic Whisper - Soft Intimate Vocals", 15.5f,  0.28f, 0.25f,  3.5f,  950.0f, 3.5f, 0.90f, -3.5f, 1, 0, true  },
    { "Broadcast Presence - Spoken Word & VO",   11.0f,  0.18f, 0.55f,  5.0f, 1150.0f, 5.5f, 0.85f, -2.5f, 2, 0, true  },
    { "Germanium Velvet - Warm Lo-Fi Vocal",     21.0f, -0.46f, 0.05f, -1.5f,  750.0f, 2.5f, 1.00f, -6.0f, 2, 1, true  },
    { "Drum Room Crunch - Parallel Drum Bus",    24.0f,  0.38f, 0.82f,  2.5f, 1400.0f, 4.0f, 0.45f, -4.0f, 2, 0, false },
    { "DI Bass Bite - Picked & Fingerstyle",      18.0f,  0.34f, 0.65f, -2.0f,  520.0f, 3.0f, 0.72f, -4.5f, 1, 1, false }
}};

constexpr std::array<const char*, 11> kPresetWatchedParamIds {{
    "driveDb", "dcBias", "diodeBlend", "tiltDb", "pivotHz", "boxCutDb",
    "mix", "outputDb", "oversampling", "eqRouting", "softVoiceBoost"
}};

void setParamNormalizedWithGesture(juce::AudioProcessorValueTreeState& apvts,
                                   const juce::String& id,
                                   float plainValue)
{
    if (auto* param = apvts.getParameter(id))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->convertTo0to1(plainValue));
        param->endChangeGesture();
    }
}
} // namespace

MilitantDriveAudioProcessorEditor::MilitantDriveAudioProcessorEditor(MilitantDriveAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    juce::LookAndFeel::setDefaultLookAndFeel(&consoleLookAndFeel);
    setLookAndFeel(&consoleLookAndFeel);

    // Top Header Controls
    presetHeaderLabel.setText("PRESET", juce::dontSendNotification);
    presetHeaderLabel.setJustificationType(juce::Justification::centredRight);
    presetHeaderLabel.setFont(consoleLookAndFeel.makeFont(11.0f, true));
    presetHeaderLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9ba196));
    addAndMakeVisible(presetHeaderLabel);

    for (int i = 0; i < static_cast<int>(kFactoryPresets.size()); ++i)
        presetBox.addItem(juce::String::fromUTF8(kFactoryPresets[static_cast<size_t>(i)].name), i + 1);

    presetBox.setTextWhenNothingSelected("Custom Session");
    presetBox.setTextWhenNoChoicesAvailable("Custom Session");
    presetBox.setJustificationType(juce::Justification::centredLeft);
    presetBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0d0f0d));
    presetBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xffecece6));
    presetBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff343b32));
    presetBox.onChange = [this]
    {
        if (isApplyingPreset)
            return;

        const int selectedId = presetBox.getSelectedId();
        if (selectedId >= 1 && selectedId <= static_cast<int>(kFactoryPresets.size()))
            applyPresetByIndex(selectedId - 1);
    };
    addAndMakeVisible(presetBox);

    osHeaderLabel.setText("OS", juce::dontSendNotification);
    osHeaderLabel.setJustificationType(juce::Justification::centred);
    osHeaderLabel.setFont(consoleLookAndFeel.makeFont(10.0f, true));
    osHeaderLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9ba196));
    addAndMakeVisible(osHeaderLabel);

    auto configureOsBtn = [this](juce::TextButton& btn, const juce::String& text, int osIndex)
    {
        btn.setButtonText(text);
        btn.setComponentID("os_segment");
        btn.setClickingTogglesState(true);
        btn.setRadioGroupId(42);
        btn.onClick = [this, osIndex]
        {
            setParamNormalizedWithGesture(audioProcessor.apvts, "oversampling", static_cast<float>(osIndex));
        };
        addAndMakeVisible(btn);
    };
    configureOsBtn(os1xButton, "1x", 0);
    configureOsBtn(os2xButton, "2x", 1);
    configureOsBtn(os4xButton, "4x", 2);

    resetButton.setButtonText("Reset");
    resetButton.setComponentID("utility_btn");
    resetButton.onClick = [this] { applyPresetByIndex(0); };
    addAndMakeVisible(resetButton);

    // Module 1 & Module 2 Knobs
    configureKnob(driveSlider,      driveLabel,      driveSubLabel,      "DRIVE",   "INPUT GAIN",  15.5, false);
    configureKnob(dcBiasSlider,     dcBiasLabel,     dcBiasSubLabel,     "DC BIAS", "ASYMMETRY",    0.28, true);
    configureKnob(diodeBlendSlider, diodeBlendLabel, diodeBlendSubLabel, "GE / SI", "DIODE BLEND",  0.25, false);
    configureKnob(mixSlider,        mixLabel,        mixSubLabel,        "MIX",     "DRY / WET",    0.90, false);

    configureKnob(tiltSlider,       tiltLabel,       tiltSubLabel,       "TILT EQ", "WARM / CRISP", 3.5,  true);
    configureKnob(pivotSlider,      pivotLabel,      pivotSubLabel,      "PIVOT",   "CENTER FREQ",  950.0, false);
    configureKnob(boxCutSlider,     boxCutLabel,     boxCutSubLabel,     "BOX CUT", "280 HZ DIP",   3.5,  false);

    // Console Linear Fader for Output Trim
    outputFader.setSliderStyle(juce::Slider::LinearVertical);
    outputFader.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 18);
    outputFader.setDoubleClickReturnValue(true, -3.5);
    outputFader.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffecece6));
    outputFader.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(outputFader);

    outputLabel.setText("OUTPUT", juce::dontSendNotification);
    outputLabel.setJustificationType(juce::Justification::centred);
    outputLabel.setFont(consoleLookAndFeel.makeFont(11.0f, true));
    outputLabel.setColour(juce::Label::textColourId, juce::Colour(0xffecece6));
    addAndMakeVisible(outputLabel);

    // Illuminated Push-Buttons
    configurePushButton(softBoostButton, softBoostLabel, "SOFT BOOST", false);
    configurePushButton(eqRouteButton,   eqRouteLabel,   "POST EQ",    false);
    configurePushButton(bypassButton,    bypassLabel,    "BYPASS",     true);

    // APVTS Attachments
    driveAttach      = std::make_unique<SliderAttach>(audioProcessor.apvts, "driveDb", driveSlider);
    dcBiasAttach     = std::make_unique<SliderAttach>(audioProcessor.apvts, "dcBias", dcBiasSlider);
    diodeBlendAttach = std::make_unique<SliderAttach>(audioProcessor.apvts, "diodeBlend", diodeBlendSlider);
    tiltAttach       = std::make_unique<SliderAttach>(audioProcessor.apvts, "tiltDb", tiltSlider);
    pivotAttach      = std::make_unique<SliderAttach>(audioProcessor.apvts, "pivotHz", pivotSlider);
    boxCutAttach     = std::make_unique<SliderAttach>(audioProcessor.apvts, "boxCutDb", boxCutSlider);
    mixAttach        = std::make_unique<SliderAttach>(audioProcessor.apvts, "mix", mixSlider);
    outputAttach     = std::make_unique<SliderAttach>(audioProcessor.apvts, "outputDb", outputFader);

    bypassAttach     = std::make_unique<ButtonAttach>(audioProcessor.apvts, "bypass", bypassButton);
    softBoostAttach  = std::make_unique<ButtonAttach>(audioProcessor.apvts, "softVoiceBoost", softBoostButton);

    // Formatted Readout Suffixes (configured after SliderAttachment so units always display)
    driveSlider.textFromValueFunction = [](double v)
    {
        return juce::String(v, 1) + " dB";
    };

    dcBiasSlider.textFromValueFunction = [](double v)
    {
        return (v > 0.004 ? "+" : "") + juce::String(v, 2);
    };

    diodeBlendSlider.textFromValueFunction = [](double v)
    {
        return juce::String(juce::roundToInt(v * 100.0)) + "%";
    };
    diodeBlendSlider.valueFromTextFunction = [](const juce::String& text)
    {
        return juce::jlimit(0.0, 1.0, text.getDoubleValue() * 0.01);
    };

    mixSlider.textFromValueFunction = [](double v)
    {
        return juce::String(juce::roundToInt(v * 100.0)) + "%";
    };
    mixSlider.valueFromTextFunction = [](const juce::String& text)
    {
        return juce::jlimit(0.0, 1.0, text.getDoubleValue() * 0.01);
    };

    tiltSlider.textFromValueFunction = [](double v)
    {
        return (v > 0.04 ? "+" : "") + juce::String(v, 1) + " dB";
    };

    pivotSlider.textFromValueFunction = [](double v)
    {
        return juce::String(juce::roundToInt(v)) + " Hz";
    };

    boxCutSlider.textFromValueFunction = [](double v)
    {
        return "-" + juce::String(std::abs(v), 1) + " dB";
    };
    boxCutSlider.valueFromTextFunction = [](const juce::String& text)
    {
        return juce::jlimit(0.0, 12.0, std::abs(text.getDoubleValue()));
    };

    outputFader.textFromValueFunction = [](double v)
    {
        return (v > 0.04 ? "+" : "") + juce::String(v, 1) + " dB";
    };

    eqRouteButton.onClick = [this]
    {
        setParamNormalizedWithGesture(audioProcessor.apvts, "eqRouting",
                                      eqRouteButton.getToggleState() ? 1.0f : 0.0f);
    };

    for (const auto* paramId : kPresetWatchedParamIds)
        audioProcessor.apvts.addParameterListener(paramId, this);

    syncControlStatesFromProcessor();
    syncPresetDropdownFromProcessor();

    setSize(940, 445);
    startTimerHz(30);
}

MilitantDriveAudioProcessorEditor::~MilitantDriveAudioProcessorEditor()
{
    stopTimer();
    for (const auto* paramId : kPresetWatchedParamIds)
        audioProcessor.apvts.removeParameterListener(paramId, this);

    setLookAndFeel(nullptr);
    if (&juce::LookAndFeel::getDefaultLookAndFeel() == &consoleLookAndFeel)
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
}

void MilitantDriveAudioProcessorEditor::parameterChanged(const juce::String& parameterID, float)
{
    if (isApplyingPreset || parameterID == "bypass")
        return;

    presetDirty.store(true);
}

void MilitantDriveAudioProcessorEditor::configureKnob(
    juce::Slider& slider, juce::Label& label, juce::Label& sublabel,
    const juce::String& title, const juce::String& subtitle,
    double defaultValue, bool bipolar)
{
    slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 78, 18);
    slider.setDoubleClickReturnValue(true, defaultValue);
    slider.getProperties().set("bipolar", bipolar);
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffecece6));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(slider);

    label.setText(title, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(consoleLookAndFeel.makeFont(11.0f, true));
    label.setColour(juce::Label::textColourId, juce::Colour(0xffecece6));
    addAndMakeVisible(label);

    sublabel.setText(subtitle, juce::dontSendNotification);
    sublabel.setJustificationType(juce::Justification::centred);
    sublabel.setFont(consoleLookAndFeel.makeFont(9.5f, false));
    sublabel.setColour(juce::Label::textColourId, juce::Colour(0xff9ba196));
    addAndMakeVisible(sublabel);
}

void MilitantDriveAudioProcessorEditor::configurePushButton(
    juce::TextButton& btn, juce::Label& label, const juce::String& title, bool isCrimson)
{
    btn.setComponentID(isCrimson ? "illuminated_crimson" : "illuminated");
    btn.setClickingTogglesState(true);
    addAndMakeVisible(btn);

    label.setText(title, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(consoleLookAndFeel.makeFont(10.5f, true));
    label.setColour(juce::Label::textColourId, juce::Colour(0xff9ba196));
    addAndMakeVisible(label);
}

void MilitantDriveAudioProcessorEditor::applyPresetByIndex(int presetIndex)
{
    if (presetIndex < 0 || presetIndex >= static_cast<int>(kFactoryPresets.size()))
        return;

    const juce::ScopedValueSetter<bool> guard(isApplyingPreset, true);
    const auto& preset = kFactoryPresets[static_cast<size_t>(presetIndex)];

    driveSlider.setValue(preset.driveDb, juce::sendNotificationSync);
    dcBiasSlider.setValue(preset.dcBias, juce::sendNotificationSync);
    diodeBlendSlider.setValue(preset.diodeBlend, juce::sendNotificationSync);
    tiltSlider.setValue(preset.tiltDb, juce::sendNotificationSync);
    pivotSlider.setValue(preset.pivotHz, juce::sendNotificationSync);
    boxCutSlider.setValue(preset.boxCutDb, juce::sendNotificationSync);
    mixSlider.setValue(preset.mix, juce::sendNotificationSync);
    outputFader.setValue(preset.outputDb, juce::sendNotificationSync);

    setParamNormalizedWithGesture(audioProcessor.apvts, "oversampling", static_cast<float>(preset.oversamplingIndex));
    setParamNormalizedWithGesture(audioProcessor.apvts, "eqRouting", static_cast<float>(preset.eqRoutingIndex));

    if (softBoostButton.getToggleState() != preset.softVoiceBoost)
        softBoostButton.setToggleState(preset.softVoiceBoost, juce::sendNotificationSync);
    else
        setParamNormalizedWithGesture(audioProcessor.apvts, "softVoiceBoost", preset.softVoiceBoost ? 1.0f : 0.0f);

    // Preserve current Bypass state intentionally when switching presets or resetting
    presetDirty.store(false);
    syncControlStatesFromProcessor();
    presetBox.setSelectedId(presetIndex + 1, juce::dontSendNotification);
}

int MilitantDriveAudioProcessorEditor::detectMatchingPresetId() const noexcept
{
    const int osIdx = juce::jlimit(0, 2, static_cast<int>(std::lround(
        audioProcessor.apvts.getRawParameterValue("oversampling")->load())));
    const int eqIdx = static_cast<int>(std::lround(
        audioProcessor.apvts.getRawParameterValue("eqRouting")->load()));
    const float driveDb    = audioProcessor.apvts.getRawParameterValue("driveDb")->load();
    const float dcBias     = audioProcessor.apvts.getRawParameterValue("dcBias")->load();
    const float diodeBlend = audioProcessor.apvts.getRawParameterValue("diodeBlend")->load();
    const float tiltDb     = audioProcessor.apvts.getRawParameterValue("tiltDb")->load();
    const float pivotHz    = audioProcessor.apvts.getRawParameterValue("pivotHz")->load();
    const float boxCutDb   = audioProcessor.apvts.getRawParameterValue("boxCutDb")->load();
    const float mix        = audioProcessor.apvts.getRawParameterValue("mix")->load();
    const float outputDb   = audioProcessor.apvts.getRawParameterValue("outputDb")->load();
    const bool  softBoost  = audioProcessor.apvts.getRawParameterValue("softVoiceBoost")->load() > 0.5f;

    for (int i = 0; i < static_cast<int>(kFactoryPresets.size()); ++i)
    {
        const auto& pr = kFactoryPresets[static_cast<size_t>(i)];
        const bool match = std::abs(driveDb - pr.driveDb) < 0.06f
            && std::abs(dcBias - pr.dcBias) < 0.006f
            && std::abs(diodeBlend - pr.diodeBlend) < 0.006f
            && std::abs(tiltDb - pr.tiltDb) < 0.06f
            && std::abs(pivotHz - pr.pivotHz) < 0.9f
            && std::abs(boxCutDb - pr.boxCutDb) < 0.06f
            && std::abs(mix - pr.mix) < 0.006f
            && std::abs(outputDb - pr.outputDb) < 0.06f
            && osIdx == pr.oversamplingIndex
            && eqIdx == pr.eqRoutingIndex
            && softBoost == pr.softVoiceBoost;
        if (match)
            return i + 1;
    }

    return 0; // Shows "Custom Session" via setTextWhenNothingSelected
}

void MilitantDriveAudioProcessorEditor::syncControlStatesFromProcessor()
{
    const int osIdx = juce::jlimit(0, 2, static_cast<int>(std::lround(
        audioProcessor.apvts.getRawParameterValue("oversampling")->load())));
    if (os1xButton.getToggleState() != (osIdx == 0)) os1xButton.setToggleState(osIdx == 0, juce::dontSendNotification);
    if (os2xButton.getToggleState() != (osIdx == 1)) os2xButton.setToggleState(osIdx == 1, juce::dontSendNotification);
    if (os4xButton.getToggleState() != (osIdx == 2)) os4xButton.setToggleState(osIdx == 2, juce::dontSendNotification);

    const bool isPostEq = audioProcessor.apvts.getRawParameterValue("eqRouting")->load() > 0.5f;
    if (eqRouteButton.getToggleState() != isPostEq)
        eqRouteButton.setToggleState(isPostEq, juce::dontSendNotification);
}

void MilitantDriveAudioProcessorEditor::syncPresetDropdownFromProcessor()
{
    const int matchedId = detectMatchingPresetId();
    if (presetBox.getSelectedId() != matchedId)
        presetBox.setSelectedId(matchedId, juce::dontSendNotification);
}

float MilitantDriveAudioProcessorEditor::estimateThdPercent() const noexcept
{
    const bool bypass = audioProcessor.apvts.getRawParameterValue("bypass")->load() > 0.5f;
    const float mix   = audioProcessor.apvts.getRawParameterValue("mix")->load();
    if (bypass || mix <= 0.001f)
        return 0.0f;

    const float driveDb    = audioProcessor.apvts.getRawParameterValue("driveDb")->load();
    const float dcBias     = audioProcessor.apvts.getRawParameterValue("dcBias")->load();
    const float diodeBlend = audioProcessor.apvts.getRawParameterValue("diodeBlend")->load();
    const bool  softBoost  = audioProcessor.apvts.getRawParameterValue("softVoiceBoost")->load() > 0.5f;
    const float driveGain  = std::pow(10.0f, driveDb / 20.0f);

    constexpr int N = 128;
    constexpr float amp = 0.14f;
    double out[N] {};
    double mean = 0.0;

    for (int i = 0; i < N; ++i)
    {
        const float x = amp * std::sin(juce::MathConstants<float>::twoPi * static_cast<float>(i) / static_cast<float>(N));
        const float sb = softBoost ? (1.0f + 2.4f / (1.0f + 10.0f * std::abs(x))) : 1.0f;
        const float u  = x * driveGain * sb;

        constexpr float kGe = 1.45f;
        const float geAsym  = (u >= 0.0f) ? 1.0f : 0.82f;
        const float yGe     = std::tanh(kGe * geAsym * (u + dcBias)) - std::tanh(kGe * dcBias);

        constexpr float kSi = 3.4f;
        const float siBias  = dcBias * 0.6f;
        const float ySi     = std::tanh(kSi * (u + siBias)) - std::tanh(kSi * siBias);

        const float wet = juce::jlimit(-1.0f, 1.0f, yGe * (1.0f - diodeBlend) + ySi * diodeBlend);
        const double mixed = static_cast<double>(x) * (1.0 - mix) + static_cast<double>(wet) * mix;
        out[i] = mixed;
        mean += mixed;
    }
    mean /= static_cast<double>(N);

    double fundMagSq = 0.0;
    double harmMagSq = 0.0;

    for (int h = 1; h <= 12; ++h)
    {
        double re = 0.0;
        double im = 0.0;
        for (int i = 0; i < N; ++i)
        {
            const double angle = juce::MathConstants<double>::twoPi * static_cast<double>(h * i) / static_cast<double>(N);
            const double val   = out[i] - mean;
            re += val * std::cos(angle);
            im -= val * std::sin(angle);
        }
        const double magSq = re * re + im * im;
        if (h == 1) fundMagSq = magSq;
        else harmMagSq += magSq;
    }

    if (fundMagSq < 1e-12)
        return 0.0f;
    return static_cast<float>(juce::jmin(99.9, std::sqrt(harmMagSq / fundMagSq) * 100.0));
}

void MilitantDriveAudioProcessorEditor::timerCallback()
{
    smoothedInPeakDb  = smoothedInPeakDb  * 0.7f + audioProcessor.inputLevelDb.load()  * 0.3f;
    smoothedInRmsDb   = smoothedInRmsDb   * 0.7f + audioProcessor.inputRmsDb.load()    * 0.3f;
    smoothedOutPeakDb = smoothedOutPeakDb * 0.7f + audioProcessor.outputLevelDb.load() * 0.3f;
    smoothedOutRmsDb  = smoothedOutRmsDb  * 0.7f + audioProcessor.outputRmsDb.load()   * 0.3f;
    outClipped        = audioProcessor.outputClipped.load();

    syncControlStatesFromProcessor();
    if (!presetBox.isPopupActive() && presetDirty.exchange(false))
        syncPresetDropdownFromProcessor();

    repaint();
}

void MilitantDriveAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Outer Rack Chassis (#191C18)
    g.fillAll(juce::Colour(0xff191c18));

    auto outerChassis = getLocalBounds().reduced(10).toFloat();
    g.setColour(juce::Colour(0xff191c18));
    g.fillRoundedRectangle(outerChassis, 4.0f);
    g.setColour(juce::Colour(0xff343b32));
    g.drawRoundedRectangle(outerChassis, 4.0f, 1.0f);

    // Top Header Row: MILITANT DRIVE (No Subtitle)
    g.setColour(juce::Colour(0xffecece6));
    g.setFont(consoleLookAndFeel.makeFont(20.0f, true));
    g.drawText("MILITANT DRIVE", 22, 20, 220, 26, juce::Justification::left);

    // Header Bottom Hairline Divider
    g.setColour(juce::Colour(0xff343b32));
    g.drawLine(22.0f, 56.0f, static_cast<float>(getWidth() - 22), 56.0f, 1.0f);

    // Oversampling Segmented Frame Background
    auto osFrame = juce::Rectangle<float>(670.0f, 20.0f, 146.0f, 26.0f);
    g.setColour(juce::Colour(0xff0d0f0d));
    g.fillRoundedRectangle(osFrame, 3.0f);
    g.setColour(juce::Colour(0xff343b32));
    g.drawRoundedRectangle(osFrame, 3.0f, 1.0f);

    // Main Tactical Olive Hardware Faceplate (#232821)
    auto faceplate = juce::Rectangle<float>(20.0f, 68.0f, static_cast<float>(getWidth() - 40), static_cast<float>(getHeight() - 88));
    g.setColour(juce::Colour(0xff232821));
    g.fillRoundedRectangle(faceplate, 3.0f);
    g.setColour(juce::Colour(0xff343b32));
    g.drawRoundedRectangle(faceplate, 3.0f, 1.0f);

    // Column Vertical Dividers
    g.setColour(juce::Colour(0xff343b32));
    g.drawLine(340.0f, 82.0f, 340.0f, static_cast<float>(getHeight() - 34), 1.0f);
    g.drawLine(660.0f, 82.0f, 660.0f, static_cast<float>(getHeight() - 34), 1.0f);
    g.drawLine(758.0f, 82.0f, 758.0f, static_cast<float>(getHeight() - 34), 1.0f);

    // Horizontal Footers inside Modules 1 & 2
    g.setColour(juce::Colour(0xff31382f));
    g.drawLine(36.0f,  268.0f, 324.0f, 268.0f, 1.0f);
    g.drawLine(356.0f, 268.0f, 644.0f, 268.0f, 1.0f);

    // Preamp & Diode Section Header + Dynamic Diode Mode
    g.setColour(juce::Colour(0xffecece6));
    g.setFont(consoleLookAndFeel.makeFont(12.0f, true));
    g.drawText("PREAMP & DIODE STAGE", 36, 82, 200, 16, juce::Justification::left);

    const float blend = audioProcessor.apvts.getRawParameterValue("diodeBlend")->load();
    const juce::String diodeModeText = (blend < 0.35f) ? "Germanium Warmth"
                                     : (blend > 0.65f) ? "Silicon Edge"
                                                       : "Hybrid Ge/Si";
    g.setColour(juce::Colour(0xff9ba196));
    g.setFont(consoleLookAndFeel.makeFont(10.5f, false));
    g.drawText(diodeModeText, 210, 82, 114, 16, juce::Justification::right);

    // Tilt-EQ Section Header + Dynamic EQ Routing Mode
    g.setColour(juce::Colour(0xffecece6));
    g.setFont(consoleLookAndFeel.makeFont(12.0f, true));
    g.drawText("TILT-EQ & BOX CARVE", 356, 82, 200, 16, juce::Justification::left);

    const bool isPostEq = audioProcessor.apvts.getRawParameterValue("eqRouting")->load() > 0.5f;
    g.setColour(juce::Colour(0xff9ba196));
    g.setFont(consoleLookAndFeel.makeFont(10.5f, false));
    g.drawText(isPostEq ? "Post-Diode" : "Pre-Diode", 544, 82, 100, 16, juce::Justification::right);

    // Illuminated Push-Button ON / OFF Micro-Labels
    g.setFont(consoleLookAndFeel.makeFont(10.0f, true));
    g.setColour(juce::Colour(0xff9ba196));
    g.drawText(softBoostButton.getToggleState() ? "ON" : "OFF", 94,  374, 44, 14, juce::Justification::centred);
    g.drawText(eqRouteButton.getToggleState()   ? "ON" : "OFF", 412, 374, 44, 14, juce::Justification::centred);
    g.drawText(bypassButton.getToggleState()    ? "ON" : "OFF", 538, 374, 44, 14, juce::Justification::centred);

    //==========================================================================
    // MODULE 4: MILITARY DUAL 14-SEGMENT LED LADDER VU METER + SCALE + THD
    //==========================================================================
    auto meterHousing = juce::Rectangle<float>(774.0f, 82.0f, 134.0f, 330.0f);
    g.setColour(juce::Colour(0xff141714));
    g.fillRoundedRectangle(meterHousing, 3.0f);
    g.setColour(juce::Colour(0xff343b32));
    g.drawRoundedRectangle(meterHousing, 3.0f, 1.0f);

    // Column Headers: IN / OUT
    g.setColour(juce::Colour(0xff9ba196));
    g.setFont(consoleLookAndFeel.makeFont(10.0f, true));
    g.drawText("IN",  809, 89, 28, 12, juce::Justification::centred);
    g.drawText("OUT", 844, 89, 28, 12, juce::Justification::centred);

    // Recessed Dark Bezel
    auto bezel = juce::Rectangle<float>(783.0f, 105.0f, 116.0f, 240.0f);
    g.setColour(juce::Colour(0xff0b0c0b));
    g.fillRoundedRectangle(bezel, 2.0f);
    g.setColour(juce::Colour(0xff2a2f28));
    g.drawRoundedRectangle(bezel, 2.0f, 1.0f);

    static constexpr float thresholds[14] = { 8, 6, 4, 2, 0, -2, -4, -8, -12, -16, -20, -28, -40, -49 };
    static const char* scaleLabels[14]    = { "+8", "+6", "+4", "+2", "0", "-2", "-4", "-8", "-12", "-16", "-20", "-28", "-40", "-60" };

    const float inVu  = (smoothedInPeakDb  <= -59.5f) ? -90.0f : (smoothedInPeakDb  * 0.35f + smoothedInRmsDb  * 0.65f + 10.0f);
    const float outVu = (smoothedOutPeakDb <= -59.5f) ? -90.0f : (smoothedOutPeakDb * 0.35f + smoothedOutRmsDb * 0.65f + 10.0f);

    const float startY = 112.0f;
    const float segH   = 11.0f;
    const float segGap = 4.0f;

    g.setFont(consoleLookAndFeel.makeFont(9.0f, false));
    for (int i = 0; i < 14; ++i)
    {
        const float sy = startY + static_cast<float>(i) * (segH + segGap);

        // Flanking Left & Right dB Markings
        g.setColour(juce::Colour(0xff9ba196));
        g.drawText(scaleLabels[i], 785, static_cast<int>(sy - 1.0f), 22, 13, juce::Justification::right);
        g.drawText(scaleLabels[i], 874, static_cast<int>(sy - 1.0f), 22, 13, juce::Justification::left);

        const bool inLit  = inVu >= thresholds[i];
        const bool outLit = outVu >= thresholds[i] || (i == 0 && outClipped);

        auto drawSeg = [&g, sy, segH](float sx, bool lit, int rowIdx)
        {
            juce::Colour fillCol, borderCol;
            if (rowIdx < 2)
            {
                fillCol   = lit ? juce::Colour(0xffff3b0a) : juce::Colour(0xff23100b);
                borderCol = lit ? juce::Colour(0xffff6d47) : juce::Colour(0xff120806);
            }
            else if (rowIdx < 4)
            {
                fillCol   = lit ? juce::Colour(0xfff5e027) : juce::Colour(0xff23200c);
                borderCol = lit ? juce::Colour(0xfffef08a) : juce::Colour(0xff121006);
            }
            else
            {
                fillCol   = lit ? juce::Colour(0xff36e216) : juce::Colour(0xff0e1d0c);
                borderCol = lit ? juce::Colour(0xff6ef254) : juce::Colour(0xff081007);
            }
            g.setColour(fillCol);
            g.fillRoundedRectangle(sx, sy, 26.0f, segH, 1.5f);
            g.setColour(borderCol);
            g.drawRoundedRectangle(sx, sy, 26.0f, segH, 1.5f, 0.7f);
        };

        drawSeg(811.0f, inLit,  i);
        drawSeg(845.0f, outLit, i);
    }

    // dB Footer inside Bezel
    g.setColour(juce::Colour(0xff787f74));
    g.setFont(consoleLookAndFeel.makeFont(8.5f, false));
    g.drawText("dB", 785, 326, 22, 12, juce::Justification::right);
    g.drawText("dB", 874, 326, 22, 12, juce::Justification::left);

    // Peak dBFS / CLIP Readouts below Bezel
    const juce::String inStr = (smoothedInPeakDb <= -59.5f) ? "-inf" : juce::String(smoothedInPeakDb, 1);
    const juce::String outStr = outClipped
        ? "CLIP"
        : (smoothedOutPeakDb <= -59.5f ? "-inf" : juce::String(smoothedOutPeakDb, 1));

    g.setFont(consoleLookAndFeel.makeFont(10.0f, false));
    g.setColour(juce::Colour(0xffecece6));
    g.drawText(inStr, 807, 352, 34, 14, juce::Justification::centred);

    g.setColour(outClipped ? juce::Colour(0xffff3b0a) : juce::Colour(0xffecece6));
    g.setFont(consoleLookAndFeel.makeFont(10.0f, outClipped));
    g.drawText(outStr, 842, 352, 34, 14, juce::Justification::centred);

    // Est. THD Footer inside Meter Panel
    g.setColour(juce::Colour(0xff2a2f28));
    g.drawLine(784.0f, 376.0f, 898.0f, 376.0f, 1.0f);

    g.setColour(juce::Colour(0xff9ba196));
    g.setFont(consoleLookAndFeel.makeFont(9.5f, true));
    g.drawText("EST. THD", 786, 384, 58, 16, juce::Justification::left);

    g.setColour(juce::Colour(0xffe59a2b));
    g.setFont(consoleLookAndFeel.makeFont(11.0f, true));
    g.drawText(juce::String(estimateThdPercent(), 1) + "%", 844, 384, 52, 16, juce::Justification::right);
}

void MilitantDriveAudioProcessorEditor::resized()
{
    // Top Utility Controls
    presetHeaderLabel.setBounds(336, 20, 54, 26);
    presetBox.setBounds(396, 20, 262, 26);

    osHeaderLabel.setBounds(673, 22, 26, 22);
    os1xButton.setBounds(701, 22, 36, 22);
    os2xButton.setBounds(739, 22, 36, 22);
    os4xButton.setBounds(777, 22, 36, 22);

    resetButton.setBounds(826, 20, 92, 26);

    // Rotary Knobs Layout
    constexpr int knobW = 90;
    constexpr int knobH = 104;
    const int row1KnobY = 142;
    const int row2KnobY = 304;

    auto placeKnob = [](juce::Slider& s, juce::Label& l, juce::Label& sl, int cx, int y)
    {
        const int left = cx - knobW / 2;
        l.setBounds(left, y - 30, knobW, 14);
        sl.setBounds(left, y - 16, knobW, 13);
        s.setBounds(left, y, knobW, knobH);
    };

    // Module 1 Knobs
    placeKnob(driveSlider,      driveLabel,      driveSubLabel,      86,  row1KnobY);
    placeKnob(dcBiasSlider,     dcBiasLabel,     dcBiasSubLabel,     180, row1KnobY);
    placeKnob(diodeBlendSlider, diodeBlendLabel, diodeBlendSubLabel, 274, row1KnobY);
    placeKnob(mixSlider,        mixLabel,        mixSubLabel,        242, row2KnobY);

    // Module 2 Knobs
    placeKnob(tiltSlider,       tiltLabel,       tiltSubLabel,       406, row1KnobY);
    placeKnob(pivotSlider,      pivotLabel,      pivotSubLabel,      500, row1KnobY);
    placeKnob(boxCutSlider,     boxCutLabel,     boxCutSubLabel,     594, row1KnobY);

    // Illuminated Push-Buttons
    constexpr int btnSize = 44;
    auto placeBtn = [](juce::TextButton& b, juce::Label& l, int cx, int y)
    {
        l.setBounds(cx - 44, y - 18, 88, 15);
        b.setBounds(cx - btnSize / 2, y, btnSize, btnSize);
    };

    placeBtn(softBoostButton, softBoostLabel, 116, 324);
    placeBtn(eqRouteButton,   eqRouteLabel,   434, 324);
    placeBtn(bypassButton,    bypassLabel,    560, 324);

    // Module 3: Console Linear Fader for Output Trim
    outputLabel.setBounds(668, 86, 82, 16);
    outputFader.setBounds(664, 110, 90, 276);
}
