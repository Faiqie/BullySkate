/* Authored test process, not game code. Changes its own marker data after
   startup to exercise a file/memory difference without modifying a game. */
#include <windows.h>
#include <string.h>
#include "game_compat.h"
#include "native-sha-vectors.h"
#pragma section(".probe",execute,read)
__declspec(allocate(".probe")) __declspec(dllexport) volatile unsigned char compatCodeA[16]={0};
__declspec(allocate(".probe")) __declspec(dllexport) volatile unsigned char compatCodeB[16]={0};
__declspec(dllexport) volatile DWORD compatData=123;
int main(void){
    DWORD protection,previous,written;unsigned i;char eventName[80];char *command;HANDLE stop;
    unsigned char sequence[55];
    for(i=0;i<sizeof(sequence);i++)sequence[i]=(unsigned char)i;
    for(i=0;i<sizeof(shaLengths)/sizeof(shaLengths[0]);i++)if(!bullyFingerprintMatches(sequence,shaLengths[i],shaExpected[i]))return 9;
    if(bullyFingerprintMatches(NULL,1,shaExpected[0])||bullyFingerprintMatches(sequence,56,shaExpected[0]))return 10;
    if(!VirtualProtect(compatCodeA,16,PAGE_EXECUTE_READWRITE,&protection))return 11;
    for(i=0;i<16;i++)compatCodeA[i]=(unsigned char)(i+1);
    VirtualProtect(compatCodeA,16,protection,&previous);
    if(!VirtualProtect(compatCodeB,16,PAGE_EXECUTE_READWRITE,&protection))return 12;
    for(i=0;i<16;i++)compatCodeB[i]=(unsigned char)(0x91+i);
    command=GetCommandLineA();
    if(strstr(command,"--bad"))compatCodeB[0]^=1;
    VirtualProtect(compatCodeB,16,protection,&previous);
    FlushInstructionCache(GetCurrentProcess(),compatCodeA,16);FlushInstructionCache(GetCurrentProcess(),compatCodeB,16);
    wsprintfA(eventName,"Local\\BullySkateProbe-%lu",GetCurrentProcessId());
    stop=CreateEventA(NULL,TRUE,FALSE,eventName);if(!stop)return 13;
    {char ready[8]={'r','e','a','d','y',strstr(command,"--bad")?'b':'g',compatCodeB[0]==0x90?'b':'g','\n'};WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),ready,8,&written,NULL);}
    WaitForSingleObject(stop,30000);CloseHandle(stop);
    return 0;
}
