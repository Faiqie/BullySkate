using System;
using System.Runtime.InteropServices;

// Runs without a DPI manifest, like the legacy game. No visible window is opened.
static class GameDpiProbe {
    [DllImport("shcore.dll")] static extern int GetProcessDpiAwareness(IntPtr process,out int awareness);
    static int Main() {
        int awareness;
        int result=GetProcessDpiAwareness(IntPtr.Zero,out awareness);
        if(result!=0)throw new Exception("GetProcessDpiAwareness failed: "+result);
        Console.WriteLine(awareness);
        return 0;
    }
}
