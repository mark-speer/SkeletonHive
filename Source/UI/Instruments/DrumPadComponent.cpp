#include "DrumPadComponent.h"

#include "Engine/ContentDragManager.h"

namespace skeletonhive
{

DrumPadComponent::DrumPadComponent (int padIndexIn, int midiNoteIn)
    : padIndex (padIndexIn), midiNote (midiNoteIn)
{
}

void DrumPadComponent::setSelected (bool shouldBeSelected)
{
    if (selected == shouldBeSelected)
        return;

    selected = shouldBeSelected;
    repaint();
}

void DrumPadComponent::setSampleName (const juce::String& name)
{
    sampleName = name;
    empty = name.isEmpty();
    repaint();
}

void DrumPadComponent::setEmpty (bool shouldBeEmpty)
{
    empty = shouldBeEmpty;

    if (empty)
        sampleName = {};

    repaint();
}

void DrumPadComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const auto base = empty ? juce::Colour (0xff2a2a2a) : juce::Colour (0xff3a4a3a);
    const auto fill = dragHover ? base.brighter (0.15f) : (selected ? base.brighter (0.25f) : base);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 4.0f);

    g.setColour (selected ? juce::Colours::white : juce::Colours::white.withAlpha (0.75f));
    g.drawRoundedRectangle (bounds, 4.0f, selected ? 2.0f : 1.0f);

    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("Pad " + juce::String (padIndex + 1), bounds.removeFromTop (18.0f).reduced (4.0f, 0.0f),
                juce::Justification::centredLeft, true);

    g.setFont (juce::FontOptions (10.0f));
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.drawText ("MIDI " + juce::String (midiNote), bounds.removeFromTop (14.0f).reduced (4.0f, 0.0f),
                juce::Justification::centredLeft, true);

    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.setFont (juce::FontOptions (10.5f));

    if (empty)
        g.drawFittedText ("Empty", bounds.toNearestInt(), juce::Justification::centred, 2);
    else
        g.drawFittedText (sampleName, bounds.toNearestInt(), juce::Justification::centred, 2);
}

void DrumPadComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Browse Sample...");
        menu.addItem (2, "Clear Sample", ! empty);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [this] (int result)
                            {
                                if (result == 1 && onBrowseRequested)
                                    onBrowseRequested (padIndex);
                                else if (result == 2 && onClearRequested)
                                    onClearRequested (padIndex);
                            });
        return;
    }

    if (onSelected)
        onSelected (padIndex);
}

void DrumPadComponent::mouseDoubleClick (const juce::MouseEvent&)
{
    if (onBrowseRequested)
        onBrowseRequested (padIndex);
}

bool DrumPadComponent::isInterestedInDragSource (const SourceDetails& dragSourceDetails)
{
    return dragSourceDetails.description.toString().startsWith (ContentDragTypes::sampleInsert);
}

void DrumPadComponent::itemDragEnter (const SourceDetails&)
{
    dragHover = true;
    repaint();
}

void DrumPadComponent::itemDragExit (const SourceDetails&)
{
    dragHover = false;
    repaint();
}

void DrumPadComponent::itemDropped (const SourceDetails& dragSourceDetails)
{
    dragHover = false;

    const auto payload = ContentDragPayload::parse (dragSourceDetails.description);

    if (payload.isValid() && onSampleDropped)
        onSampleDropped (padIndex, payload.file);

    repaint();
}

} // namespace skeletonhive
