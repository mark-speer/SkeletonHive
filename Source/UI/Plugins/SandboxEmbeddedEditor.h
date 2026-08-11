#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

class SandboxEmbeddedEditor : public juce::Component
{
public:
    SandboxEmbeddedEditor (void* nativeHandle, int editorWidth, int editorHeight);
    ~SandboxEmbeddedEditor() override;

    int getPreferredWidth() const { return preferredWidth; }
    int getPreferredHeight() const { return preferredHeight; }

    void resized() override;
    void parentHierarchyChanged() override;
    void mouseDown (const juce::MouseEvent& event) override;

    /** Detach and forget the worker HWND so later resize/focus cannot touch a dead window. */
    void detach();

private:
    void attachIfNeeded();
    void updateChildBounds();

    void* pluginHwnd = nullptr;
    int preferredWidth = 400;
    int preferredHeight = 300;
    bool attached = false;
};

} // namespace skeletonhive
