using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Threading;
using Gian07.Launcher.Services;

namespace Gian07.Launcher;

public sealed partial class PadCaptureDialog : Window
{
    private readonly DispatcherTimer _timer;
    private PadChoice? _previousInput;

    public PadCaptureDialog()
    {
        InitializeComponent();

        _timer = new DispatcherTimer
        {
            Interval = TimeSpan.FromMilliseconds(50)
        };
        _timer.Tick += CaptureTick;
    }

    public PadCaptureDialog(string action)
        : this()
    {
        Title = $"Pad remap: {action}";
        PromptControl.Text = $"Press a controller input for {action}...";
    }

    private TextBlock PromptControl => this.FindControl<TextBlock>("PromptText")!;

    protected override void OnOpened(EventArgs e)
    {
        base.OnOpened(e);
        _previousInput = ControllerService.ReadActiveInput();
        _timer.Start();
    }

    protected override void OnClosed(EventArgs e)
    {
        _timer.Stop();
        base.OnClosed(e);
    }

    private void CaptureTick(object? sender, EventArgs args)
    {
        var input = ControllerService.ReadActiveInput();
        if (input is null)
        {
            _previousInput = null;
            return;
        }

        if (_previousInput is not null && _previousInput.Value == input.Value)
        {
            return;
        }

        Close(input);
    }

    private void CancelClick(object? sender, RoutedEventArgs args)
    {
        Close((PadChoice?)null);
    }
}
