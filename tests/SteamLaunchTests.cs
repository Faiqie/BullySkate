using System;
using System.IO;
using BullySkate;

static class SteamLaunchTests {
    static void Check(bool value,string message) {if(!value)throw new Exception(message);}
    static void Reject(string game) {
        Check(!LauncherCore.IsSteamInstallation(game),"Invalid Steam installation accepted");
        try {LauncherCore.SteamGameStartInfo(game);throw new Exception("Invalid Steam launch permitted");}
        catch(IOException) {}
    }
    static int Main(string[] args) {
        string library=Path.Combine(Path.GetFullPath(args[0]),"Steam $(no_command) & spaces");
        string steamapps=Path.Combine(library,"steamapps");
        string game=Path.Combine(steamapps,"common","Bully Scholarship Edition");
        Directory.CreateDirectory(game);
        string manifest=Path.Combine(steamapps,"appmanifest_12200.acf");
        Reject(game);
        File.WriteAllText(manifest,"\"AppState\" { \"appid\" \"12200\" \"installdir\" \"Bully Scholarship Edition\" }");
        Check(LauncherCore.IsSteamInstallation(game),"Steam manifest not recognized");
        Check(LauncherCore.IsSteamInstallation(game+Path.DirectorySeparatorChar),"Trailing separator rejected");
        var start=LauncherCore.SteamGameStartInfo(game);
        Check(start.FileName=="steam://rungameid/12200"&&start.UseShellExecute,"Normal Steam URI not used");
        Check(String.IsNullOrEmpty(start.Arguments),"User path entered URI arguments");
        Check(!File.Exists(Path.Combine(game,"steam_appid.txt")),"Steam app-id override created");
        Reject(Path.Combine(steamapps,"common","Another copy"));
        File.WriteAllText(manifest,"\"appid\" \"99999\" \"installdir\" \"Bully Scholarship Edition\"");
        Reject(game);
        File.WriteAllText(manifest,"\"appid\" \"12200\" \"installdir\" \"Another copy\"");
        Reject(game);
        File.WriteAllText(manifest,"\"appid\" \"12200\"");Reject(game);
        File.WriteAllText(manifest,new String('x',1024*1024+1));Reject(game);
        File.WriteAllText(manifest,"\"appid\" \"12200\" \"installdir\" \"Bully Scholarship Edition\"");
        Reject(Path.Combine(library,"Bully Scholarship Edition"));
        Console.WriteLine("PASS: selected Steam manifest, fixed authenticated URI, wrong app/folder/missing/oversized manifest and metacharacter paths; no Steam client launched.");
        return 0;
    }
}
