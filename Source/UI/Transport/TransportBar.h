#pragma once





#include "Engine/TransportController.h"





namespace skeletonhive


{





class SessionManager;


class SessionArrangementBridge;





class TransportBar : public juce::Component,


                     private juce::ChangeListener,


                     private juce::Timer


{


public:


    TransportBar (te::Edit& edit, TransportController& transport);


    ~TransportBar() override;





    std::function<void()> onAddAudioTrack;


    std::function<void()> onAddMidiTrack;


    std::function<void()> onAddMidiClip;


    std::function<void()> onToggleMixer;


    std::function<void()> onToggleSidechain;


    std::function<void()> onToggleMidiLearn;





    void setLearnModeActive (bool active);





    void setSessionManager (SessionManager* manager);


    void setSessionArrangementBridge (SessionArrangementBridge* bridge);


    void setSessionViewActive (bool active);





    std::function<void()> onToggleRecordToArrangement;


    std::function<void()> onCaptureSession;


    std::function<void()> onTogglePerformancePanel;





    void setPerformancePanelVisible (bool visible);





private:


    void syncSessionControls();


    void syncSessionCaptureControls();


    void updateSessionControlsVisibility();


    void resized() override;


    void timerCallback() override;


    void changeListenerCallback (juce::ChangeBroadcaster*) override;


    void updateButtonStates();


    void syncTempoControls();





    te::Edit& edit;


    TransportController& transportController;


    SessionManager* sessionManager = nullptr;


    SessionArrangementBridge* sessionArrangementBridge = nullptr;


    bool sessionViewActive = false;





    juce::Label viewLabel { {}, "Arrangement" };


    juce::ComboBox launchQuantizeBox;


    juce::ComboBox sceneLaunchModeBox;


    juce::TextButton recordToArrangementButton { "Rec>" };


    juce::TextButton captureButton { "Capture" };


    juce::TextButton performanceButton { "Perf" };


    juce::Label writePositionLabel { {}, "Write: 0.0.0" };





    juce::DrawableButton returnButton { "Return to start", juce::DrawableButton::ImageFitted },
        playButton { "Play/Pause", juce::DrawableButton::ImageFitted },
        stopButton { "Stop", juce::DrawableButton::ImageFitted },
        recordButton { "Record", juce::DrawableButton::ImageFitted },
        loopButton { "Loop", juce::DrawableButton::ImageFitted },
        punchButton { "Punch", juce::DrawableButton::ImageFitted },
        clickButton { "Click", juce::DrawableButton::ImageFitted },
        takesButton { "Takes", juce::DrawableButton::ImageFitted };


    juce::TextButton addAudioButton { "+ Audio" }, addMidiButton { "+ MIDI" },


        addClipButton { "+ Clip" },


        mixerButton { "Mixer" }, sidechainButton { "Sidechain" },


        learnButton { "Learn" };


    juce::Label positionLabel { {}, "00:00:00.000" };


    juce::Slider tempoSlider;


    juce::Label tempoLabel { {}, "BPM" };


    juce::ComboBox timeSigBox;


    juce::Slider clickVolumeSlider;


    juce::ComboBox countInBox;


};





} // namespace skeletonhive


