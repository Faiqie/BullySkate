using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;

namespace BullySkate {
    public static class AudioStartup {
        [DllImport("ole32.dll")] static extern int CoInitializeEx(IntPtr reserved,uint flags);
        [DllImport("ole32.dll")] static extern void CoUninitialize();
        [DllImport("ole32.dll")] static extern int CoCreateInstance(ref Guid clsid,IntPtr outer,uint context,ref Guid iid,out IntPtr result);
        [DllImport("winmm.dll")] static extern uint waveOutGetNumDevs();
        public static int ProbeXact() {
            int init=CoInitializeEx(IntPtr.Zero,2);
            try {
                var clsid=new Guid("962f5027-99be-4692-a468-85802cf8de61");
                var iid=new Guid("e72c1b9a-d717-41c0-81a6-50eb56e80649");
                IntPtr engine;
                int result=CoCreateInstance(ref clsid,IntPtr.Zero,1,ref iid,out engine);
                if(engine!=IntPtr.Zero)Marshal.Release(engine);
                return result;
            } finally {if(init>=0)CoUninitialize();}
        }
        public static bool MissingRuntime(int result) {
            return result==unchecked((int)0x80040154)||result==unchecked((int)0x800401F8)||result==unchecked((int)0x8007007E);
        }
        public static void Ensure(string package,Action<string> progress) {
            if(waveOutGetNumDevs()==0)throw new IOException("Windows has no available sound output. Connect speakers or headphones, select them in Windows Sound settings, then reopen the launcher.");
            int result=ProbeXact();
            if(MissingRuntime(result)) {
                progress("Bully's legacy audio runtime is missing. Preparing Microsoft's DirectX repair; Windows may request administrator permission.");
                var start=new ProcessStartInfo(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System),"WindowsPowerShell","v1.0","powershell.exe"),
                    "-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "+LauncherCore.Quote(Path.Combine(package,"tools","RepairAudio.ps1"))) {
                    UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true
                };
                start.EnvironmentVariables.Remove("PSModulePath");
                using(var child=new Process{StartInfo=start}) {
                    child.OutputDataReceived+=delegate(object s,DataReceivedEventArgs e){if(e.Data!=null)progress(e.Data);};
                    child.ErrorDataReceived+=delegate(object s,DataReceivedEventArgs e){if(e.Data!=null)progress(e.Data);};
                    child.Start();child.BeginOutputReadLine();child.BeginErrorReadLine();child.WaitForExit();
                    if(child.ExitCode!=0)throw new IOException("The audio repair did not finish. Install Microsoft's DirectX June 2010 End-User Runtimes, then reopen the launcher. https://www.microsoft.com/en-us/download/details.aspx?id=8109");
                }
                result=ProbeXact();
            }
            if(result<0)throw new IOException("Bully's legacy audio engine could not start (0x"+result.ToString("X8")+"). Select an enabled default output in Windows Sound settings. If needed, repair Microsoft's DirectX June 2010 runtime.");
        }
    }
}
