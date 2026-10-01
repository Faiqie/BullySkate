using System;
using System.IO;
using System.Text;
using BullySkate;

static class BuildCheck {
    static int Main(string[] args) {
        if(args.Length!=1&&!(args.Length==2&&args[0]=="--running")){Console.WriteLine("Usage: BullyBuildCheck.exe [--running] PATH_TO_BULLY_EXE");return 2;}
        try {
            var result=args.Length==2?GameCompatibility.InspectRunning(args[1]):GameCompatibility.Inspect(args[0]);
            Console.WriteLine(result.Report());
            if(args.Length==1&&GameCompatibility.CanDeferToRuntime(result)) {
                Console.WriteLine("Steam wrapper: install permitted; full in-memory engine verification is required before any native hooks.");return 0;
            }
            return result.Supported?0:2;
        } catch(Exception error){Console.WriteLine("Compatibility check failed: "+error.Message);return 1;}
    }
}
