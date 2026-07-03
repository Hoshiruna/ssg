using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.CompilerServices;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Media;
using Avalonia.Platform.Storage;
using Gian07.Launcher.Services;

namespace Gian07.Launcher;

public sealed partial class MainWindow : Window
{
    private const byte GraphFullscreen = 0x08;
    private const byte GraphExclusive = 0x10;
    private static readonly IReadOnlyList<ScaleOption> ScaleOptions =
    [
        new(0, "Fit to display"),
        new(4, "640 × 480 (1×)"),
        new(5, "800 × 600 (1.25×)"),
        new(6, "960 × 720 (1.5×)"),
        new(7, "1120 × 840 (1.75×)"),
        new(8, "1280 × 960 (2×)"),
        new(9, "1440 × 1080 (2.25×)"),
        new(10, "1600 × 1200 (2.5×)"),
        new(12, "1920 × 1440 (3×)")
    ];

    private readonly LauncherSettings _settings;
    private GameConfig _config = new();
    private bool _controllerDetected;

    public ObservableCollection<KeyBindingRow> KeyBindingRows { get; } = [];

    public MainWindow()
    {
        InitializeComponent();
        DataContext = this;

        WindowSizeControl.ItemsSource = ScaleOptions;
        _settings = LauncherSettingsStore.Load();
        ExecutablePathControl.Text = FindInitialExecutable(_settings.GameExecutable);
        RefreshControllers();
        LoadSelectedConfiguration();
    }

    private RadioButton WindowedRadioControl => this.FindControl<RadioButton>("WindowedRadio")!;
    private RadioButton FullscreenRadioControl => this.FindControl<RadioButton>("FullscreenRadio")!;
    private ComboBox WindowSizeControl => this.FindControl<ComboBox>("WindowSizeCombo")!;
    private RadioButton FpsNoneControl => this.FindControl<RadioButton>("FpsNoneRadio")!;
    private RadioButton FpsHalfControl => this.FindControl<RadioButton>("FpsHalfRadio")!;
    private RadioButton FpsThirdControl => this.FindControl<RadioButton>("FpsThirdRadio")!;
    private RadioButton FpsAutomaticControl => this.FindControl<RadioButton>("FpsAutomaticRadio")!;
    private RadioButton Color32Control => this.FindControl<RadioButton>("Color32Radio")!;
    private RadioButton Color16Control => this.FindControl<RadioButton>("Color16Radio")!;
    private CheckBox VsyncControl => this.FindControl<CheckBox>("VsyncCheck")!;
    private CheckBox BorderlessControl => this.FindControl<CheckBox>("BorderlessCheck")!;
    private ComboBox ControllerControl => this.FindControl<ComboBox>("ControllerCombo")!;
    private TextBox ExecutablePathControl => this.FindControl<TextBox>("ExecutablePathBox")!;
    private TextBlock StatusControl => this.FindControl<TextBlock>("StatusText")!;

    private void LoadSelectedConfiguration()
    {
        var executable = ResolveExecutablePath();
        _config = GameConfigFile.Load(executable);

        WindowedRadioControl.IsChecked = (_config.GraphFlags & GraphFullscreen) == 0;
        FullscreenRadioControl.IsChecked = !WindowedRadioControl.IsChecked;
        WindowSizeControl.SelectedItem =
            ScaleOptions.FirstOrDefault(option => option.Value == _config.WindowScale4x)
            ?? ScaleOptions[0];
        FpsNoneControl.IsChecked = _config.FpsDivisor == 1;
        FpsHalfControl.IsChecked = _config.FpsDivisor == 2;
        FpsThirdControl.IsChecked = _config.FpsDivisor == 3;
        FpsAutomaticControl.IsChecked = _config.FpsDivisor == 0;
        if (
            FpsNoneControl.IsChecked != true &&
            FpsHalfControl.IsChecked != true &&
            FpsThirdControl.IsChecked != true &&
            FpsAutomaticControl.IsChecked != true)
        {
            FpsNoneControl.IsChecked = true;
        }
        Color16Control.IsChecked = _config.BitDepth == 16;
        Color32Control.IsChecked = !Color16Control.IsChecked;
        VsyncControl.IsChecked = _config.VSync != 0;
        BorderlessControl.IsChecked = (_config.GraphFlags & GraphExclusive) == 0;
        KeyBindingRows.Clear();
        KeyBindingRows.Add(CreateRow("Left", BindingId.Left, _config.KeyLeft, _config.PadLeft));
        KeyBindingRows.Add(CreateRow("Right", BindingId.Right, _config.KeyRight, _config.PadRight));
        KeyBindingRows.Add(CreateRow("Up", BindingId.Up, _config.KeyUp, _config.PadUp));
        KeyBindingRows.Add(CreateRow("Down", BindingId.Down, _config.KeyDown, _config.PadDown));
        KeyBindingRows.Add(CreateRow("Shot", BindingId.Shot, _config.KeyShot, _config.PadShot));
        KeyBindingRows.Add(CreateRow("Bomb", BindingId.Bomb, _config.KeyBomb, _config.PadBomb));
        KeyBindingRows.Add(CreateRow("Focus", BindingId.Focus, _config.KeyFocus, _config.PadFocus));
        KeyBindingRows.Add(CreateRow("Pause", BindingId.Pause, _config.KeyPause, _config.PadPause));

    }

