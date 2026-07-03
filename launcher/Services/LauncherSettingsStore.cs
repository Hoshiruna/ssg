using System.Text.Json;

namespace Gian07.Launcher.Services;

internal sealed class LauncherSettings
{
    public string GameExecutable { get; set; } = string.Empty;
}

internal static class LauncherSettingsStore
{
    private const string SettingsFileName = "launcher.json";

    public static LauncherSettings Load()
    {
        var path = GetSettingsPath();
        try
        {
            if (File.Exists(path))
            {
                return JsonSerializer.Deserialize<LauncherSettings>(File.ReadAllText(path))
                    ?? new LauncherSettings();
            }
        }
        catch (IOException)
        {
        }
        catch (UnauthorizedAccessException)
        {
        }
        catch (JsonException)
        {
        }

        return new LauncherSettings
        {
            GameExecutable = ReadLegacyExecutablePath()
        };
    }

    public static void Save(LauncherSettings settings)
    {
        var path = GetSettingsPath();
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        var json = JsonSerializer.Serialize(settings, new JsonSerializerOptions
        {
            WriteIndented = true
        });
        File.WriteAllText(path, json);
    }

    private static string GetSettingsPath()
    {
        var root = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
        if (string.IsNullOrWhiteSpace(root))
        {
            root = AppContext.BaseDirectory;
        }

        return Path.Combine(root, "SSG", SettingsFileName);
    }

    private static string ReadLegacyExecutablePath()
    {
        var path = Path.Combine(AppContext.BaseDirectory, "GIAN07_launcher.ini");
        try
        {
            if (!File.Exists(path))
            {
                return string.Empty;
            }

            foreach (var line in File.ReadLines(path))
            {
                const string prefix = "GameExecutable=";
                if (line.StartsWith(prefix, StringComparison.OrdinalIgnoreCase))
                {
                    return line[prefix.Length..].Trim();
                }
            }
        }
        catch (IOException)
        {
        }
        catch (UnauthorizedAccessException)
        {
        }

        return string.Empty;
    }
}
