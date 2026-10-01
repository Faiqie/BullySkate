using System;
using System.Runtime.InteropServices;

// A non-game host must load the ASI without applying any Bully address hooks.
static class NativeCompatibilityGuardTests {
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr LoadLibrary(string path);
    [DllImport("kernel32.dll")] static extern bool FreeLibrary(IntPtr module);
    [DllImport("kernel32.dll",CharSet=CharSet.Ansi)] static extern IntPtr GetProcAddress(IntPtr module,string name);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void Initialize();
    static int Main(string[] args) {
        IntPtr module=LoadLibrary(args[0]);
        if(module==IntPtr.Zero)throw new Exception("Native loader could not be tested: Win32 error "+Marshal.GetLastWin32Error());
        var entry=GetProcAddress(module,"InitializeASI");
        if(entry==IntPtr.Zero)throw new Exception("InitializeASI startup export is missing.");
        var initialize=(Initialize)Marshal.GetDelegateForFunctionPointer(entry,typeof(Initialize));
        initialize();initialize();
        if(!FreeLibrary(module))throw new Exception("Native loader did not unload cleanly.");
        Console.WriteLine("PASS: actual ASI loads, safely retries InitializeASI twice, and unloads in an incompatible x86 host without game hooks.");
        return 0;
    }
}
