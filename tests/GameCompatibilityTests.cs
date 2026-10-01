using System;
using System.IO;
using System.Text;
using BullySkate;

static class GameCompatibilityTests {
    static void Check(bool condition,string message) {if(!condition)throw new Exception(message);}
    static void U16(byte[] data,int offset,ushort value) {Array.Copy(BitConverter.GetBytes(value),0,data,offset,2);}
    static void U32(byte[] data,int offset,uint value) {Array.Copy(BitConverter.GetBytes(value),0,data,offset,4);}
    static void Section(byte[] data,int offset,string name,uint address,uint raw,uint flags) {
        Array.Copy(Encoding.ASCII.GetBytes(name),0,data,offset,name.Length);
        U32(data,offset+8,0x200);U32(data,offset+12,address);U32(data,offset+16,0x200);U32(data,offset+20,raw);U32(data,offset+36,flags);
    }
    static byte[] Image() {
        var data=new byte[0xA00];U16(data,0,0x5A4D);U32(data,0x3C,0x80);U32(data,0x80,0x4550);
        U16(data,0x84,0x14C);U16(data,0x86,3);U16(data,0x94,0xE0);U16(data,0x98,0x10B);
        U32(data,0x98+28,0x400000);U32(data,0x98+32,0x1000);U32(data,0x98+36,0x200);
        U32(data,0x98+56,0x4000);U32(data,0x98+60,0x400);U32(data,0x98+92,16);
        Section(data,0x178,".text",0x1000,0x400,0x60000020);
        Section(data,0x178+40,".data",0x2000,0x600,0xC0000040);
        Section(data,0x178+80,".rsrc",0x3000,0x800,0x40000040);
        for(int i=0;i<16;i++){data[0x400+i]=(byte)(i+1);data[0x420+i]=(byte)(0x91+i);}
        return data;
    }
    static GameBuildInfo Inspect(string folder,string name,byte[] bytes) {
        var path=Path.Combine(folder,name);File.WriteAllBytes(path,bytes);return GameCompatibility.Inspect(path);
    }
    static int Main(string[] args) {
        string folder=Path.GetFullPath(args[0]);Directory.CreateDirectory(folder);
        var baseline=Image();var original=Inspect(folder,"original.exe",baseline);
        Check(original.Supported&&original.CodeMatched==2&&original.RegionsMatched==1,"Valid layout rejected");
        var headers=(byte[])baseline.Clone();headers[0x88]=91;
        var headerBuild=Inspect(folder,"different-headers.exe",headers);
        Check(headerBuild.Supported&&headerBuild.ExecutableHash!=original.ExecutableHash,"Checksum-only header difference rejected");
        var resources=(byte[])baseline.Clone();resources[0x850]=123;
        Check(Inspect(folder,"different-resources.exe",resources).Supported,"Resource-only change rejected");
        var overlay=new byte[baseline.Length+31];Array.Copy(baseline,overlay,baseline.Length);overlay[overlay.Length-1]=44;
        Check(Inspect(folder,"different-overlay.exe",overlay).Supported,"Overlay-only change rejected");
        var code=(byte[])baseline.Clone();code[0x401]^=1;
        Check(!Inspect(folder,"different-code.exe",code).Supported,"Changed native code accepted");
        var rebased=(byte[])baseline.Clone();U32(rebased,0x98+28,0x500000);
        Check(!Inspect(folder,"different-base.exe",rebased).Supported,"Different native address base accepted");
        var x64=(byte[])baseline.Clone();U16(x64,0x84,0x8664);
        Check(!Inspect(folder,"wrong-machine.exe",x64).Supported,"Different architecture accepted");
        var readonlyData=(byte[])baseline.Clone();U32(readonlyData,0x178+40+36,0x40000040);
        Check(!Inspect(folder,"readonly-globals.exe",readonlyData).Supported,"Non-writable native data accepted");
        var rawOverflow=(byte[])baseline.Clone();U32(rawOverflow,0x178+20,0xFFFFFFF0);
        Check(!Inspect(folder,"bad-section.exe",rawOverflow).Supported,"Invalid section bounds accepted");
        var overlap=(byte[])baseline.Clone();U32(overlap,0x178+80+12,0x1000);
        Check(!Inspect(folder,"overlapping-sections.exe",overlap).Supported,"Ambiguous mapping accepted");
        foreach(int length in new[]{0,2,63,128,200,450}) {
            var truncated=new byte[length];Array.Copy(baseline,truncated,Math.Min(length,baseline.Length));
            Check(!Inspect(folder,"truncated-"+length+".exe",truncated).Supported,"Truncated executable accepted");
        }
        Console.WriteLine("PASS: compatible header/resource/overlay variants; changed code, wrong layouts and malformed executables rejected.");
        return 0;
    }
}
