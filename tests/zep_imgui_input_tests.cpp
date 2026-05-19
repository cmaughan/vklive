#include "zep/imgui/editor_imgui.h"
#include "zep/imgui/usb_hid_keys.h"

#include <iostream>

int main()
{
    if (Zep::ZepImGuiInput::LetterKey('f' - 'a') != ImGuiKey_F)
    {
        std::cerr << "Ctrl+F must poll ImGuiKey_F\n";
        return 1;
    }

    if (Zep::ZepImGuiInput::LetterKey('b' - 'a') != ImGuiKey_B)
    {
        std::cerr << "Ctrl+B must poll ImGuiKey_B\n";
        return 1;
    }

    if (Zep::ZepImGuiInput::LetterZepKey('f' - 'a') != 'f')
    {
        std::cerr << "Ctrl+F must be delivered to Zep as lowercase f plus Ctrl\n";
        return 1;
    }

    if (Zep::ZepImGuiInput::LetterZepKey('b' - 'a') != 'b')
    {
        std::cerr << "Ctrl+B must be delivered to Zep as lowercase b plus Ctrl\n";
        return 1;
    }

    if (Zep::ZepImGuiInput::DigitKey(1) != ImGuiKey_1 || Zep::ZepImGuiInput::DigitKey(0) != ImGuiKey_0)
    {
        std::cerr << "Ctrl+digit mode shortcuts must poll ImGui named digit keys\n";
        return 1;
    }

    if (int(Zep::ZepImGuiInput::LetterKey('f' - 'a')) == KEY_F)
    {
        std::cerr << "Ctrl+F regressed to USB HID key codes instead of ImGui named keys\n";
        return 1;
    }

    return 0;
}
