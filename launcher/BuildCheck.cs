using System;
using System.IO;
using System.Text;
using BullySkate;

static class BuildCheck {
    static int Main(string[] args) {
        if(args.Length!=1){Console.WriteLine("Usage: BullyBuildCheck.exe PATH_TO_BULLY_EXE");return 2;}
        try {
            var result=GameCompatibility.Inspect(args[0]);
            Console.WriteLine(result.Report());
            return result.Supported?0:2;
        } catch(Exception error){Console.WriteLine("Compatibility check failed: "+error.Message);return 1;}
    }
}
