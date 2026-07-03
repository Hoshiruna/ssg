using System.Buffers.Binary;
using System.Text;

namespace Gian07.Launcher.Services;

internal sealed class GameConfig
{
    public byte GameLevel { get; set; } = 1;
    public byte PlayerStock { get; set; } = 2;
    public byte BombStock { get; set; } = 2;
    public byte DeviceId { get; set; }
    public byte BitDepth { get; set; } = 32;
    public byte FpsDivisor { get; set; } = 1;
    public byte GraphFlags { get; set; }
    public byte SoundFlags { get; set; } = 0x03;
    public byte InputFlags { get; set; } = 0x02;
    public byte DebugFlags { get; set; }
    public byte PadShot { get; set; } = PadMapping.DPadRight;
    public byte PadBomb { get; set; } = PadMapping.DPadUp;
    public byte PadFocus { get; set; } = PadMapping.DPadDown;
    public byte PadCancel { get; set; }
    public byte PadLeft { get; set; } = PadMapping.LStickLeft;
    public byte PadRight { get; set; } = PadMapping.LStickRight;
    public byte PadUp { get; set; } = PadMapping.LStickUp;
    public byte PadDown { get; set; } = PadMapping.LStickDown;
    public byte PadPause { get; set; } = (byte)(PadMapping.ButtonFirst + 2);
    public byte ExtraStageFlags { get; set; }
    public byte StageSelect { get; set; }
    public byte SeVolume { get; set; } = 51;
    public byte BgmVolume { get; set; } = 51;
    public string BgmPack { get; set; } = string.Empty;
    public byte MidiFlags { get; set; } = 1;
    public string GraphicsApi { get; set; } = string.Empty;
    public byte WindowScale4x { get; set; } = 4;
    public short WindowLeft { get; set; } = short.MinValue;
    public short WindowTop { get; set; } = short.MinValue;
    public byte ScreenshotEffort { get; set; }
    public byte VSync { get; set; } = 1;
    public ushort KeyLeft { get; set; } = 80;
    public ushort KeyRight { get; set; } = 79;
    public ushort KeyUp { get; set; } = 82;
    public ushort KeyDown { get; set; } = 81;
    public ushort KeyShot { get; set; } = 29;
    public ushort KeyBomb { get; set; } = 27;
    public ushort KeyFocus { get; set; } = 225;
    public ushort KeyPause { get; set; } = 41;
}

internal static class GameConfigFile
{
    public const string CurrentFileName = "CONFIG.DAT";

    public static string GetPath(string executablePath)
    {
        var directory = Path.GetDirectoryName(Path.GetFullPath(executablePath));
        return Path.Combine(directory ?? AppContext.BaseDirectory, CurrentFileName);
    }

    public static GameConfig Load(string executablePath)
    {
        var directory = Path.GetDirectoryName(Path.GetFullPath(executablePath));
        directory ??= AppContext.BaseDirectory;

        var path = Path.Combine(directory, CurrentFileName);
        if (TryLoad(path, out var config))
        {
            return config;
        }

        return new GameConfig();
    }

    public static void Save(string executablePath, GameConfig config)
    {
        var path = GetPath(executablePath);
        var directory = Path.GetDirectoryName(path);
        if (string.IsNullOrWhiteSpace(directory) || !Directory.Exists(directory))
        {
            throw new DirectoryNotFoundException("The selected game directory does not exist.");
        }

        using var stream = new MemoryStream();
        using var writer = new BigEndianWriter(stream);
        writer.Write(config.GameLevel);
        writer.Write(config.PlayerStock);
        writer.Write(config.BombStock);
        writer.Write(config.DeviceId);
        writer.Write(config.BitDepth);
        writer.Write(config.FpsDivisor);
        writer.Write(config.GraphFlags);
        writer.Write(config.SoundFlags);
        writer.Write(config.InputFlags);
        writer.Write(config.DebugFlags);
        writer.Write(config.PadShot);
        writer.Write(config.PadBomb);
        writer.Write(config.PadFocus);
        writer.Write(config.PadCancel);
        writer.Write(config.ExtraStageFlags);
        writer.Write(config.StageSelect);
        writer.Write(config.SeVolume);
        writer.Write(config.BgmVolume);
        writer.Write(config.BgmPack);
        writer.Write(config.MidiFlags);
        writer.Write(config.GraphicsApi);
        writer.Write(config.WindowScale4x);
        writer.Write(config.WindowLeft);
        writer.Write(config.WindowTop);
        writer.Write(config.ScreenshotEffort);
        writer.Write(config.VSync);
        writer.Write(config.KeyLeft);
        writer.Write(config.KeyRight);
        writer.Write(config.KeyUp);
        writer.Write(config.KeyDown);
        writer.Write(config.KeyShot);
        writer.Write(config.KeyBomb);
        writer.Write(config.KeyFocus);
        writer.Write(config.KeyPause);
        writer.Write(config.PadLeft);
        writer.Write(config.PadRight);
        writer.Write(config.PadUp);
        writer.Write(config.PadDown);
        writer.Write(config.PadPause);

        var bytes = stream.ToArray();
        WriteAtomically(path, bytes);
    }

