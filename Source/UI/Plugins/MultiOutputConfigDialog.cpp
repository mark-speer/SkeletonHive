#include "MultiOutputConfigDialog.h"

namespace skeletonhive
{

namespace
{

class MultiOutputConfigPanel : public juce::Component
{
public:
    MultiOutputConfigPanel (te::AudioTrack& parentTrackIn, te::Plugin& instrumentIn)
        : parentTrack (parentTrackIn), instrument (instrumentIn)
    {
        title.setText ("Output Routing — " + instrument.getName(), juce::dontSendNotification);
        title.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        addAndMakeVisible (title);

        help.setText ("Route additional plugin outputs to child tracks. Bus 0 (main) stays on this track.",
                      juce::dontSendNotification);
        help.setFont (juce::FontOptions (12.0f));
        help.setColour (juce::Label::textColourId, juce::Colours::grey);
        addAndMakeVisible (help);

        buses = MultiOutputRouting::getOutputBuses (instrument);
        existingRoutes = MultiOutputRouting::getRoutes (instrument);

        for (const auto& bus : buses)
        {
            if (bus.busIndex == 0)
                continue;

            auto row = std::make_unique<Row>();
            row->busIndex = bus.busIndex;
            row->label.setText (bus.name + (bus.isActive ? "" : " (inactive)"), juce::dontSendNotification);
            row->label.setMinimumHorizontalScale (0.7f);
            addAndMakeVisible (row->label);

            row->toggle.setToggleState (false, juce::dontSendNotification);
            row->toggle.onClick = [this] { refreshApplyEnabled(); };
            addAndMakeVisible (row->toggle);

            for (const auto& route : existingRoutes)
            {
                if (route.outputBusIndex == bus.busIndex)
                {
                    row->toggle.setToggleState (route.enabled, juce::dontSendNotification);
                    row->existingTrackId = route.childTrackId;
                    break;
                }
            }

            if (auto* child = te::findAudioTrackForID (parentTrack.edit, row->existingTrackId))
                row->trackLabel.setText ("→ " + child->getName(), juce::dontSendNotification);
            else
                row->trackLabel.setText ("→ (new track)", juce::dontSendNotification);

            row->trackLabel.setFont (juce::FontOptions (11.0f));
            row->trackLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
            addAndMakeVisible (row->trackLabel);

            rows.push_back (std::move (row));
        }

        if (rows.empty())
        {
            emptyLabel.setText ("This instrument has no additional output buses.\n"
                                "Enable extra outputs in the plugin editor if supported.",
                                juce::dontSendNotification);
            emptyLabel.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (emptyLabel);
        }

        applyButton.setButtonText ("Apply");
        applyButton.onClick = [this] { applyAndClose(); };
        addAndMakeVisible (applyButton);

        cancelButton.setButtonText ("Cancel");
        cancelButton.onClick = [this]
        {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->exitModalState (0);
        };
        addAndMakeVisible (cancelButton);

        setSize (460, juce::jmax (180, 90 + (int) rows.size() * 36));
        refreshApplyEnabled();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);
        title.setBounds (area.removeFromTop (24));
        help.setBounds (area.removeFromTop (32));
        area.removeFromTop (8);

        if (emptyLabel.isVisible())
        {
            emptyLabel.setBounds (area.removeFromTop (60));
        }
        else
        {
            for (auto& row : rows)
            {
                auto rowArea = area.removeFromTop (28);
                row->toggle.setBounds (rowArea.removeFromLeft (28).reduced (2));
                row->label.setBounds (rowArea.removeFromLeft (180));
                row->trackLabel.setBounds (rowArea);
            }
        }

        area.removeFromTop (8);
        auto buttonRow = area.removeFromBottom (28);
        cancelButton.setBounds (buttonRow.removeFromRight (80));
        buttonRow.removeFromRight (8);
        applyButton.setBounds (buttonRow.removeFromRight (80));
    }

private:
    struct Row
    {
        int busIndex = 0;
        te::EditItemID existingTrackId;
        juce::Label label;
        juce::ToggleButton toggle;
        juce::Label trackLabel;
    };

    te::AudioTrack& parentTrack;
    te::Plugin& instrument;
    juce::Array<OutputBusInfo> buses;
    juce::Array<OutputRoute> existingRoutes;
    std::vector<std::unique_ptr<Row>> rows;

    juce::Label title, help, emptyLabel;
    juce::TextButton applyButton, cancelButton;

    void refreshApplyEnabled()
    {
        applyButton.setEnabled (! rows.empty());
    }

    void applyAndClose()
    {
        juce::Array<OutputRoute> routes;

        for (const auto& existing : existingRoutes)
        {
            if (existing.outputBusIndex == 0)
                routes.add (existing);
        }

        for (const auto& row : rows)
        {
            OutputRoute route;
            route.outputBusIndex = row->busIndex;
            route.enabled = row->toggle.getToggleState();
            route.childTrackId = row->existingTrackId;
            routes.add (route);
        }

        MultiOutputRouting::applyRoutes (parentTrack, instrument, routes);

        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState (1);
    }
};

} // namespace

void MultiOutputConfigDialog::show (te::AudioTrack& instrumentTrack, te::Plugin& instrument,
                                    juce::Component* centreAround)
{
    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = "Configure Outputs";
    opts.content.setOwned (new MultiOutputConfigPanel (instrumentTrack, instrument));
    opts.componentToCentreAround = centreAround;
    opts.useNativeTitleBar = true;
    opts.resizable = false;
    opts.launchAsync();
}

} // namespace skeletonhive
