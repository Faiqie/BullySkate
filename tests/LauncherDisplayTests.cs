using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Windows.Forms;
using BullySkate;

[assembly: System.Runtime.Versioning.TargetFramework(".NETFramework,Version=v4.8",FrameworkDisplayName=".NET Framework 4.8")]

static class LauncherDisplayTests {
    [DllImport("user32.dll")] static extern IntPtr GetThreadDpiAwarenessContext();
    [DllImport("user32.dll")] static extern bool AreDpiAwarenessContextsEqual(IntPtr first,IntPtr second);
    static void Check(bool condition,string message) {if(!condition)throw new Exception(message);}
    static int ChildAwareness(string game,bool windowed,bool fix) {
        var start=LauncherCore.GameStartInfo(game,windowed,fix);
        Check(!start.UseShellExecute,"Game launch uses a shell");
        Check(start.WorkingDirectory==game,"Game working directory changed");
        Check(start.EnvironmentVariables["BULLY_SKATE_WINDOWED"]==(windowed?"1":"0"),"Display mode changed");
        start.RedirectStandardOutput=true;start.CreateNoWindow=true;start.WindowStyle=ProcessWindowStyle.Hidden;
        using(var process=Process.Start(start)) {
            string text=process.StandardOutput.ReadToEnd().Trim();
            Check(process.WaitForExit(10000)&&process.ExitCode==0,"DPI probe failed");
            return Int32.Parse(text);
        }
    }
    [STAThread]
    static int Main(string[] args) {
        Application.EnableVisualStyles();
        Check(AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(),new IntPtr(-4)),"Launcher is not per-monitor DPI aware");
        string previous=Environment.GetEnvironmentVariable("__COMPAT_LAYER");
        try {
            string game=Path.GetFullPath(args[0]);
            string[] cases={null,"DPIUNAWARE","GDIDPISCALING DPIUNAWARE","HIGHDPIAWARE","Win7RTM DPIUNAWARE","  RunAsInvoker\tDPIUNAWARE  "};
            foreach(string layers in cases) {
                Environment.SetEnvironmentVariable("__COMPAT_LAYER",layers);
                var start=LauncherCore.GameStartInfo(game,false,true);
                string merged=start.EnvironmentVariables["__COMPAT_LAYER"];
                Check(merged.EndsWith("HIGHDPIAWARE"),"Scaling override missing");
                Check(!merged.Contains("DPIUNAWARE")&&!merged.Contains("GDIDPISCALING"),"Conflicting scaling retained");
                if(layers!=null&&layers.Contains("Win7RTM"))Check(merged.Contains("Win7RTM"),"Unrelated compatibility setting removed");
                if(layers!=null&&layers.Contains("RunAsInvoker"))Check(merged.Contains("RunAsInvoker"),"Unrelated compatibility setting removed");
                Check(Environment.GetEnvironmentVariable("__COMPAT_LAYER")==layers,"Parent environment changed");
                int awareness=ChildAwareness(game,false,true);
                Check(awareness==1||awareness==2,"Game DPI awareness was "+awareness+" for inherited layers: "+layers);
            }
            Environment.SetEnvironmentVariable("__COMPAT_LAYER",null);
            Check(ChildAwareness(game,false,false)==0,"Troubleshooting opt-out ignored");
            Check(ChildAwareness(game,true,true)>0,"Windowed launch lost the scaling fix");
            Environment.SetEnvironmentVariable("__COMPAT_LAYER","DPIUNAWARE");
            Check(ChildAwareness(game,false,false)==0,"Inherited scaling opt-out changed");
            Check(LauncherCore.GameStartInfo(game,false,false).EnvironmentVariables["__COMPAT_LAYER"]=="DPIUNAWARE","Opt-out compatibility settings changed");
            Console.WriteLine("PASS: per-monitor launcher, 6 real child DPI cases, both display modes, opt-out and preserved parent settings.");
            return 0;
        } finally {Environment.SetEnvironmentVariable("__COMPAT_LAYER",previous);}
    }
}
