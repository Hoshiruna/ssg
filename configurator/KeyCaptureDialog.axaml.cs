using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;

namespace Gian07.Configurator;

public sealed partial class KeyCaptureDialog : Window
{
    public KeyCaptureDialog()
    {
        InitializeComponent();

        AddHandler(
            InputElement.KeyDownEvent,
            CaptureKeyDown,
            RoutingStrategies.Tunnel,
            handledEventsToo: true);
    }

    public KeyCaptureDialog(string action)
        : this()
    {
        Title = $"Key remap: {action}";
    }

    private TextBlock PromptControl => this.FindControl<TextBlock>("PromptText")!;

    private void CaptureKeyDown(object? sender, KeyEventArgs args)
    {
        var key = KeyboardMapping.FromPhysicalKey(args.PhysicalKey);
        if (key is null)
        {
            PromptControl.Text = "This key cannot be assigned. Press another key...";
            return;
        }

        args.Handled = true;
        Close(key);
    }

    private void CancelClick(object? sender, RoutedEventArgs args)
    {
        Close((KeyChoice?)null);
    }
}
