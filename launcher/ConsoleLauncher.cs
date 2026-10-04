using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Xml;
using System.Windows.Forms;

[assembly: System.Runtime.Versioning.TargetFramework(".NETFramework,Version=v4.8",FrameworkDisplayName=".NET Framework 4.8")]

namespace BullySkate {
    public static class ConsoleLauncher {
        [DllImport("kernel32.dll")] static extern IntPtr GetConsoleWindow();
        [DllImport("user32.dll")] static extern bool ShowWindow(IntPtr window,int command);
        static readonly StringBuilder Transcript=new StringBuilder();
        static void Say(string message) { lock(Transcript) { Console.WriteLine(message);Transcript.AppendLine(message); } }
        static string Value(Dictionary<string,string> args,string key,string fallback) {
            string value;return args.TryGetValue(key,out value)?value:fallback;
        }
        static Dictionary<string,string> Arguments(string[] args) {
            var result=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
            foreach(var key in new[]{"--setup","--no-launch","--check","--fullscreen","--windowed","--no-dpi-fix","--diagnose-game","--diagnose-running","--help"})result[key]="false";
            for(int i=0;i<args.Length;i++) {
                var key=args[i];
                if(result.ContainsKey(key)) {result[key]="true";continue;}
                if(key!="--game"&&key!="--xex"&&key!="--state-dir"&&key!="--report")throw new ArgumentException("Unknown argument: "+key);
                if(++i==args.Length)throw new ArgumentException("Missing value for "+key);
                result[key]=args[i];
            }
            return result;
        }
        sealed class ConsoleOwner : IWin32Window {
            public IntPtr Handle {get {return GetConsoleWindow();}}
        }
        static string PickGameFile(bool bully,string previous) {
            if(Console.IsInputRedirected)throw new IOException("Automated setup requires --game and --xex. Interactive setup opens file-selection windows.");
            var name=bully?"Bully.exe":"default.xex";
            Say("Select "+name+" in the file-selection window. Cancel leaves your saved setup unchanged.");
            using(var dialog=new OpenFileDialog()) {
                dialog.Title=bully?"Choose Bully Scholarship Edition — Bully.exe":"Choose Skate 3 Xbox 360 — default.xex";
                dialog.Filter=bully?"Bully executable (Bully.exe)|Bully.exe":"Skate 3 executable (default.xex)|default.xex";
                dialog.CheckFileExists=true;dialog.CheckPathExists=true;dialog.Multiselect=false;dialog.RestoreDirectory=true;
                if(!String.IsNullOrEmpty(previous)) {
                    var folder=Directory.Exists(previous)?previous:Path.GetDirectoryName(previous);
                    if(Directory.Exists(folder))dialog.InitialDirectory=folder;
                }
                if(dialog.ShowDialog(new ConsoleOwner())!=DialogResult.OK)throw new OperationCanceledException();
                return dialog.FileName;
            }
        }
        static Dictionary<string,string> Load(string file) {
            var result=new Dictionary<string,string>();
            if(!File.Exists(file))return result;
            var document=new XmlDocument();document.XmlResolver=null;
            try {document.Load(file);}catch(XmlException) {Say("Saved setup could not be read. Enter the game paths again.");return result;}
            foreach(var name in new[]{"Game","Xex","Assets","BullyAssets","AudioAssets"}) {
                var node=document.SelectSingleNode("/BullySkate/"+name);
                if(node!=null)result[name]=node.InnerText;
            }
            return result;
        }
        static void Save(string file,string game,string xex,string assets,string bullyAssets,string audioAssets) {
            var settings=new XmlWriterSettings { Indent=true,Encoding=new UTF8Encoding(false) };
            var temp=file+".new";
            using(var writer=XmlWriter.Create(temp,settings)) {
                writer.WriteStartElement("BullySkate");
                writer.WriteElementString("Game",game);writer.WriteElementString("Xex",xex);writer.WriteElementString("Assets",assets);
                writer.WriteElementString("BullyAssets",bullyAssets);
                writer.WriteElementString("AudioAssets",audioAssets);
                writer.WriteEndElement();
            }
            if(File.Exists(file))File.Replace(temp,file,file+".previous");else File.Move(temp,file);
        }
        static string Unpack(string state) {
            using(var resource=Assembly.GetExecutingAssembly().GetManifestResourceStream("BullySkate.payload")) {
                if(resource==null)throw new IOException("Launcher payload is missing.");
                string id;
                using(var hash=System.Security.Cryptography.SHA256.Create())id=BitConverter.ToString(hash.ComputeHash(resource)).Replace("-","").ToLowerInvariant();
                resource.Position=0;
                var root=Path.Combine(state,"builds",id.Substring(0,20));
                if(Directory.Exists(root)) {
                    try {
                        bool good=true;
                        foreach(var file in LauncherCore.Manifest(Path.Combine(root,"deployment.sha256"),root))good &= LauncherCore.Matches(file.FullPath,file.Hash);
                        if(good)return root;
                    } catch(IOException) {} catch(InvalidDataException) {}
                    // Retain the damaged cache for diagnosis and publish a clean build.
                    Directory.Move(root,root+"-old-"+Guid.NewGuid().ToString("N"));
                }
                Say("Preparing the bundled skating runtime...");
                Directory.CreateDirectory(Path.GetDirectoryName(root));
                var stage=root+"-new-"+Guid.NewGuid().ToString("N");
                Directory.CreateDirectory(stage);
                using(var archive=new ZipArchive(resource,ZipArchiveMode.Read,true)) {
                    foreach(var entry in archive.Entries) {
                        if(entry.FullName.EndsWith("/"))continue;
                        var target=LauncherCore.Below(stage,entry.FullName);
                        Directory.CreateDirectory(Path.GetDirectoryName(target));
                        using(var source=entry.Open())using(var output=File.Create(target))source.CopyTo(output);
                    }
                }
                foreach(var file in LauncherCore.Manifest(Path.Combine(stage,"deployment.sha256"),stage))
                    if(!LauncherCore.Matches(file.FullPath,file.Hash))throw new InvalidDataException("Bundled file failed verification: "+file.Relative);
                Directory.Move(stage,root);
                return root;
            }
        }
        static string Extract(string package,string state,string xex) {
            var destination=Path.Combine(state,"assets",Guid.NewGuid().ToString("N"));
            var arguments="--xex "+LauncherCore.Quote(xex)+" --out "+LauncherCore.Quote(destination)+
                          " --manifest "+LauncherCore.Quote(Path.Combine(package,"runtime","expected-skate-assets.sha256"));
            RunExtractor(package,arguments);
            return destination;
        }
        static string ExtractBully(string package,string state,string game) {
            var destination=Path.Combine(state,"bully-assets",Guid.NewGuid().ToString("N"));
            RunExtractor(package,"--bully "+LauncherCore.Quote(game)+" --out "+LauncherCore.Quote(destination));
            return destination;
        }
        static string ExtractAudio(string package,string state,string xex,string assets) {
            var destination=Path.Combine(state,"audio-assets",Guid.NewGuid().ToString("N"));
            RunExtractor(package,"--audio "+LauncherCore.Quote(xex)+" --skate-assets "+LauncherCore.Quote(assets)+" --out "+LauncherCore.Quote(destination));
            return destination;
        }
        static void RunExtractor(string package,string arguments) {
            var start=new ProcessStartInfo(Path.Combine(package,"runtime","SkateAssetExtractor.exe"),arguments) {
                WorkingDirectory=package,UseShellExecute=false,CreateNoWindow=true,
                RedirectStandardOutput=true,RedirectStandardError=true
            };
            using(var child=new Process { StartInfo=start }) {
                DataReceivedEventHandler receive=delegate(object sender,DataReceivedEventArgs e) {if(e.Data!=null)Say(e.Data);};
                child.OutputDataReceived+=receive;child.ErrorDataReceived+=receive;
                child.Start();child.BeginOutputReadLine();child.BeginErrorReadLine();child.WaitForExit();
                if(child.ExitCode!=0)throw new IOException("Game data preparation did not finish. Your saved setup was preserved.");
            }
        }
        [STAThread]
        public static int Main(string[] raw) {
            Application.EnableVisualStyles();
            Dictionary<string,string> args=null;
            try {
                args=Arguments(raw);
                if(args["--help"]=="true") {
                    Say("Bully Skate Launcher: first run asks for Bully and Skate 3 default.xex; later runs launch Bully.");
                    Say("Keep the complete extracted Xbox 360 Skate 3 folder around its XEX.");
                    Say("F8 > Video: fullscreen, borderless, VSync, filtering and frame limit.");
                    Say("--windowed: borderless for this launch. --fullscreen: fullscreen for this launch.");
                    Say("--setup: change game paths / re-extract. --fullscreen: keep the native display path.");
                    Say("--check: verify saved setup. --no-launch: prepare or verify without starting Bully.");
                    Say("Display scaling is corrected automatically. --no-dpi-fix: use original Windows scaling for troubleshooting.");
                    Say("--diagnose-game: pick Bully.exe and save a compatibility report; no installation or Skate 3 files required.");
                    Say("--diagnose-running: leave Bully open at its menu, then pick its EXE to check the loaded engine read-only.");
                    Say("Optional: --game PATH --xex PATH --state-dir PATH --report FILE");return 0;
                }
                var state=Path.GetFullPath(Value(args,"--state-dir",Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"BullySkate")));
                Directory.CreateDirectory(state);
                if(args["--diagnose-game"]=="true"||args["--diagnose-running"]=="true") {
                    bool running=args["--diagnose-running"]=="true";
                    if(running)Say("Leave your normal Bully copy running at its main menu. This check only reads its memory; it does not install, alter or close the game.");
                    var game=LauncherCore.GameFolder(Value(args,"--game",null)??PickGameFile(true,null));
                    var info=running?GameCompatibility.InspectRunning(Path.Combine(game,"Bully.exe")):GameCompatibility.Inspect(Path.Combine(game,"Bully.exe"));
                    var report=Path.Combine(state,running?"running-compatibility-report.txt":"compatibility-report.txt");
                    File.WriteAllText(report,info.Report(),new UTF8Encoding(false));
                    Say(info.Report());Say("Compatibility report saved to "+report);
                    if(!running&&GameCompatibility.CanDeferToRuntime(info))Say("Steam-wrapped file: installation is allowed, with full engine verification deferred to ASI startup.");
                    if(!Console.IsInputRedirected){Console.Write("Press Enter to close.");Console.ReadLine();}
                    return info.Supported?0:2;
                }
                using(var mutex=new Mutex(false,"Local\\BullySkateConsoleLauncher")) {
                    bool acquired;
                    try {acquired=mutex.WaitOne(0);}catch(AbandonedMutexException){acquired=true;}
                    if(!acquired)throw new IOException("The skating launcher is already preparing or starting Bully.");
                    try {
                        Console.Title="Bully Skate Launcher";
                        Console.InputEncoding=new UTF8Encoding(false);
                        Console.OutputEncoding=new UTF8Encoding(false);
                        Say("Bully Skate Launcher");
                        var settings=Path.Combine(state,"console-setup.xml");
                        var saved=Load(settings);
                        var package=Unpack(state);
                        bool setup=args["--setup"]=="true"||args.ContainsKey("--game")||args.ContainsKey("--xex")||!saved.ContainsKey("Assets")||!saved.ContainsKey("Game")||!saved.ContainsKey("Xex");
                        if(args["--check"]=="true"&&setup)throw new IOException("No saved setup is available. Run the launcher normally first.");
                        string game,xex,assets,bullyAssets,audioAssets;bool saveSetup=setup;
                        if(setup) {
                            Say("First-run setup extracts your local Skate 3 data and installs the skating mod.");
                            Say("Choose each game in the file-selection windows. No path copying is needed.");
                            game=Value(args,"--game",null)??PickGameFile(true,Value(saved,"Game",null));
                            game=LauncherCore.GameFolder(game);
                            var executable=Path.Combine(game,"Bully.exe");
                            GameCompatibility.Require(executable,Say);
                            if(GameCompatibility.Inspect(executable).SteamWrapped&&!LauncherCore.IsSteamInstallation(game))throw new IOException("Choose Bully.exe from Steam's installed Bully folder. Steam > Manage > Browse local files shows the correct folder.");
                            LauncherCore.CheckRunning(game);
                            xex=Value(args,"--xex",null)??PickGameFile(false,Value(saved,"Xex",null));
                            xex=Path.GetFullPath(xex.Trim().Trim('"'));
                            if(Directory.Exists(xex))xex=Path.Combine(xex,"default.xex");
                            assets=Extract(package,state,xex);
                            bullyAssets=ExtractBully(package,state,game);
                        } else {
                            game=saved["Game"];xex=saved["Xex"];assets=saved["Assets"];Say("Using your saved setup.");
                            bullyAssets=Value(saved,"BullyAssets",null);
                            var receipt=bullyAssets==null?null:Path.Combine(bullyAssets,"receipt.json");
                            bool currentGeometry=receipt!=null&&File.Exists(receipt)&&Regex.IsMatch(File.ReadAllText(receipt),@"""schema""\s*:\s*7\b");
                            if(!currentGeometry) {
                                if(args["--check"]=="true")throw new IOException("Run the launcher normally once to prepare your local Bully collision and rig data.");
                                LauncherCore.CheckRunning(game);bullyAssets=ExtractBully(package,state,game);saveSetup=true;
                            }
                        }
                        audioAssets=setup?null:Value(saved,"AudioAssets",null);
                        if(audioAssets==null||!File.Exists(Path.Combine(audioAssets,"private","audio","audio_manifest.json"))) {
                            if(args["--check"]=="true")throw new IOException("Open the launcher normally once to prepare Skate 3 sounds from your files.");
                            LauncherCore.CheckRunning(game);audioAssets=ExtractAudio(package,state,xex,assets);saveSetup=true;
                        }
                        LauncherCore.CheckAudio(audioAssets);
                        var selection=LauncherCore.Check(package,game,assets,bullyAssets,Say);
                        selection.AudioAssets=audioAssets;
                        if(args["--check"]=="true") {
                            if(!LauncherCore.Installed(package,selection.Game,selection.BullyAssets,audioAssets))throw new IOException("The mod installation needs repair. Run the launcher normally.");
                            Say("Saved setup and installed files verified.");return 0;
                        }
                        LauncherCore.Install(package,selection,Say);
                        if(saveSetup)Save(settings,selection.Game,xex,selection.Assets,selection.BullyAssets,audioAssets);
                        if(args["--no-launch"]=="true") {Say("Ready. Open this launcher again to play.");return 0;}
                        bool windowed=args["--windowed"]=="true"&&args["--fullscreen"]!="true";
                        bool dpiFix=args["--no-dpi-fix"]!="true";
                        bool steam=LauncherCore.IsSteamInstallation(selection.Game);
                        if(!steam&&GameCompatibility.Inspect(Path.Combine(selection.Game,"Bully.exe")).SteamWrapped)throw new IOException("This Steam-wrapped copy must be selected from Steam's installed Bully folder. In Steam use Manage > Browse local files, then select that Bully.exe in the launcher.");
                        AudioStartup.Ensure(package,Say);
                        Say(steam?"Starting your selected Bully installation through Steam...":"Starting Bully...");
                        Say(dpiFix?"Windows display scaling correction enabled.":"Using original Windows display scaling.");
                        Say("Right stick click + D-pad Down: Skate / Bully. Right stick click + D-pad Left: Skate menu.");
                        if(steam) {
                            LauncherCore.StartSteamGame(selection.Game,dpiFix,args["--fullscreen"]=="true"?"0":windowed?"1":null);
                            var steamConsole=GetConsoleWindow();if(steamConsole!=IntPtr.Zero)ShowWindow(steamConsole,0);
                            return 0;
                        }
                        var console=GetConsoleWindow();
                        if(console!=IntPtr.Zero)ShowWindow(console,0);
                        try {var start=LauncherCore.GameStartInfo(selection.Game,windowed,dpiFix);if(args["--fullscreen"]=="true")start.EnvironmentVariables["BULLY_SKATE_DISPLAY"]="0";using(var gameProcess=Process.Start(start))Say("Bully started. Process "+gameProcess.Id+".");}
                        catch {if(console!=IntPtr.Zero)ShowWindow(console,5);throw;}
                        return 0;
                    } finally {mutex.ReleaseMutex();}
                }
            } catch(OperationCanceledException) {
                Say("Setup cancelled. Your saved setup was preserved; Bully was not started.");return 2;
            } catch(Exception error) {
                Say("Error: "+error.Message);
                if(!Console.IsInputRedirected) {Console.Write("Press Enter to close.");Console.ReadLine();}
                return 1;
            } finally {
                if(args!=null&&args.ContainsKey("--report")) {
                    var path=Path.GetFullPath(args["--report"]);
                    Directory.CreateDirectory(Path.GetDirectoryName(path));
                    File.WriteAllText(path,Transcript.ToString(),new UTF8Encoding(false));
                }
            }
        }
    }
}
