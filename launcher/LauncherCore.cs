using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;

namespace BullySkate {
    public sealed class FileEntry {
        public string Relative, Hash, FullPath;
    }
    public sealed class Selection {
        public string Game, Assets, BullyAssets;
    }
    public static class LauncherCore {
        public const string GameHash="BD6E757DBA71F04539F0C3A66DD216F40450456F0012BECD2E14848DD3EC174E";
        public static readonly string[] LoaderNames={"dinput8.dll","dsound.dll","version.dll","winmm.dll","d3d9.dll"};
        public static string Hash(string path) {
            using(var stream=File.OpenRead(path)) using(var hash=SHA256.Create())
                return BitConverter.ToString(hash.ComputeHash(stream)).Replace("-","");
        }
        public static string Below(string root,string relative) {
            root=Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar);
            var full=Path.GetFullPath(Path.Combine(root,relative.Replace('/',Path.DirectorySeparatorChar)));
            if(Path.IsPathRooted(relative)||!full.StartsWith(root+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase))
                throw new InvalidDataException("A package file has an invalid path.");
            return full;
        }
        public static List<FileEntry> Manifest(string path,string root) {
            var result=new List<FileEntry>();
            foreach(var line in File.ReadAllLines(path)) {
                if(String.IsNullOrWhiteSpace(line))continue;
                var match=Regex.Match(line,@"^([a-fA-F0-9]{64})  (.+)$");
                if(!match.Success)throw new InvalidDataException("A build file list is damaged. Download the launcher again.");
                var relative=match.Groups[2].Value;
                result.Add(new FileEntry { Relative=relative,Hash=match.Groups[1].Value,FullPath=Below(root,relative) });
            }
            if(result.Count==0)throw new InvalidDataException("The package file list is empty.");
            return result;
        }
        public static bool Matches(string path,string hash) {
            return File.Exists(path)&&String.Equals(Hash(path),hash,StringComparison.OrdinalIgnoreCase);
        }
        public static string GameFolder(string input) {
            if(String.IsNullOrWhiteSpace(input))throw new ArgumentException("Choose the folder containing Bully.exe.");
            var path=Path.GetFullPath(input.Trim().Trim('"'));
            if(File.Exists(path)&&String.Equals(Path.GetFileName(path),"Bully.exe",StringComparison.OrdinalIgnoreCase))
                path=Path.GetDirectoryName(path);
            return path.TrimEnd(Path.DirectorySeparatorChar);
        }
        public static string AssetsFolder(string package,string input) {
            if(String.IsNullOrWhiteSpace(input))throw new ArgumentException("Extract the Skate 3 data during first-run setup.");
            var path=Path.GetFullPath(input.Trim().Trim('"'));
            if(File.Exists(Path.Combine(path,"assets","private","game.json")))path=Path.Combine(path,"assets");
            return path.TrimEnd(Path.DirectorySeparatorChar);
        }
        public static bool HasLoader(string game) {
            foreach(var name in LoaderNames) {
                var file=Path.Combine(game,name);
                if(!File.Exists(file))continue;
                var description=FileVersionInfo.GetVersionInfo(file).FileDescription??"";
                if(description.IndexOf("ASI Loader",StringComparison.OrdinalIgnoreCase)>=0)return true;
            }
            return false;
        }
        public static void CheckRunning(string game) {
            foreach(var process in Process.GetProcessesByName("Bully"))using(process) {
                try {
                    if(String.Equals(process.MainModule.FileName,Path.Combine(game,"Bully.exe"),StringComparison.OrdinalIgnoreCase))
                        throw new InvalidOperationException("Bully is already running. Close it, then open the launcher again.");
                } catch(System.ComponentModel.Win32Exception) {
                    throw new InvalidOperationException("Close the running Bully window before installing this build.");
                }
            }
        }
        public static List<FileEntry> BullyManifest(string root) {
            var entries=Manifest(Path.Combine(root,"asset-manifest.sha256"),root);
            var required=new HashSet<string>(new[]{"jimmy-bind.json","vehicle-bounds.txt","world.bmgeo","world.bmrails"},StringComparer.Ordinal);
            foreach(var entry in entries)if(!required.Remove(entry.Relative))throw new InvalidDataException("Invalid locally prepared Bully asset list.");
            if(required.Count!=0)throw new InvalidDataException("Incomplete locally prepared Bully assets.");
            return entries;
        }
        public static Selection Check(string package,string gameInput,string assetsInput,string bullyAssets,Action<string> progress) {
            var game=GameFolder(gameInput);var assets=AssetsFolder(package,assetsInput);
            progress("Checking your Bully folder...");
            var exe=Path.Combine(game,"Bully.exe");
            if(!File.Exists(exe))throw new FileNotFoundException("Bully.exe was not found. Browse to your Bully game folder.");
            if(!Matches(exe,GameHash))throw new InvalidDataException("This build needs Bully Scholarship Edition 1.200 SP Build 3. The selected executable is a different version.");
            foreach(var relative in new[]{"Scripts/Scripts.img","Act/Act.img"})
                if(!File.Exists(Below(game,relative)))throw new FileNotFoundException("Select the complete Bully folder. Missing: "+relative);
            CheckRunning(game);
            if(!HasLoader(game)&&File.Exists(Path.Combine(game,"dinput8.dll")))
                throw new InvalidDataException("The selected folder has an unrecognized dinput8.dll. It was preserved. Use your Bully folder with its ASI loader configured.");
            progress("Checking the skating build...");
            foreach(var entry in Manifest(Path.Combine(package,"deployment.sha256"),package))
                if(!Matches(entry.FullPath,entry.Hash))throw new InvalidDataException("A build file is missing or changed: "+entry.Relative+". Download the launcher again.");
            progress("Checking the extracted Skate 3 data...");
            foreach(var entry in Manifest(Path.Combine(package,"runtime","expected-skate-assets.sha256"),assets))
                if(!Matches(entry.FullPath,entry.Hash))throw new InvalidDataException("Extracted skating data is missing or different: "+entry.Relative+". Run the launcher with --setup to extract it again.");
            progress("Checking your locally prepared Bully collision and rig...");
            bullyAssets=Path.GetFullPath(bullyAssets);
            foreach(var entry in BullyManifest(bullyAssets))if(!Matches(entry.FullPath,entry.Hash))throw new InvalidDataException("Local Bully data needs preparation again. Run with --setup: "+entry.Relative);
            return new Selection { Game=game,Assets=assets,BullyAssets=bullyAssets };
        }
        public static bool Installed(string package,string game,string bullyAssets) {
            var collection=Path.Combine(game,"_derpy_script_loader","scripts","BullyMotion");
            var assets=Path.Combine(collection,"skate-assets");
            if(!Matches(Path.Combine(game,"derpy_script_loader.asi"),Hash(Path.Combine(package,"runtime","derpy_script_loader.asi"))))return false;
            if(!HasLoader(game))return false;
            if(!Matches(Path.Combine(collection,"SkatePhysicsWorker.exe"),Hash(Path.Combine(package,"runtime","SkatePhysicsWorker.exe"))))return false;
            if(!Matches(Path.Combine(collection,"vcruntime140.dll"),Hash(Path.Combine(package,"runtime","worker-dependencies","vcruntime140.dll"))))return false;
            if(!File.Exists(Path.Combine(game,"vcruntime140.dll")))return false;
            var sourcePath=Path.Combine(collection,"source-path.txt");
            if(!File.Exists(sourcePath)||!String.Equals(File.ReadAllText(sourcePath).Trim(),assets,StringComparison.OrdinalIgnoreCase))return false;
            var scriptRoot=Path.Combine(package,"scripts","BullyMotion");
            foreach(var file in Directory.GetFiles(scriptRoot,"*",SearchOption.AllDirectories)) {
                var relative=file.Substring(scriptRoot.Length+1);
                if(relative=="config.txt"||relative=="settings.dat")continue;
                if(!Matches(Below(collection,relative),Hash(file)))return false;
            }
            var config=Path.Combine(collection,"config.txt");
            if(!File.Exists(config))return false;
            var text=File.ReadAllText(config);
            if(!Regex.IsMatch(text,@"(?m)^\s*main_script\s+main\.lua\s*$")||!Regex.IsMatch(text,@"(?m)^\s*pre_init_script\s+register\.lua\s*$"))return false;
            var manifest=Path.Combine(package,"runtime","expected-skate-assets.sha256");
            if(!Matches(Path.Combine(assets,"asset-manifest.sha256"),Hash(manifest)))return false;
            foreach(var entry in Manifest(manifest,assets))if(!Matches(entry.FullPath,entry.Hash))return false;
            foreach(var entry in BullyManifest(bullyAssets))if(!Matches(Below(Path.Combine(collection,"assets"),entry.Relative),entry.Hash))return false;
            foreach(var file in Directory.GetFiles(Path.Combine(package,"runtime","Microsoft.VC80.OpenMP")))
                if(!File.Exists(Path.Combine(game,"Microsoft.VC80.OpenMP",Path.GetFileName(file))))return false;
            return File.Exists(Path.Combine(game,"_derpy_script_loader","config.txt"));
        }
        // Windows argv quoting, with no shell evaluation of user-selected paths.
        public static string Quote(string value) {
            var output=new StringBuilder("\"");int slashes=0;
            foreach(var c in value) {
                if(c=='\\'){slashes++;continue;}
                if(c=='"')output.Append('\\',slashes*2+1);
                else output.Append('\\',slashes);
                output.Append(c);slashes=0;
            }
            output.Append('\\',slashes*2);return output.Append('"').ToString();
        }
        public static void Install(string package,Selection selection,Action<string> progress) {
            if(Installed(package,selection.Game,selection.BullyAssets)){progress("Your skating build is already up to date.");return;}
            progress("Backing up and installing the skating build...");
            var arguments="-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "+Quote(Path.Combine(package,"Install.ps1"))+" -GamePath "+Quote(selection.Game)+" -SkateAssets "+Quote(selection.Assets)+" -BullyAssets "+Quote(selection.BullyAssets);
            var start=new ProcessStartInfo(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System),"WindowsPowerShell","v1.0","powershell.exe"),arguments) {
                WorkingDirectory=package,UseShellExecute=false,CreateNoWindow=true,WindowStyle=ProcessWindowStyle.Hidden,
                RedirectStandardOutput=true,RedirectStandardError=true
            };
            // A PowerShell 7 host can pass its module paths to Windows PowerShell.
            // Let the child initialize its own standard module paths instead.
            start.EnvironmentVariables.Remove("PSModulePath");
            var transcript=new StringBuilder();
            using(var process=new Process { StartInfo=start }) {
                DataReceivedEventHandler receive=delegate(object sender,DataReceivedEventArgs e){if(e.Data!=null)lock(transcript)transcript.AppendLine(e.Data);};
                process.OutputDataReceived+=receive;process.ErrorDataReceived+=receive;
                process.Start();process.BeginOutputReadLine();process.BeginErrorReadLine();process.WaitForExit();
                if(process.ExitCode!=0)throw new IOException("Installation could not finish.\r\n\r\n"+transcript.ToString());
            }
            if(!Installed(package,selection.Game,selection.BullyAssets))throw new IOException("The installed files did not pass verification. The game has not been started.");
            progress("Installed files verified.");
        }
        public static ProcessStartInfo GameStartInfo(string game,bool windowed,bool dpiFix) {
            var start=new ProcessStartInfo(Path.Combine(game,"Bully.exe")) { WorkingDirectory=game,UseShellExecute=false,WindowStyle=ProcessWindowStyle.Normal };
            start.EnvironmentVariables["BULLY_SKATE_WINDOWED"]=windowed?"1":"0";
            if(dpiFix) {
                // Apply the Windows Application scaling override before Bully creates
                // any HWNDs. Keep this in the child environment; no global DPI or
                // compatibility settings are changed on the player's PC.
                var layers=new List<string>();
                foreach(var layer in Regex.Split(start.EnvironmentVariables["__COMPAT_LAYER"]??"",@"\s+")) {
                    if(layer.Length==0||String.Equals(layer,"HIGHDPIAWARE",StringComparison.OrdinalIgnoreCase)||
                       String.Equals(layer,"DPIUNAWARE",StringComparison.OrdinalIgnoreCase)||
                       String.Equals(layer,"GDIDPISCALING",StringComparison.OrdinalIgnoreCase))continue;
                    layers.Add(layer);
                }
                layers.Add("HIGHDPIAWARE");
                start.EnvironmentVariables["__COMPAT_LAYER"]=String.Join(" ",layers.ToArray());
            }
            return start;
        }
        public static Process StartGame(string game,bool windowed) {
            return StartGame(game,windowed,true);
        }
        public static Process StartGame(string game,bool windowed,bool dpiFix) {
            CheckRunning(game);
            return Process.Start(GameStartInfo(game,windowed,dpiFix));
        }
    }
}