    private static void WriteAtomically(string path, byte[] bytes)
    {
        var temporaryPath = path + ".tmp";
        File.WriteAllBytes(temporaryPath, bytes);
        File.Move(temporaryPath, path, true);
    }

    private static bool TryLoad(string path, out GameConfig config)
    {
        config = new GameConfig();
        if (!File.Exists(path))
        {
            return false;
        }

        try
        {
            using var stream = File.OpenRead(path);
            using var reader = new BigEndianReader(stream);
            config.GameLevel = reader.ReadByte();
            config.PlayerStock = reader.ReadByte();
            config.BombStock = reader.ReadByte();
            config.DeviceId = reader.ReadByte();
            config.BitDepth = reader.ReadByte();
            config.FpsDivisor = reader.ReadByte();
            config.GraphFlags = reader.ReadByte();
            config.SoundFlags = reader.ReadByte();
            config.InputFlags = reader.ReadByte();
            config.DebugFlags = reader.ReadByte();
            config.PadShot = reader.ReadByte();
            config.PadBomb = reader.ReadByte();
            config.PadFocus = reader.ReadByte();
            config.PadCancel = reader.ReadByte();
            config.ExtraStageFlags = reader.ReadByte();
            config.StageSelect = reader.ReadByte();
            config.SeVolume = reader.ReadByte();
            config.BgmVolume = reader.ReadByte();
            config.BgmPack = reader.ReadString();
            config.MidiFlags = reader.ReadByte();
            config.GraphicsApi = reader.ReadString();
            config.WindowScale4x = reader.ReadByte();
            config.WindowLeft = reader.ReadInt16();
            config.WindowTop = reader.ReadInt16();
            config.ScreenshotEffort = reader.ReadByte();

            config.VSync = reader.ReadByte();
            config.KeyLeft = reader.ReadUInt16();
            config.KeyRight = reader.ReadUInt16();
            config.KeyUp = reader.ReadUInt16();
            config.KeyDown = reader.ReadUInt16();
            config.KeyShot = reader.ReadUInt16();
            config.KeyBomb = reader.ReadUInt16();
            config.KeyFocus = reader.ReadUInt16();
            config.KeyPause = reader.ReadUInt16();
            config.PadLeft = reader.ReadByte();
            config.PadRight = reader.ReadByte();
            config.PadUp = reader.ReadByte();
            config.PadDown = reader.ReadByte();
            config.PadPause = reader.ReadByte();

            return stream.Position == stream.Length;
        }
        catch (Exception) when (
            File.Exists(path))
        {
            config = new GameConfig();
            return false;
        }
    }

    private sealed class BigEndianReader(Stream stream) : IDisposable
    {
        private readonly BinaryReader _reader = new(stream, Encoding.UTF8, leaveOpen: true);

        public byte ReadByte() => _reader.ReadByte();

        public short ReadInt16()
        {
            Span<byte> bytes = stackalloc byte[2];
            _reader.BaseStream.ReadExactly(bytes);
            return BinaryPrimitives.ReadInt16BigEndian(bytes);
        }

        public ushort ReadUInt16()
        {
            Span<byte> bytes = stackalloc byte[2];
            _reader.BaseStream.ReadExactly(bytes);
            return BinaryPrimitives.ReadUInt16BigEndian(bytes);
        }

        public string ReadString()
        {
            Span<byte> sizeBytes = stackalloc byte[4];
            _reader.BaseStream.ReadExactly(sizeBytes);
            var size = BinaryPrimitives.ReadUInt32BigEndian(sizeBytes);
            if (size > 1024 * 1024 || size > _reader.BaseStream.Length - _reader.BaseStream.Position)
            {
                throw new InvalidDataException("Invalid string length in game configuration.");
            }

            var bytes = _reader.ReadBytes((int)size);
            return Encoding.UTF8.GetString(bytes);
        }

        public void Dispose() => _reader.Dispose();
    }

    private sealed class BigEndianWriter(Stream stream) : IDisposable
    {
        private readonly BinaryWriter _writer = new(stream, Encoding.UTF8, leaveOpen: true);

        public void Write(byte value) => _writer.Write(value);

        public void Write(short value)
        {
            Span<byte> bytes = stackalloc byte[2];
            BinaryPrimitives.WriteInt16BigEndian(bytes, value);
            _writer.Write(bytes);
        }

        public void Write(ushort value)
        {
            Span<byte> bytes = stackalloc byte[2];
            BinaryPrimitives.WriteUInt16BigEndian(bytes, value);
            _writer.Write(bytes);
        }

        public void Write(string value)
        {
            var bytes = Encoding.UTF8.GetBytes(value);
            Span<byte> sizeBytes = stackalloc byte[4];
            BinaryPrimitives.WriteUInt32BigEndian(sizeBytes, (uint)bytes.Length);
            _writer.Write(sizeBytes);
            _writer.Write(bytes);
        }

        public void Dispose() => _writer.Dispose();
    }
}
