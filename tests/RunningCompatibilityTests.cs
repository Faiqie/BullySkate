using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using BullySkate;

static class RunningCompatibilityTests {
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool QueryFullProcessImageName(IntPtr handle,uint flags,StringBuilder name,ref int length);
    static void Check(bool value,string message){if(!value)throw new Exception(message);}
    static void Case(string path,bool bad){
        using(var child=new Process {StartInfo=new ProcessStartInfo(path,bad?"--bad":"") {UseShellExecute=false,CreateNoWindow=true,RedirectStandardInput=true,RedirectStandardOutput=true}}) {
            child.Start();
            try {
                string ready=child.StandardOutput.ReadLine();
                Check(ready==(bad?"readybb":"readygg"),"Authored native probe did not become ready in expected mode (including native SHA-256 self-checks): "+ready);
                Check(!child.HasExited,"Authored fixture exited after readiness; code: "+(child.HasExited?child.ExitCode.ToString():"running"));
                bool found=false;
                for(int attempt=0;attempt<20&&!found;attempt++) {
                    foreach(var item in Process.GetProcesses())using(item)if(item.Id==child.Id)found=true;
                    if(!found)Thread.Sleep(100);
                }
                Check(found,"Live authored process not visible in Windows process enumeration");
                var file=GameCompatibility.Inspect(path);
                Check(!file.Supported&&file.CodeMatched==0&&file.RegionsMatched==1,"Fixture does not reproduce file/memory code difference");
                var loaded=GameCompatibility.InspectRunning(path);
                if(loaded.CodeMatched!=(bad?1:2)||loaded.RegionsMatched!=1) {
                    var queried=new StringBuilder(32768);int length=queried.Capacity;
                    bool query=QueryFullProcessImageName(child.Handle,0,queried,ref length);
                    Console.WriteLine("Fixture path: {0}; child image: {1}; query: {2}; error: {3}; process: {4}",path,queried,query,Marshal.GetLastWin32Error(),child.ProcessName);
                    var canonical=typeof(GameCompatibility).GetMethod("CanonicalFile",BindingFlags.NonPublic|BindingFlags.Static);
                    Console.WriteLine("Selected canonical: {0}; image canonical: {1}",canonical.Invoke(null,new object[]{path}),query?canonical.Invoke(null,new object[]{queried.ToString()}):"unavailable");
                }
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