    private KeyBindingRow CreateRow(
        string action,
        BindingId id,
        ushort scancode,
        byte padValue = 0)
    {
        return new KeyBindingRow(
            action,
            id,
            KeyboardMapping.FromScancode(scancode),
            PadMapping.FromValue(padValue),
            _controllerDetected);
    }

    private void CaptureControls()
    {
        _config.GraphFlags &= unchecked((byte)~(GraphFullscreen | GraphExclusive));
        if (FullscreenRadioControl.IsChecked == true)
        {
            _config.GraphFlags |= GraphFullscreen;
        }

        if (BorderlessControl.IsChecked != true)
        {
            _config.GraphFlags |= GraphExclusive;
        }

        _config.WindowScale4x = (WindowSizeControl.SelectedItem as ScaleOption)?.Value ?? 0;
        _config.FpsDivisor =
            FpsAutomaticControl.IsChecked == true ? (byte)0 :
            FpsHalfControl.IsChecked == true ? (byte)2 :
            FpsThirdControl.IsChecked == true ? (byte)3 :
            (byte)1;
        _config.BitDepth = Color16Control.IsChecked == true ? (byte)16 : (byte)32;
        _config.VSync = VsyncControl.IsChecked == true ? (byte)1 : (byte)0;

        foreach (var row in KeyBindingRows)
        {
            SetKey(row.Id, row.SelectedKey.Scancode);
            SetPad(row.Id, row.SelectedPad.Value);
        }
    }

    private void SetKey(BindingId id, ushort value)
    {
        switch (id)
        {
            case BindingId.Left: _config.KeyLeft = value; break;
            case BindingId.Right: _config.KeyRight = value; break;
            case BindingId.Up: _config.KeyUp = value; break;
            case BindingId.Down: _config.KeyDown = value; break;
            case BindingId.Shot: _config.KeyShot = value; break;
            case BindingId.Bomb: _config.KeyBomb = value; break;
            case BindingId.Focus: _config.KeyFocus = value; break;
            case BindingId.Pause: _config.KeyPause = value; break;
        }
    }

    private void SetPad(BindingId id, byte value)
    {
        switch (id)
        {
            case BindingId.Left: _config.PadLeft = value; break;
            case BindingId.Right: _config.PadRight = value; break;
            case BindingId.Up: _config.PadUp = value; break;
            case BindingId.Down: _config.PadDown = value; break;
            case BindingId.Shot: _config.PadShot = value; break;
            case BindingId.Bomb: _config.PadBomb = value; break;
            case BindingId.Focus: _config.PadFocus = value; break;
            case BindingId.Pause: _config.PadPause = value; break;
        }
    }

    private bool SaveConfiguration()
    {
        try
        {
            var executable = ResolveExecutablePath();
            CaptureControls();
            GameConfigFile.Save(executable, _config);
            _settings.GameExecutable = ExecutablePathControl.Text?.Trim() ?? string.Empty;
            LauncherSettingsStore.Save(_settings);
            SetStatus("Settings saved.", isError: false);
            return true;
        }
        catch (Exception exception) when (
            exception is IOException or UnauthorizedAccessException or ArgumentException)
        {
            SetStatus(exception.Message, isError: true);
            return false;
        }
    }

