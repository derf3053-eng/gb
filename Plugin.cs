
using BepInEx;
using BepInEx.Unity.IL2CPP;

[BepInPlugin(
    "gorebox.visuals.esp",
    "GoreBox ESP",
    "1.0.0"
)]
[BepInProcess("HuesBox.exe")]
public class GoreBoxESPPlugin : BasePlugin
{
    public override void Load()
    {
        Log.LogInfo("GoreBox ESP plugin loaded!");
    }
}
