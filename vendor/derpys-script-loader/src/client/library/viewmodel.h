/* The skating build has no weapon renderer or embedded game mesh. */
static int weapon_enabled=0;
void fakieBeforeDeviceReset(IDirect3DDevice9 *device) {(void)device;}
static int FS_Weapon(lua_State *lua) {(void)lua;return 0;}
static void weapon_draw(IDirect3DDevice9 *device,IDirect3DSurface9 *depth,float clock) {(void)device;(void)depth;(void)clock;}
