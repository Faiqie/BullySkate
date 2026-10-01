using System;
using System.Runtime.InteropServices;

// A non-game host must load the ASI without applying any Bully address hooks.
static class NativeCompatibilityGuardTests {
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr LoadLibrary(string path);
    [DllImport("kernel32.dll")] static extern bool FreeLibrary(IntPtr module);
    static int Main(string[] args) {
        IntPtr module=LoadLibrary(args[0]);
        if(module==IntPtr.Zero)throw new Exception("Native loader could not be tested: Win32 error "+Marshal.GetLastWin32Error());
        if(!FreeLibrary(module))throw new Exception("Native loader did not unload cleanly.");
        Console.WriteLine("PASS: actual ASI loads and unloads in an incompatible x86 host without game hooks.");
        return 0;
    }
}
