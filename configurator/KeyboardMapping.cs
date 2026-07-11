using Avalonia.Input;

namespace Gian07.Configurator;

internal static class KeyboardMapping
{
    private static readonly IReadOnlyDictionary<PhysicalKey, KeyChoice> ByPhysicalKey =
        CreateMappings();

    private static readonly IReadOnlyDictionary<ushort, KeyChoice> ByScancode =
        ByPhysicalKey.Values
            .GroupBy(key => key.Scancode)
            .ToDictionary(group => group.Key, group => group.First());

    public static KeyChoice? FromPhysicalKey(PhysicalKey physicalKey)
    {
        return ByPhysicalKey.GetValueOrDefault(physicalKey);
    }

    public static KeyChoice FromScancode(ushort scancode)
    {
        if (ByScancode.TryGetValue(scancode, out var key))
        {
            return key;
        }

        return scancode == 0
            ? new KeyChoice(0, "Not assigned")
            : new KeyChoice(scancode, $"Key {scancode}");
    }

    private static IReadOnlyDictionary<PhysicalKey, KeyChoice> CreateMappings()
    {
        var mappings = new Dictionary<PhysicalKey, KeyChoice>();

        void Add(PhysicalKey physicalKey, ushort scancode, string label)
        {
            mappings.Add(physicalKey, new KeyChoice(scancode, label));
        }

        for (var index = 0; index < 26; index++)
        {
            var physicalKey = (PhysicalKey)((int)PhysicalKey.A + index);
            Add(physicalKey, (ushort)(4 + index), ((char)('A' + index)).ToString());
        }

        var numberKeys = new[]
        {
            PhysicalKey.Digit1, PhysicalKey.Digit2, PhysicalKey.Digit3,
            PhysicalKey.Digit4, PhysicalKey.Digit5, PhysicalKey.Digit6,
            PhysicalKey.Digit7, PhysicalKey.Digit8, PhysicalKey.Digit9,
            PhysicalKey.Digit0
        };
        for (var index = 0; index < numberKeys.Length; index++)
        {
            Add(numberKeys[index], (ushort)(30 + index), ((index + 1) % 10).ToString());
        }

        Add(PhysicalKey.Enter, 40, "Enter");
        Add(PhysicalKey.Escape, 41, "Escape");
        Add(PhysicalKey.Backspace, 42, "Backspace");
        Add(PhysicalKey.Tab, 43, "Tab");
        Add(PhysicalKey.Space, 44, "Space");
        Add(PhysicalKey.Minus, 45, "Minus");
        Add(PhysicalKey.Equal, 46, "Equals");
        Add(PhysicalKey.BracketLeft, 47, "Left Bracket");
        Add(PhysicalKey.BracketRight, 48, "Right Bracket");
        Add(PhysicalKey.Backslash, 49, "Backslash");
        Add(PhysicalKey.Semicolon, 51, "Semicolon");
        Add(PhysicalKey.Quote, 52, "Apostrophe");
        Add(PhysicalKey.Backquote, 53, "Grave");
        Add(PhysicalKey.Comma, 54, "Comma");
        Add(PhysicalKey.Period, 55, "Period");
        Add(PhysicalKey.Slash, 56, "Slash");
        Add(PhysicalKey.CapsLock, 57, "Caps Lock");

        for (var index = 0; index < 12; index++)
        {
            var physicalKey = (PhysicalKey)((int)PhysicalKey.F1 + index);
            Add(physicalKey, (ushort)(58 + index), $"F{index + 1}");
        }

        Add(PhysicalKey.PrintScreen, 70, "Print Screen");
        Add(PhysicalKey.ScrollLock, 71, "Scroll Lock");
        Add(PhysicalKey.Pause, 72, "Pause");
        Add(PhysicalKey.Insert, 73, "Insert");
        Add(PhysicalKey.Home, 74, "Home");
        Add(PhysicalKey.PageUp, 75, "Page Up");
        Add(PhysicalKey.Delete, 76, "Delete");
        Add(PhysicalKey.End, 77, "End");
        Add(PhysicalKey.PageDown, 78, "Page Down");
        Add(PhysicalKey.ArrowRight, 79, "Right Arrow");
        Add(PhysicalKey.ArrowLeft, 80, "Left Arrow");
        Add(PhysicalKey.ArrowDown, 81, "Down Arrow");
        Add(PhysicalKey.ArrowUp, 82, "Up Arrow");

        Add(PhysicalKey.NumLock, 83, "Num Lock");
        Add(PhysicalKey.NumPadDivide, 84, "Numpad /");
        Add(PhysicalKey.NumPadMultiply, 85, "Numpad *");
        Add(PhysicalKey.NumPadSubtract, 86, "Numpad −");
        Add(PhysicalKey.NumPadAdd, 87, "Numpad +");
        Add(PhysicalKey.NumPadEnter, 88, "Numpad Enter");
        for (var index = 0; index < 9; index++)
        {
            var physicalKey = (PhysicalKey)((int)PhysicalKey.NumPad1 + index);
            Add(physicalKey, (ushort)(89 + index), $"Numpad {index + 1}");
        }
        Add(PhysicalKey.NumPad0, 98, "Numpad 0");
        Add(PhysicalKey.NumPadDecimal, 99, "Numpad .");

        Add(PhysicalKey.IntlBackslash, 100, "Non-US Backslash");
        Add(PhysicalKey.ContextMenu, 101, "Menu");
        Add(PhysicalKey.Power, 102, "Power");
        Add(PhysicalKey.NumPadEqual, 103, "Numpad =");

        for (var index = 0; index < 12; index++)
        {
            var physicalKey = (PhysicalKey)((int)PhysicalKey.F13 + index);
            Add(physicalKey, (ushort)(104 + index), $"F{index + 13}");
        }

        Add(PhysicalKey.Help, 117, "Help");
        Add(PhysicalKey.Select, 119, "Select");
        Add(PhysicalKey.Again, 121, "Again");
        Add(PhysicalKey.Undo, 122, "Undo");
        Add(PhysicalKey.Cut, 123, "Cut");
        Add(PhysicalKey.Copy, 124, "Copy");
        Add(PhysicalKey.Paste, 125, "Paste");
        Add(PhysicalKey.Find, 126, "Find");
        Add(PhysicalKey.AudioVolumeMute, 127, "Volume Mute");
        Add(PhysicalKey.AudioVolumeUp, 128, "Volume Up");
        Add(PhysicalKey.AudioVolumeDown, 129, "Volume Down");
        Add(PhysicalKey.NumPadComma, 133, "Numpad Comma");

        Add(PhysicalKey.IntlRo, 135, "International Ro");
        Add(PhysicalKey.KanaMode, 136, "Kana");
        Add(PhysicalKey.IntlYen, 137, "Yen");
        Add(PhysicalKey.Convert, 138, "Convert");
        Add(PhysicalKey.NonConvert, 139, "Non-Convert");
        Add(PhysicalKey.Lang1, 144, "Language 1");
        Add(PhysicalKey.Lang2, 145, "Language 2");
        Add(PhysicalKey.Lang3, 146, "Language 3");
        Add(PhysicalKey.Lang4, 147, "Language 4");
        Add(PhysicalKey.Lang5, 148, "Language 5");

        Add(PhysicalKey.NumPadParenLeft, 182, "Numpad (");
        Add(PhysicalKey.NumPadParenRight, 183, "Numpad )");
        Add(PhysicalKey.NumPadClear, 216, "Numpad Clear");

        Add(PhysicalKey.ControlLeft, 224, "Left Ctrl");
        Add(PhysicalKey.ShiftLeft, 225, "Left Shift");
        Add(PhysicalKey.AltLeft, 226, "Left Alt");
        Add(PhysicalKey.MetaLeft, 227, "Left Meta");
        Add(PhysicalKey.ControlRight, 228, "Right Ctrl");
        Add(PhysicalKey.ShiftRight, 229, "Right Shift");
        Add(PhysicalKey.AltRight, 230, "Right Alt");
        Add(PhysicalKey.MetaRight, 231, "Right Meta");

        Add(PhysicalKey.Sleep, 258, "Sleep");
        Add(PhysicalKey.WakeUp, 259, "Wake");
        Add(PhysicalKey.MediaTrackNext, 267, "Next Track");
        Add(PhysicalKey.MediaTrackPrevious, 268, "Previous Track");
        Add(PhysicalKey.MediaStop, 269, "Media Stop");
        Add(PhysicalKey.Eject, 270, "Eject");
        Add(PhysicalKey.MediaPlayPause, 271, "Play / Pause");
        Add(PhysicalKey.MediaSelect, 272, "Media Select");
        Add(PhysicalKey.Open, 274, "Open");
        Add(PhysicalKey.Props, 279, "Properties");
        Add(PhysicalKey.BrowserSearch, 280, "Browser Search");
        Add(PhysicalKey.BrowserHome, 281, "Browser Home");
        Add(PhysicalKey.BrowserBack, 282, "Browser Back");
        Add(PhysicalKey.BrowserForward, 283, "Browser Forward");
        Add(PhysicalKey.BrowserStop, 284, "Browser Stop");
        Add(PhysicalKey.BrowserRefresh, 285, "Browser Refresh");
        Add(PhysicalKey.BrowserFavorites, 286, "Browser Favorites");

        return mappings;
    }
}
