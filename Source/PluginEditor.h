#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include "PluginProcessor.h"
#include "VintageConsoleChannelStripLookAndFeel.h"

class MilitantDriveAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                               private juce::AudioProcessorValueTreeState::Listener,
                                               private juce::Timer
{
public:
    explicit MilitantDriveAudioProcessorEditor(MilitantDriveAudioProcessor&);
    ~MilitantDriveAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void timerCallback() override;
    void configureKnob(juce::Slider& slider, juce::Label& label, juce::Label& sublabel,
                       const juce::String& title, const juce::String& subtitle,
                       double defaultValue, bool bipolar = false);
    void configurePushButton(juce::TextButton& btn, juce::Label& label,
                             const juce::String& title, bool isCrimson = false);
    void applyPresetByIndex(int presetIndex);
    int detectMatchingPresetId() const noexcept;
    void syncControlStatesFromProcessor();
    void syncPresetDropdownFromProcessor();
    float estimateThdPercent() const noexcept;

    MilitantDriveAudioProcessor& audioProcessor;
    VintageConsoleChannelStripLookAndFeel consoleLookAndFeel;

    // Top Header Controls
    juce::Label presetHeaderLabel;
    juce::ComboBox presetBox;
    juce::Label osHeaderLabel;
    juce::TextButton os1xButton, os2xButton, os4xButton;
    juce::TextButton resetButton;

    // Tactical Winged-Bar Rotary Knobs
    juce::Slider driveSlider, dcBiasSlider, diodeBlendSlider;
    juce::Slider tiltSlider, pivotSlider, boxCutSlider;
    juce::Slider mixSlider;

    // Console Linear Fader for Output Trim
    juce::Slider outputFader;

    // Illuminated Push-Buttons
    juce::TextButton bypassButton, softBoostButton, eqRouteButton;

    juce::Label driveLabel, driveSubLabel;
    juce::Label dcBiasLabel, dcBiasSubLabel;
    juce::Label diodeBlendLabel, diodeBlendSubLabel;
    juce::Label tiltLabel, tiltSubLabel;
    juce::Label pivotLabel, pivotSubLabel;
    juce::Label boxCutLabel, boxCutSubLabel;
    juce::Label mixLabel, mixSubLabel;
    juce::Label outputLabel;
    juce::Label bypassLabel, softBoostLabel, eqRouteLabel;

    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttach> driveAttach, dcBiasAttach, diodeBlendAttach;
    std::unique_ptr<SliderAttach> tiltAttach, pivotAttach, boxCutAttach;
    std::unique_ptr<SliderAttach> mixAttach, outputAttach;
    std::unique_ptr<ButtonAttach> bypassAttach, softBoostAttach;

    float smoothedInPeakDb  = -60.0f;
    float smoothedInRmsDb   = -60.0f;
    float smoothedOutPeakDb = -60.0f;
    float smoothedOutRmsDb  = -60.0f;
    bool  outClipped        = false;
    bool  isApplyingPreset  = false;
    std::atomic<bool> presetDirty { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MilitantDriveAudioProcessorEditor)
};