    private async void BrowseClick(object? sender, RoutedEventArgs args)
    {
        var executablePatterns = OperatingSystem.IsWindows()
            ? new[] { "*.exe" }
            : new[] { "*" };
        var files = await StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title = "Select the game executable",
            AllowMultiple = false,
            FileTypeFilter =
            [
                new FilePickerFileType("Game executable")
                {
                    Patterns = executablePatterns
                }
            ]
        });

        var path = files.FirstOrDefault()?.TryGetLocalPath();
        if (string.IsNullOrWhiteSpace(path))
        {
            return;
        }

        ExecutablePathControl.Text = path;
        LoadSelectedConfiguration();
    }

    private void RefreshControllersClick(object? sender, RoutedEventArgs args)
    {
        RefreshControllers();
        SetStatus("Controller list refreshed.", isError: false);
    }

    private void RefreshControllers()
    {
        var controllers = ControllerService.Enumerate();
        _controllerDetected = controllers.Count > 1;
        ControllerControl.ItemsSource = controllers;
        ControllerControl.SelectedIndex = 0;

        foreach (var row in KeyBindingRows)
        {
            row.IsPadEnabled = _controllerDetected;
        }
    }

    private async void RemapKeyClick(object? sender, RoutedEventArgs args)
    {
        if (sender is not Button { DataContext: KeyBindingRow row })
        {
            return;
        }

        var dialog = new KeyCaptureDialog(row.Action);
        var selectedKey = await dialog.ShowDialog<KeyChoice?>(this);
        if (selectedKey is not null)
        {
            row.SelectedKey = selectedKey;
        }
    }

    private async void RemapPadClick(object? sender, RoutedEventArgs args)
    {
        if (sender is not Button { DataContext: KeyBindingRow row })
        {
            return;
        }

        var dialog = new PadCaptureDialog(row.Action);
        var selectedPad = await dialog.ShowDialog<PadChoice?>(this);
        if (selectedPad is not null)
        {
            row.SelectedPad = selectedPad;
        }
    }

    private void CancelClick(object? sender, RoutedEventArgs args)
    {
        Close();
    }

    private void OkClick(object? sender, RoutedEventArgs args)
    {
        if (SaveConfiguration())
        {
            Close();
        }
    }

    private void StartGameClick(object? sender, RoutedEventArgs args)
    {
        if (!SaveConfiguration())
        {
            return;
        }

        var executable = ResolveExecutablePath();
        if (!File.Exists(executable))
        {
            SetStatus("The selected game executable does not exist.", isError: true);
            return;
        }

        try
        {
            Process.Start(new ProcessStartInfo
            {
                FileName = executable,
                WorkingDirectory = Path.GetDirectoryName(executable) ?? AppContext.BaseDirectory,
                UseShellExecute = false
            });
            Close();
        }
        catch (Exception exception) when (
            exception is InvalidOperationException or System.ComponentModel.Win32Exception)
        {
            SetStatus($"Could not start the game: {exception.Message}", isError: true);
        }
    }

    private string ResolveExecutablePath()
    {
        var value = ExecutablePathControl.Text?.Trim();
        if (string.IsNullOrWhiteSpace(value))
        {
            value = DefaultExecutableName();
        }

        return Path.IsPathRooted(value)
            ? Path.GetFullPath(value)
            : Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, value));
    }

    private static string FindInitialExecutable(string savedPath)
    {
        if (!string.IsNullOrWhiteSpace(savedPath))
        {
            var resolved = Path.IsPathRooted(savedPath)
                ? savedPath
                : Path.Combine(AppContext.BaseDirectory, savedPath);
            if (File.Exists(resolved))
            {
                return savedPath;
            }
        }

        var fileName = DefaultExecutableName();
        var directory = new DirectoryInfo(AppContext.BaseDirectory);
        for (var depth = 0; directory is not null && depth < 5; depth++, directory = directory.Parent)
        {
            var candidate = Path.Combine(directory.FullName, fileName);
            if (File.Exists(candidate))
            {
                return candidate;
            }
        }

        return fileName;
    }

    private static string DefaultExecutableName()
    {
        return OperatingSystem.IsWindows() ? "GIAN07.exe" : "GIAN07";
    }

    private void SetStatus(string message, bool isError)
    {
        StatusControl.Text = message;
        StatusControl.Foreground = isError ? Brushes.IndianRed : Brushes.SeaGreen;
    }
}

public sealed record ScaleOption(byte Value, string Label)
{
    public override string ToString() => Label;
}

public sealed record KeyChoice(ushort Scancode, string Label)
{
    public override string ToString() => Label;
}

public sealed record PadChoice(byte Value, string Label)
{
    public override string ToString() => Label;
}

public sealed class KeyBindingRow : INotifyPropertyChanged
{
    private KeyChoice _selectedKey;
    private PadChoice _selectedPad;
    private bool _isPadEnabled;

    public KeyBindingRow(
        string action,
        BindingId id,
        KeyChoice selectedKey,
        PadChoice selectedPad,
        bool isPadEnabled)
    {
        Action = action;
        Id = id;
        _selectedKey = selectedKey;
        _selectedPad = selectedPad;
        _isPadEnabled = isPadEnabled;
    }

    public event PropertyChangedEventHandler? PropertyChanged;

    public string Action { get; }
    public BindingId Id { get; }

    public KeyChoice SelectedKey
    {
        get => _selectedKey;
        set
        {
            if (_selectedKey == value)
            {
                return;
            }

            _selectedKey = value;
            OnPropertyChanged();
        }
    }

    public PadChoice SelectedPad
    {
        get => _selectedPad;
        set
        {
            if (_selectedPad == value)
            {
                return;
            }

            _selectedPad = value;
            OnPropertyChanged();
        }
    }

    public bool IsPadEnabled
    {
        get => _isPadEnabled;
        set
        {
            if (_isPadEnabled == value)
            {
                return;
            }

            _isPadEnabled = value;
            OnPropertyChanged();
        }
    }

    private void OnPropertyChanged([CallerMemberName] string? propertyName = null)
    {
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
    }
}

public enum BindingId
{
    Left,
    Right,
    Up,
    Down,
    Shot,
    Bomb,
    Focus,
    Pause
}
