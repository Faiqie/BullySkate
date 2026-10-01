#ifndef BULLYSKATE_GAME_COMPAT_H
#define BULLYSKATE_GAME_COMPAT_H
#include "game_layout_fingerprints.h"
/* Minimum check used by the bridge after startup. The loader also requires
   all loaded fingerprints/data mappings below BEFORE writing its first hook.
   The 1.200 version marker is also used by SilentPatchBully's InjectHooks:
   https://github.com/CookiePLMonster/SilentPatchBully/blob/master/SilentPatchBully/SilentPatchBully.cpp */
static int bullyNativeLayoutAvailable(void){
    __try {
        unsigned char *base=(unsigned char*)GetModuleHandleA(NULL);
        IMAGE_DOS_HEADER *dos=(IMAGE_DOS_HEADER*)base;
        IMAGE_NT_HEADERS32 *nt;
        if(base!=(unsigned char*)0x400000||dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0x40||dos->e_lfanew>0x100000)return 0;
        nt=(IMAGE_NT_HEADERS32*)(base+dos->e_lfanew);
        if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386||
           nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC||nt->OptionalHeader.ImageBase!=0x400000||
           nt->OptionalHeader.SizeOfImage<0x1CE6D08)return 0;
        return !memcmp((void*)0x860C6B,"\xC7\x45\xFC\xFE\xFF\xFF\xFF",7)&&
            !memcmp((void*)0x461EA0,"\x83\xEC\x3C\x66\x81\x3D\x88\xAE\xC1\x00\xFF\xFF",12)&&
            !memcmp((void*)0x5C83B0,"\x83\xEC\x0C\x56\x8B\x74\x24\x14",8);
    } __except(EXCEPTION_EXECUTE_HANDLER) {return 0;}
}
/* Startup only. UAL skips its .bind callbacks before loading ASIs, leaving
   Steam authentication/unwrapping to Steam. Never patch an unverified engine. */
static int bullyMappedRange(DWORD address,DWORD length,int executable,int writable){
    MEMORY_BASIC_INFORMATION info;DWORD protection;int canExecute,canWrite;
    if(VirtualQuery((void*)address,&info,sizeof(info))!=sizeof(info))return 0;
    protection=info.Protect&0xFF;
    canExecute=protection==PAGE_EXECUTE||protection==PAGE_EXECUTE_READ||protection==PAGE_EXECUTE_READWRITE||protection==PAGE_EXECUTE_WRITECOPY;
    canWrite=protection==PAGE_READWRITE||protection==PAGE_WRITECOPY||protection==PAGE_EXECUTE_READWRITE||protection==PAGE_EXECUTE_WRITECOPY;
    return info.State==MEM_COMMIT&&!(info.Protect&PAGE_GUARD)&&protection&&protection!=PAGE_NOACCESS&&
        (ULONGLONG)address+length<=(ULONGLONG)(DWORD)info.BaseAddress+info.RegionSize&&
        (!executable||canExecute)&&(!writable||canWrite);
}
static DWORD bullyRotateRight(DWORD value,unsigned count){return (value>>count)|(value<<(32-count));}
/* Single-block SHA-256 for short code fingerprints. No allocation or provider
   DLL loads: safe for the legacy DllMain startup path. Profile lengths <=55. */
static int bullyFingerprintMatches(const void *address,DWORD length,const unsigned char expected[32]){
    static const DWORD constants[64]={
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    DWORD initial[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    DWORD w[64],a,b,c,d,e,f,g,h,i,t1,t2;unsigned char block[64]={0};
    if(length>55)return 0;
    __try {for(i=0;i<length;i++)block[i]=((const unsigned char*)address)[i];}
    __except(EXCEPTION_EXECUTE_HANDLER){return 0;}
    block[length]=0x80;block[62]=(unsigned char)((length*8)>>8);block[63]=(unsigned char)(length*8);
    for(i=0;i<16;i++)w[i]=((DWORD)block[i*4]<<24)|((DWORD)block[i*4+1]<<16)|((DWORD)block[i*4+2]<<8)|block[i*4+3];
    for(i=16;i<64;i++)w[i]=w[i-16]+(bullyRotateRight(w[i-15],7)^bullyRotateRight(w[i-15],18)^(w[i-15]>>3))+w[i-7]+(bullyRotateRight(w[i-2],17)^bullyRotateRight(w[i-2],19)^(w[i-2]>>10));
    a=initial[0];b=initial[1];c=initial[2];d=initial[3];e=initial[4];f=initial[5];g=initial[6];h=initial[7];
    for(i=0;i<64;i++){
        t1=h+(bullyRotateRight(e,6)^bullyRotateRight(e,11)^bullyRotateRight(e,25))+((e&f)^((~e)&g))+constants[i]+w[i];
        t2=(bullyRotateRight(a,2)^bullyRotateRight(a,13)^bullyRotateRight(a,22))+((a&b)^(a&c)^(b&c));
        h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    initial[0]+=a;initial[1]+=b;initial[2]+=c;initial[3]+=d;initial[4]+=e;initial[5]+=f;initial[6]+=g;initial[7]+=h;
    for(i=0;i<32;i++)if(expected[i]!=(unsigned char)(initial[i/4]>>(24-8*(i%4))))return 0;
    return 1;
}
static int bullyVerifyLoadedEngine(unsigned *codeMatched,unsigned *dataMatched,DWORD *firstMismatch){
    unsigned i;DWORD imageSize;
    *codeMatched=0;*dataMatched=0;*firstMismatch=0;
    if(!bullyNativeLayoutAvailable())return 0;
    imageSize=((IMAGE_NT_HEADERS32*)((char*)0x400000+((IMAGE_DOS_HEADER*)0x400000)->e_lfanew))->OptionalHeader.SizeOfImage;
    for(i=0;i<sizeof(bullyCodeProbes)/sizeof(bullyCodeProbes[0]);i++){
        const BullyCodeProbe *probe=&bullyCodeProbes[i];
        if((ULONGLONG)probe->address+probe->length<=(ULONGLONG)0x400000+imageSize&&bullyMappedRange(probe->address,probe->length,1,0)&&
           bullyFingerprintMatches((void*)probe->address,probe->length,probe->hash))(*codeMatched)++;
        else if(!*firstMismatch)*firstMismatch=probe->address;
    }
    for(i=0;i<sizeof(bullyDataProbes)/sizeof(bullyDataProbes[0]);i++){
        const BullyDataProbe *probe=&bullyDataProbes[i];
        if((ULONGLONG)probe->address+probe->length<=(ULONGLONG)0x400000+imageSize&&bullyMappedRange(probe->address,probe->length,0,probe->writable))(*dataMatched)++;
        else if(!*firstMismatch)*firstMismatch=probe->address;
    }
    return *codeMatched==sizeof(bullyCodeProbes)/sizeof(bullyCodeProbes[0])&&*dataMatched==sizeof(bullyDataProbes)/sizeof(bullyDataProbes[0]);
}
#endif
