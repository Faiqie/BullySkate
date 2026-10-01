#ifndef BULLYSKATE_GAME_COMPAT_H
#define BULLYSKATE_GAME_COMPAT_H
/* Minimum in-memory guard, checked BEFORE the loader writes any game hooks.
   The launcher separately verifies every referenced file code/data location.
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
#endif
