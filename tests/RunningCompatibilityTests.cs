using System;
using System.Diagnostics;
using System.IO;
using BullySkate;

static class RunningCompatibilityTests {
    static void Check(bool value,string message){if(!value)throw new Exception(message);}
    static void Case(string path,bool bad){
        using(var child=new Process {StartInfo=new ProcessStartInfo(path,bad?"--bad":"") {UseShellExecute=false,CreateNoWindow=true,RedirectStandardInput=true,RedirectStandardOutput=true}}) {
            child.Start();
            try {
                string ready=child.StandardOutput.ReadLine();
                Check(ready==(bad?"readybb":"readygg"),"Authored native probe did not become ready in expected mode (including native SHA-256 self-checks): "+ready);
                Check(!child.HasExited,"Authored fixture exited after readiness; code: "+(child.HasExited?child.ExitCode.ToString():"running"));
                var file=GameCompatibility.Inspect(path);
                Check(!file.Supported&&file.CodeMatched==0&&file.RegionsMatched==1,"Fixture does not reproduce file/memory code difference");
                var loaded=GameCompatibility.InspectRunning(path);
                Check(loaded.CodeMatched==(bad?1:2)&&loaded.RegionsMatched==1,"Loaded engine counters differ: "+loaded.Report());
                Check(loaded.Supported!=bad,"Loaded compatibility result differs: "+loaded.Report());
                Check(loaded.Report().IndexOf(path,StringComparison.OrdinalIgnoreCase)<0,"Diagnostic report contains a personal path");
                Check(!child.HasExited,"Read-only inspection stopped the process");
            } finally {
                if(!child.HasExited){child.StandardInput.Write("x");child.StandardInput.Flush();if(!child.WaitForExit(5000))child.Kill();}
            }
        }
    }
    static int Main(string[] args){
        string path=Path.GetFullPath(args[0]);
        Case(path,false);Case(path,true);
        Check(!GameCompatibility.InspectRunning(path).Supported,"A stopped process was accepted");
        Console.WriteLine("PASS: read-only loaded-engine verification accepts authored file/memory differences, rejects changed runtime code and missing processes; native SHA-256 vectors and invalid pointers pass.");
        return 0;
    }
}
