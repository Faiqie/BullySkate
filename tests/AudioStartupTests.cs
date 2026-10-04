using System;
using BullySkate;
class AudioStartupTests {
 static int Main(){
  foreach(uint code in new uint[]{0x80040154,0x800401F8,0x8007007E})if(!AudioStartup.MissingRuntime(unchecked((int)code)))throw new Exception("Missing runtime was not recognized.");
  foreach(uint code in new uint[]{0,0x80004005,0x80070005,0x80004002})if(AudioStartup.MissingRuntime(unchecked((int)code)))throw new Exception("An unrelated sound/device failure triggered an installer.");
  int actual=AudioStartup.ProbeXact();Console.WriteLine("Actual x86 Bully XACT probe: 0x"+actual.ToString("X8"));
  if(actual<0)throw new Exception("Installed XACT 3.1 did not initialize on this test PC.");
  Console.WriteLine("PASS: native x86 audio activation and missing-runtime classification.");return 0;
 }
}
