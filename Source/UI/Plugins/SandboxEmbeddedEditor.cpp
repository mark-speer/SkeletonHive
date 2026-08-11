#include "SandboxEmbeddedEditor.h"

#if JUCE_WINDOWS
 #include <windows.h>
#endif

namespace skeletonhive
{

SandboxEmbeddedEditor::SandboxEmbeddedEditor (void* nativeHandle, int editorWidth, int editorHeight)
    : pluginHwnd (nativeHandle),
      preferredWidth (juce::jmax (editorWidth, 100)),
      preferredHeight (juce::jmax (editorHeight, 100))
{
    setOpaque (true);
}

SandboxEmbeddedEditor::~SandboxEmbeddedEditor()
{
    detach();
}

void SandboxEmbeddedEditor::detach()
{
#if JUCE_WINDOWS
    if (pluginHwnd == nullptr)
        return;

    const auto hwnd = (HWND) pluginHwnd;

    if (IsWindow (hwnd))
        SetParent (hwnd, nullptr);

    pluginHwnd = nullptr;
    attached = false;
#else
    pluginHwnd = nullptr;
    attached = false;
#endif
}

void SandboxEmbeddedEditor::parentHierarchyChanged()
{
    attachIfNeeded();
}

void SandboxEmbeddedEditor::attachIfNeeded()
{
#if JUCE_WINDOWS
    if (attached || pluginHwnd == nullptr)
        return;

    if (auto* peer = getPeer())
    {
        const auto hostHwnd = (HWND) peer->getNativeHandle();
        const auto pluginHWND = (HWND) pluginHwnd;

        if (hostHwnd == nullptr || ! IsWindow (pluginHWND))
            return;

        SetParent (pluginHWND, hostHwnd);

        auto style = (DWORD) GetWindowLong (pluginHWND, GWL_STYLE);
        style &= ~ (DWORD) (WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
        style |= WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
        SetWindowLong (pluginHWND, GWL_STYLE, (LONG) style);

        auto exStyle = (DWORD) GetWindowLong (pluginHWND, GWL_EXSTYLE);
        exStyle &= ~ (DWORD) (WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
        SetWindowLong (pluginHWND, GWL_EXSTYLE, (LONG) exStyle);

        attached = true;
        updateChildBounds();
        ShowWindow (pluginHWND, SW_SHOW);
    }
#else
    juce::ignoreUnused (attached);
#endif
}

void SandboxEmbeddedEditor::resized()
{
    attachIfNeeded();
    updateChildBounds();
}

void SandboxEmbeddedEditor::updateChildBounds()
{
#if JUCE_WINDOWS
    if (! attached || pluginHwnd == nullptr)
        return;

    const auto pluginHWND = (HWND) pluginHwnd;

    if (! IsWindow (pluginHWND))
        return;

    const auto bounds = getLocalBounds();
    SetWindowPos (pluginHWND,
                  nullptr,
                  bounds.getX(),
                  bounds.getY(),
                  bounds.getWidth(),
                  bounds.getHeight(),
                  SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
#endif
}

void SandboxEmbeddedEditor::mouseDown (const juce::MouseEvent& event)
{
    juce::Component::mouseDown (event);

#if JUCE_WINDOWS
    if (pluginHwnd != nullptr)
    {
        const auto pluginHWND = (HWND) pluginHwnd;

        if (IsWindow (pluginHWND))
            SetFocus (pluginHWND);
    }
#endif
}

} // namespace skeletonhive
