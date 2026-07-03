using System.Runtime.InteropServices;
using Gian07.Launcher;

namespace Gian07.Launcher.Services;

internal static class ControllerService
{
    public static IReadOnlyList<string> Enumerate()
    {
        var controllers = new List<string>
        {
            "Automatic (all connected controllers)"
        };

        if (!OperatingSystem.IsWindows())
        {
            return controllers;
        }

        var count = JoyGetNumDevs();
        for (uint id = 0; id < count; id++)
        {
            if (JoyGetPos(id, out _) != 0)
            {
                continue;
            }

            if (JoyGetDevCaps((nuint)id, out var caps, (uint)Marshal.SizeOf<JoyCaps>()) == 0)
            {
                controllers.Add($"{caps.Name} (Joystick {id + 1})");
            }
        }

        return controllers;
    }

    public static PadChoice? ReadActiveInput()
    {
        if (!OperatingSystem.IsWindows())
        {
            return null;
        }

        var count = JoyGetNumDevs();
        for (uint id = 0; id < count; id++)
        {
            var info = new JoyInfoEx
            {
                Size = (uint)Marshal.SizeOf<JoyInfoEx>(),
                Flags = JoyReturnAll
            };
            if (JoyGetPosEx(id, ref info) != 0)
            {
                continue;
            }

            for (var button = 0; button < 32; button++)
            {
                if ((info.Buttons & (1u << button)) != 0)
                {
                    return PadMapping.Button(button);
                }
            }

            var pov = ReadPov(info.PointOfView);
            if (pov is not null)
            {
                return pov;
            }

            var axis = ReadAxis(info.X, info.Y);
            if (axis is not null)
            {
                return axis;
            }
        }

        return null;
    }

    private static PadChoice? ReadPov(uint pov)
    {
        if (pov == JoyPovCentered)
        {
            return null;
        }

        var degrees = pov / 100;
        return degrees switch
        {
            >= 315 or < 45 => PadMapping.FromValue(PadMapping.DPadUp),
            >= 45 and < 135 => PadMapping.FromValue(PadMapping.DPadRight),
            >= 135 and < 225 => PadMapping.FromValue(PadMapping.DPadDown),
            _ => PadMapping.FromValue(PadMapping.DPadLeft)
        };
    }

    private static PadChoice? ReadAxis(uint x, uint y)
    {
        const uint low = 0x4000;
        const uint high = 0xC000;

        if (x <= low)
        {
            return PadMapping.FromValue(PadMapping.LStickLeft);
        }

        if (x >= high)
        {
            return PadMapping.FromValue(PadMapping.LStickRight);
        }

        if (y <= low)
        {
            return PadMapping.FromValue(PadMapping.LStickUp);
        }

        if (y >= high)
        {
            return PadMapping.FromValue(PadMapping.LStickDown);
        }

        return null;
    }

    [DllImport("winmm.dll", EntryPoint = "joyGetNumDevs")]
    private static extern uint JoyGetNumDevs();

    [DllImport("winmm.dll", EntryPoint = "joyGetPos")]
    private static extern uint JoyGetPos(uint joystickId, out JoyInfo info);

    [DllImport("winmm.dll", EntryPoint = "joyGetPosEx")]
    private static extern uint JoyGetPosEx(uint joystickId, ref JoyInfoEx info);

    [DllImport("winmm.dll", EntryPoint = "joyGetDevCapsW", CharSet = CharSet.Unicode)]
    private static extern uint JoyGetDevCaps(nuint joystickId, out JoyCaps caps, uint size);

    [StructLayout(LayoutKind.Sequential)]
    private struct JoyInfo
    {
        public uint X;
        public uint Y;
        public uint Z;
        public uint Buttons;
    }

    private const uint JoyReturnAll = 0xFF;
    private const uint JoyPovCentered = 0xFFFF;

    [StructLayout(LayoutKind.Sequential)]
    private struct JoyInfoEx
    {
        public uint Size;
        public uint Flags;
        public uint X;
        public uint Y;
        public uint Z;
        public uint R;
        public uint U;
        public uint V;
        public uint Buttons;
        public uint ButtonNumber;
        public uint PointOfView;
        public uint Reserved1;
        public uint Reserved2;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct JoyCaps
    {
        public ushort ManufacturerId;
        public ushort ProductId;

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)]
        public string? Name;

        public uint XMin;
        public uint XMax;
        public uint YMin;
        public uint YMax;
        public uint ZMin;
        public uint ZMax;
        public uint ButtonCount;
        public uint PeriodMin;
        public uint PeriodMax;
        public uint RMin;
        public uint RMax;
        public uint UMin;
        public uint UMax;
        public uint VMin;
        public uint VMax;
        public uint Capabilities;
        public uint MaxAxes;
        public uint AxisCount;
        public uint MaxButtons;

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)]
        public string? RegistryKey;

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
        public string? OemVxd;
    }
}
