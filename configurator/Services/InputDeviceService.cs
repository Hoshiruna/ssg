using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Gian07.Configurator.Services;

public sealed record InputDeviceInfo(
    string ProductName,
    string InstanceName,
    string ProductGuid,
    string InstanceGuid,
    string VendorId,
    string ProductId)
{
    internal static InputDeviceInfo Unavailable(string productName, string instanceName)
    {
        return new(
            productName,
            instanceName,
            "(not exposed by this platform)",
            "(not exposed by this platform)",
            "—",
            "—");
    }
}

public sealed class InputDeviceDetails : INotifyPropertyChanged
{
    private InputDeviceInfo _keyboard =
        InputDeviceInfo.Unavailable("Keyboard", "System keyboard");
    private InputDeviceInfo _mouse =
        InputDeviceInfo.Unavailable("Mouse", "System mouse");

    public event PropertyChangedEventHandler? PropertyChanged;

    public InputDeviceInfo Keyboard
    {
        get => _keyboard;
        private set
        {
            if (_keyboard == value)
            {
                return;
            }

            _keyboard = value;
            OnPropertyChanged();
        }
    }

    public InputDeviceInfo Mouse
    {
        get => _mouse;
        private set
        {
            if (_mouse == value)
            {
                return;
            }

            _mouse = value;
            OnPropertyChanged();
        }
    }

    internal void Refresh()
    {
        var devices = InputDeviceService.GetPrimaryDevices();
        Keyboard = devices.Keyboard;
        Mouse = devices.Mouse;
    }

    private void OnPropertyChanged([CallerMemberName] string? propertyName = null)
    {
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
    }
}

internal static class InputDeviceService
{
    private const uint DirectInputVersion = 0x0800;
    private const uint DeviceClassPointer = 2;
    private const uint DeviceClassKeyboard = 3;
    private const uint EnumAttachedOnly = 0x00000001;
    private const uint DeviceTypeHid = 0x00010000;

    private static readonly Guid DirectInput8InterfaceId =
        new("BF798031-483A-4DA2-AA99-5D64ED369700");

    internal static InputDeviceSnapshot GetPrimaryDevices()
    {
        var fallback = new InputDeviceSnapshot(
            InputDeviceInfo.Unavailable("Keyboard", "System keyboard"),
            InputDeviceInfo.Unavailable("Mouse", "System mouse"));

        if (!OperatingSystem.IsWindows())
        {
            return fallback;
        }

        IDirectInput8? directInput = null;
        try
        {
            var interfaceId = DirectInput8InterfaceId;
            var result = DirectInput8Create(
                GetModuleHandle(null),
                DirectInputVersion,
                ref interfaceId,
                out directInput,
                IntPtr.Zero);
            if (result < 0 || directInput is null)
            {
                return fallback;
            }

            var keyboard = GetFirstDevice(directInput, DeviceClassKeyboard, "Keyboard");
            var mouse = GetFirstDevice(directInput, DeviceClassPointer, "Mouse");
            return new InputDeviceSnapshot(
                keyboard ?? fallback.Keyboard,
                mouse ?? fallback.Mouse);
        }
        catch (Exception exception) when (
            exception is DllNotFoundException or
            EntryPointNotFoundException or
            COMException or
            PlatformNotSupportedException)
        {
            return fallback;
        }
        finally
        {
            if (directInput is not null && Marshal.IsComObject(directInput))
            {
                Marshal.FinalReleaseComObject(directInput);
            }
        }
    }

    private static InputDeviceInfo? GetFirstDevice(
        IDirectInput8 directInput,
        uint deviceClass,
        string fallbackName)
    {
        InputDeviceInfo? device = null;
        EnumDevicesCallback callback = (instancePointer, _) =>
        {
            var instance = Marshal.PtrToStructure<DeviceInstance>(instancePointer);
            device = CreateDeviceInfo(instance, fallbackName);
            return 0;
        };
        var callbackPointer = Marshal.GetFunctionPointerForDelegate(callback);

        var result = directInput.EnumDevices(
            deviceClass,
            callbackPointer,
            IntPtr.Zero,
            EnumAttachedOnly);
        GC.KeepAlive(callback);
        return result < 0 ? null : device;
    }

    private static InputDeviceInfo CreateDeviceInfo(
        DeviceInstance instance,
        string fallbackName)
    {
        var productName = string.IsNullOrWhiteSpace(instance.ProductName)
            ? fallbackName
            : instance.ProductName;
        var instanceName = string.IsNullOrWhiteSpace(instance.InstanceName)
            ? productName
            : instance.InstanceName;

        ushort vendorId = 0;
        ushort productId = 0;
        if ((instance.DeviceType & DeviceTypeHid) != 0)
        {
            var productData = BitConverter.ToUInt32(instance.ProductGuid.ToByteArray(), 0);
            vendorId = (ushort)(productData & 0xFFFF);
            productId = (ushort)(productData >> 16);
        }

        return new InputDeviceInfo(
            productName,
            instanceName,
            instance.ProductGuid.ToString("B"),
            instance.InstanceGuid.ToString("B"),
            vendorId.ToString("X4"),
            productId.ToString("X4"));
    }

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    private static extern IntPtr GetModuleHandle(string? moduleName);

    [DllImport("dinput8.dll", ExactSpelling = true)]
    private static extern int DirectInput8Create(
        IntPtr instance,
        uint version,
        ref Guid interfaceId,
        [MarshalAs(UnmanagedType.Interface)] out IDirectInput8 directInput,
        IntPtr outer);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate int EnumDevicesCallback(
        IntPtr deviceInstance,
        IntPtr context);

    [ComImport]
    [Guid("BF798031-483A-4DA2-AA99-5D64ED369700")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    private interface IDirectInput8
    {
        [PreserveSig]
        int CreateDevice(ref Guid deviceGuid, out IntPtr device, IntPtr outer);

        [PreserveSig]
        int EnumDevices(
            uint deviceClass,
            IntPtr callback,
            IntPtr context,
            uint flags);
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct DeviceInstance
    {
        public uint Size;
        public Guid InstanceGuid;
        public Guid ProductGuid;
        public uint DeviceType;

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
        public string? InstanceName;

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
        public string? ProductName;

        public Guid ForceFeedbackDriverGuid;
        public ushort UsagePage;
        public ushort Usage;
    }
}

internal sealed record InputDeviceSnapshot(
    InputDeviceInfo Keyboard,
    InputDeviceInfo Mouse);
