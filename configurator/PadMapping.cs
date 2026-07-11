namespace Gian07.Configurator;

internal static class PadMapping
{
    public const byte None = 0;
    public const byte ButtonFirst = 1;
    public const byte ButtonLast = 32;
    public const byte LStickLeft = 33;
    public const byte LStickRight = 34;
    public const byte LStickUp = 35;
    public const byte LStickDown = 36;
    public const byte DPadLeft = 37;
    public const byte DPadRight = 38;
    public const byte DPadUp = 39;
    public const byte DPadDown = 40;

    public static PadChoice FromValue(byte value)
    {
        return new PadChoice(value, LabelFor(value));
    }

    public static PadChoice Button(int zeroBasedButton)
    {
        return FromValue((byte)(ButtonFirst + zeroBasedButton));
    }

    private static string LabelFor(byte value)
    {
        return value switch
        {
            None => "Not assigned",
            >= ButtonFirst and <= ButtonLast => $"Button{value - ButtonFirst:00}",
            LStickLeft => "LStick Left",
            LStickRight => "LStick Right",
            LStickUp => "LStick Up",
            LStickDown => "LStick Down",
            DPadLeft => "D-Pad Left",
            DPadRight => "D-Pad Right",
            DPadUp => "D-Pad Up",
            DPadDown => "D-Pad Down",
            _ => $"Pad {value}"
        };
    }
}
