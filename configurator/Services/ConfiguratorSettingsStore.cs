using System.Text.Json;

namespace Gian07.Configurator.Services;

internal sealed class ConfiguratorSettings
{
    public string GameExecutable { get; set; } = string.Empty;
}

internal static class ConfiguratorSettingsStore
{
    private const string SettingsFileName = "configurator.json";

    public static ConfiguratorSettings Load()
    {
        var path = GetSettingsPath();
        try
        {
            if (File.Exists(path))
            {
                return JsonSerializer.Deserialize<ConfiguratorSettings>(File.ReadAllText(path))
                    ?? new ConfiguratorSettings();
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

        return new ConfiguratorSettings
        {
            GameExecutable = ReadLegacyExecutablePath()
        };
    }

    public static void Save(ConfiguratorSettings settings)
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
        var path = Path.Combine(AppContext.BaseDirectory, "GIAN07_configurator.ini");
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
