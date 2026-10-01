using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Diagnostics;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Xml;

namespace BullySkate {
    public sealed class GameBuildInfo {
        public bool Supported;
        public bool StructureValid,SteamWrapped,SilentPatchDetected;
        public string ValidationSource="Executable file",Sections;
        public readonly List<uint> UnmatchedCode=new List<uint>();
        public string Profile,ExecutableHash,Problem;
        public uint ImageBase,ImageSize;
        public ushort Machine;
        public int CodeMatched,CodeTotal,RegionsMatched,RegionsTotal;
        public string Report() {
            return "BullySkate executable compatibility report\r\n"+
                "Supported: "+Supported+"\r\nProfile: "+Profile+"\r\nValidation source: "+ValidationSource+"\r\n"+
                "Executable SHA-256: "+ExecutableHash+"\r\nMachine: 0x"+Machine.ToString("X4")+
                "\r\nImage base: 0x"+ImageBase.ToString("X8")+"\r\nImage size: 0x"+ImageSize.ToString("X8")+
                "\r\nCode locations matched: "+CodeMatched+"/"+CodeTotal+
                "\r\nData locations matched: "+RegionsMatched+"/"+RegionsTotal+
                "\r\nSections: "+Sections+"\r\nSteam wrapper detected: "+SteamWrapped+
                "\r\nSilentPatch detected in process: "+SilentPatchDetected+
                "\r\nUnmatched code locations: "+String.Join(",",UnmatchedCode.ConvertAll(a=>"0x"+a.ToString("X8")).ToArray())+
                "\r\nResult: "+(Supported?"Compatible native engine layout.":Problem)+"\r\n";
        }
    }
    public static class GameCompatibility {
        sealed class Section {public uint Address,VirtualSize,RawSize,RawOffset,Flags;public string Name;}
        sealed class Probe {public uint Address;public int Length;public string Hash;public bool Writable;}
        sealed class Layout {
            public string Id;public uint ImageBase;public ushort Machine;
            public readonly List<Probe> Code=new List<Probe>(),Regions=new List<Probe>();
        }
        static readonly Lazy<Layout> Expected=new Lazy<Layout>(Load);
        static uint Hex(string text) {return UInt32.Parse(text,NumberStyles.HexNumber,CultureInfo.InvariantCulture);}
        static Layout Load() {
            var document=new XmlDocument();document.XmlResolver=null;
            using(var stream=typeof(GameCompatibility).Assembly.GetManifestResourceStream("BullySkate.game-layout")) {
                if(stream==null)throw new InvalidDataException("The launcher has no engine compatibility profile.");
                using(var reader=XmlReader.Create(stream,new XmlReaderSettings {DtdProcessing=DtdProcessing.Prohibit,XmlResolver=null}))document.Load(reader);
            }
            var root=document.DocumentElement;
            var layout=new Layout {Id=root.GetAttribute("id"),ImageBase=Hex(root.GetAttribute("image-base")),Machine=(ushort)Hex(root.GetAttribute("machine"))};
            foreach(XmlElement node in root.SelectNodes("code/probe"))layout.Code.Add(new Probe {Address=Hex(node.GetAttribute("address")),Length=Int32.Parse(node.GetAttribute("length"),CultureInfo.InvariantCulture),Hash=node.GetAttribute("sha256")});
            foreach(XmlElement node in root.SelectNodes("regions/region"))layout.Regions.Add(new Probe {Address=Hex(node.GetAttribute("address")),Length=Int32.Parse(node.GetAttribute("length"),CultureInfo.InvariantCulture),Writable=node.GetAttribute("writable")=="true"});
            if(layout.Code.Count==0||layout.Regions.Count==0)throw new InvalidDataException("The engine compatibility profile is incomplete.");
            return layout;
        }
        static bool Range(byte[] data,long offset,long count) {return offset>=0&&count>=0&&offset<=data.LongLength&&count<=data.LongLength-offset;}
        static ushort U16(byte[] data,long offset) {if(!Range(data,offset,2))throw new InvalidDataException("Truncated executable header.");return BitConverter.ToUInt16(data,(int)offset);}
        static uint U32(byte[] data,long offset) {if(!Range(data,offset,4))throw new InvalidDataException("Truncated executable header.");return BitConverter.ToUInt32(data,(int)offset);}
        static string Hash(byte[] data,int offset,int count) {
            using(var algorithm=SHA256.Create())return BitConverter.ToString(algorithm.ComputeHash(data,offset,count)).Replace("-","").ToLowerInvariant();
        }
        static Section Locate(List<Section> sections,uint rva,int length) {
            Section found=null;
            foreach(var section in sections)if(rva>=section.Address&&(ulong)rva+(uint)length<=(ulong)section.Address+Math.Max(section.VirtualSize,section.RawSize)) {
                if(found!=null)return null;found=section;
            }
            return found;
        }
        static void Problem(GameBuildInfo info,string message) {if(info.Problem==null)info.Problem=message;}
        public static GameBuildInfo Inspect(string path) {
            var layout=Expected.Value;
            var info=new GameBuildInfo {Profile=layout.Id,CodeTotal=layout.Code.Count,RegionsTotal=layout.Regions.Count};
            try {
                var file=new FileInfo(path);
                if(!file.Exists)throw new InvalidDataException("Bully.exe was not found.");
                if(file.Length>128L*1024*1024)throw new InvalidDataException("Executable exceeds the compatibility check's size limit.");
                var data=File.ReadAllBytes(path);info.ExecutableHash=Hash(data,0,data.Length);
                if(data.Length<64||U16(data,0)!=0x5A4D)throw new InvalidDataException("This is not a Windows executable.");
                long pe=U32(data,0x3C);
                if(U32(data,pe)!=0x4550)throw new InvalidDataException("The PE header is missing.");
                info.Machine=U16(data,pe+4);int count=U16(data,pe+6),optionalSize=U16(data,pe+20);
                long optional=pe+24;
                if(count<1||count>96||optionalSize<96||!Range(data,optional,optionalSize)||U16(data,optional)!=0x10B)throw new InvalidDataException("A valid 32-bit PE executable is required.");
                info.ImageBase=U32(data,optional+28);info.ImageSize=U32(data,optional+56);
                if(info.Machine!=layout.Machine||info.ImageBase!=layout.ImageBase)throw new InvalidDataException("The executable uses a different CPU architecture or native address layout.");
                long table=optional+optionalSize;
                if(!Range(data,table,count*40L))throw new InvalidDataException("Truncated executable section table.");
                var sections=new List<Section>();uint entryPoint=U32(data,optional+16);
                for(int i=0;i<count;i++) {
                    long entry=table+i*40;
                    var section=new Section {Name=Encoding.ASCII.GetString(data,(int)entry,8).TrimEnd('\0'),VirtualSize=U32(data,entry+8),Address=U32(data,entry+12),RawSize=U32(data,entry+16),RawOffset=U32(data,entry+20),Flags=U32(data,entry+36)};
                    if((ulong)section.Address+Math.Max(section.VirtualSize,section.RawSize)>info.ImageSize||!Range(data,section.RawOffset,section.RawSize))throw new InvalidDataException("Invalid executable section bounds.");
                    sections.Add(section);
                    if(section.Name==".bind"&&(section.Flags&0x20000000)!=0&&entryPoint>=section.Address&&(ulong)entryPoint<(ulong)section.Address+Math.Max(section.VirtualSize,section.RawSize))info.SteamWrapped=true;
                }
                info.Sections=String.Join(",",sections.ConvertAll(s=>s.Name).ToArray());info.StructureValid=true;
                for(int i=0;i<sections.Count;i++)for(int j=i+1;j<sections.Count;j++) {
                    var a=sections[i];var b=sections[j];
                    ulong aEnd=(ulong)a.Address+Math.Max(a.VirtualSize,a.RawSize),bEnd=(ulong)b.Address+Math.Max(b.VirtualSize,b.RawSize);
                    if(aEnd>a.Address&&bEnd>b.Address&&a.Address<bEnd&&b.Address<aEnd){info.StructureValid=false;throw new InvalidDataException("Overlapping executable section mappings.");}
                }
                foreach(var probe in layout.Code) {
                    uint rva=probe.Address-layout.ImageBase;var section=Locate(sections,rva,probe.Length);
                    long offset=section==null?0:(long)section.RawOffset+rva-section.Address;
                    bool mapped=section!=null&&(section.Flags&0x20000000)!=0&&rva>=section.Address&&
                        (ulong)rva-section.Address+(uint)probe.Length<=section.RawSize&&Range(data,offset,probe.Length);
                    if(mapped&&Hash(data,(int)offset,probe.Length)==probe.Hash)info.CodeMatched++;
                    else {info.UnmatchedCode.Add(probe.Address);Problem(info,"Native code differs at 0x"+probe.Address.ToString("X8")+". Check the running engine to distinguish wrapped code from a different layout.");}
                }
                foreach(var region in layout.Regions) {
                    uint rva=region.Address-layout.ImageBase;var section=Locate(sections,rva,region.Length);
                    if(section!=null&&(section.Flags&0x40000000)!=0&&(!region.Writable||(section.Flags&0x80000000)!=0))info.RegionsMatched++;
                    else Problem(info,"Native data layout differs at 0x"+region.Address.ToString("X8")+".");
                }
                info.Supported=info.CodeMatched==info.CodeTotal&&info.RegionsMatched==info.RegionsTotal;
            } catch(InvalidDataException error) {Problem(info,error.Message);}
            catch(IOException error) {Problem(info,"Could not read the executable: "+error.GetType().Name);}
            catch(UnauthorizedAccessException) {Problem(info,"Windows denied read access to the executable.");}
            return info;
        }
        public static void Require(string path,Action<string> progress) {
            var info=Inspect(path);
            if(CanDeferToRuntime(info)) {
                if(progress!=null)progress("Steam-wrapped executable detected. The ASI will verify all engine code/data locations in memory before installing game hooks.");
                return;
            }
            if(!info.Supported)throw new InvalidDataException("This Bully executable has a different native engine layout. "+info.Problem+" Use CheckBully.cmd to create a compatibility report. Stock Steam users can also verify their game files; retail versions need the official 1.200 update.");
            if(progress!=null)progress("Bully native engine layout verified ("+info.CodeMatched+" code locations). Different executable checksums are supported.");
        }
        public static bool CanDeferToRuntime(GameBuildInfo info) {
            return info.StructureValid&&info.SteamWrapped&&info.RegionsMatched==info.RegionsTotal;
        }
        [StructLayout(LayoutKind.Sequential)] struct MemoryRegion {
            public IntPtr BaseAddress,AllocationBase;public uint AllocationProtect;public UIntPtr RegionSize;public uint State,Protect,Type;
        }
        [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr OpenProcess(uint access,bool inherit,int pid);
        [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
        [DllImport("kernel32.dll",SetLastError=true)] static extern bool ReadProcessMemory(IntPtr handle,IntPtr address,byte[] data,int count,out IntPtr read);
        [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool QueryFullProcessImageName(IntPtr handle,uint flags,StringBuilder name,ref int length);
        [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern uint GetFinalPathNameByHandle(IntPtr handle,StringBuilder name,uint length,uint flags);
        [DllImport("kernel32.dll",SetLastError=true)] static extern UIntPtr VirtualQueryEx(IntPtr handle,IntPtr address,out MemoryRegion region,UIntPtr size);
        static byte[] ReadMemory(IntPtr handle,uint address,int length) {
            var data=new byte[length];IntPtr read;
            if(!ReadProcessMemory(handle,new IntPtr(unchecked((int)address)),data,length,out read)||read.ToInt64()!=length)throw new InvalidDataException("A required process-memory location is not readable.");
            return data;
        }
        static bool MemoryMatches(IntPtr handle,uint address,int length,bool executable,bool writable) {
            MemoryRegion region;
            if(VirtualQueryEx(handle,new IntPtr(unchecked((int)address)),out region,new UIntPtr((uint)Marshal.SizeOf(typeof(MemoryRegion))))==UIntPtr.Zero)return false;
            ulong start=unchecked((uint)region.BaseAddress.ToInt32()),end=start+region.RegionSize.ToUInt64();
            uint protection=region.Protect&0xFF;
            bool execute=protection==0x10||protection==0x20||protection==0x40||protection==0x80;
            bool write=protection==0x04||protection==0x08||protection==0x40||protection==0x80;
            return region.State==0x1000&&(region.Protect&0x100)==0&&protection!=0&&protection!=1&&address>=start&&(ulong)address+(uint)length<=end&&(!executable||execute)&&(!writable||write);
        }
        static string CanonicalFile(string path) {
            using(var stream=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete)) {
                var name=new StringBuilder(32768);
                uint size=GetFinalPathNameByHandle(stream.SafeFileHandle.DangerousGetHandle(),name,(uint)name.Capacity,0);
                if(size==0||size>=name.Capacity)throw new IOException("The selected executable path could not be resolved.");
                return name.ToString();
            }
        }
        public static GameBuildInfo InspectRunning(string path) {
            path=Path.GetFullPath(path);var file=Inspect(path);var layout=Expected.Value;
            var info=new GameBuildInfo {Profile=layout.Id,ExecutableHash=file.ExecutableHash,ValidationSource="Running process memory (read only)",SteamWrapped=file.SteamWrapped,Sections=file.Sections,CodeTotal=layout.Code.Count,RegionsTotal=layout.Regions.Count};
            if(!file.StructureValid){info.Problem=file.Problem;return info;}
            bool accessDenied=false;string selected=CanonicalFile(path);
            // Use the OS image path, rather than cached performance-counter names.
            // Canonical paths also handle Windows drive aliases and junctions.
            foreach(var process in Process.GetProcesses())using(process) {
                // Query/read access only: no process writes, remote threads, suspension or injection.
                IntPtr handle=OpenProcess(0x410,false,process.Id);
                if(handle==IntPtr.Zero){try {if(process.ProcessName=="Bully")accessDenied=true;}catch(InvalidOperationException){}continue;}
                try {
                    var name=new StringBuilder(32768);int nameLength=name.Capacity;
                    if(!QueryFullProcessImageName(handle,0,name,ref nameLength))continue;
                    if(!String.Equals(Path.GetFileName(name.ToString()),"Bully.exe",StringComparison.OrdinalIgnoreCase))continue;
                    try {if(!String.Equals(CanonicalFile(name.ToString()),selected,StringComparison.OrdinalIgnoreCase))continue;}
                    catch(IOException){continue;}catch(UnauthorizedAccessException){accessDenied=true;continue;}
                    try {
                        var dos=ReadMemory(handle,layout.ImageBase,64);long pe=U32(dos,0x3C);
                        if(U16(dos,0)!=0x5A4D||pe<64||pe>1024*1024)throw new InvalidDataException("Loaded executable header differs.");
                        var header=ReadMemory(handle,layout.ImageBase+(uint)pe,120);
                        info.Machine=U16(header,4);info.ImageBase=U32(header,24+28);info.ImageSize=U32(header,24+56);
                        if(U32(header,0)!=0x4550||info.Machine!=layout.Machine||U16(header,24)!=0x10B||info.ImageBase!=layout.ImageBase)throw new InvalidDataException("Loaded executable architecture/base differs.");
                        info.StructureValid=true;
                        foreach(var probe in layout.Code) {
                            bool match=false;
                            if((ulong)probe.Address+(uint)probe.Length<=(ulong)layout.ImageBase+info.ImageSize&&MemoryMatches(handle,probe.Address,probe.Length,true,false)) {
                                try {var bytes=ReadMemory(handle,probe.Address,probe.Length);match=Hash(bytes,0,bytes.Length)==probe.Hash;}catch(InvalidDataException){}
                            }
                            if(match)info.CodeMatched++;else {info.UnmatchedCode.Add(probe.Address);Problem(info,"Loaded native code differs at 0x"+probe.Address.ToString("X8")+". Other ASI hooks can also change code after startup.");}
                        }
                        foreach(var region in layout.Regions) {
                            if((ulong)region.Address+(uint)region.Length<=(ulong)layout.ImageBase+info.ImageSize&&MemoryMatches(handle,region.Address,region.Length,false,region.Writable))info.RegionsMatched++;
                            else Problem(info,"Loaded native data differs at 0x"+region.Address.ToString("X8")+".");
                        }
                        try {foreach(ProcessModule module in process.Modules)if(module.ModuleName.IndexOf("SilentPatch",StringComparison.OrdinalIgnoreCase)>=0)info.SilentPatchDetected=true;}catch(System.ComponentModel.Win32Exception){}
                        info.Supported=info.CodeMatched==info.CodeTotal&&info.RegionsMatched==info.RegionsTotal;
                    }catch(InvalidDataException error){Problem(info,error.Message);}
                    return info;
                }finally {CloseHandle(handle);}
            }
            info.Problem=accessDenied?"Windows denied read access to a running Bully process. Use the same Windows account and elevation level for the checker and game.":"No running Bully process matches the selected executable. Start this copy normally through Steam, reach the main menu, leave it open, and run CheckBullyRunning.cmd again.";
            return info;
        }
    }
}
