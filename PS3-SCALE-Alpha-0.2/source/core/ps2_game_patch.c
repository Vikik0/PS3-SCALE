#include "ps3scale/ps2_game_patch.h"
#include <string.h>
/* Pinned PCSX2 patch values. Original opcodes are read from the exact matched
 * boot ELF and used as guards in netemu CONFIG command 0x0A. */
#define PCSX2_PATCH_COMMIT "1fd98102b8e2fffe3e0c29aab55339d48af0ffe5"
#define PCSX2_PATCH_URL(file) "https://github.com/PCSX2/pcsx2_patches/blob/" PCSX2_PATCH_COMMIT "/patches/" file
static const ps3scale_ps2_patch_word_t fps_ed[]={{0x00161860u,0x2C620000u}};
static const ps3scale_ps2_patch_word_t fps_scooby[]={{0x00131DB0u,0x2C420001u}};
static const ps3scale_ps2_patch_word_t fps_titans[]={{0x0015FF94u,0x2C620000u}};
static const ps3scale_ps2_patch_word_t fps_kim[]={{0x0015C844u,0x2C620000u}};
static const ps3scale_ps2_patch_word_t fps_happy[]={{0x0015CDD4u,0x2C620000u}};
static const ps3scale_ps2_patch_word_t fps_ant[]={{0x0015CCD4u,0x2C620000u}};
static const ps3scale_ps2_patch_word_t fps_indiana[]={{0x001505B4u,0x2C620000u}};
static const ps3scale_ps2_patch_word_t fps_jimmy[]={{0x0022CA0Cu,0x28420001u}};
static const ps3scale_ps2_patch_word_t ws_scooby[]={
 {0x001E6268u,0x080A04ACu},{0x002812B0u,0x3C013F40u},{0x002812B4u,0x4481F000u},
 {0x002812B8u,0xC6010068u},{0x002812BCu,0xC602006Cu},{0x002812C0u,0x461E0843u},
 {0x002812C4u,0xE6010068u},{0x002812C8u,0x0807989Cu}};
static const ps3scale_ps2_patch_word_t ws_titans[]={
 {0x00205784u,0x080F1E0Cu},{0x00205788u,0x00000000u},{0x003C7830u,0x3C013F40u},
 {0x003C7834u,0x4481F000u},{0x003C7838u,0x461EAD43u},{0x003C783Cu,0x4600AB06u},
 {0x003C7840u,0x0C072734u},{0x003C7844u,0x00000000u},{0x003C7848u,0x080815E2u},
 {0x001C9D78u,0x3C023FC0u},{0x001C9DA8u,0x3C013F40u},{0x001C9DACu,0x4481F000u},
 {0x001C9DB4u,0x461E0342u}};
static const ps3scale_ps2_patch_word_t ws_kim[]={
 {0x001CA558u,0x3C023F2Au},{0x001CAFC4u,0x3C024184u},{0x001CAFC8u,0x3442C4EAu},
 {0x001CA958u,0x3C013FE1u},{0x001CA95Cu,0x4481F000u},{0x001CA960u,0x461E6B42u},
 {0x001CA964u,0x461E0343u}};
static const ps3scale_ps2_patch_word_t ws_jimmy[]={
 {0x002338A4u,0x3C023F1Du},{0x00233900u,0x3C03BFAAu},{0x00232018u,0x3C023F2Bu}};
static const ps3scale_ps2_patch_word_t p480_crash_titans[]={
 {0x0057E9D4u,0x3C050000u},{0x0057E9DCu,0x3C060050u},{0x0057E9E4u,0x3C070001u}};
static const ps3scale_ps2_patch_word_t p480_george_jungle[]={
 {0x0038665Cu,0x24110000u},{0x00386660u,0x24120050u},{0x0038666Cu,0x24130001u}};
static const ps3scale_ps2_patch_word_t p480_syberia[]={
 {0x001E6314u,0x24060050u}};
#define PATCH(s,h,t,f,k,w) {s,h,t,PCSX2_PATCH_URL(f),k,w,sizeof(w)/sizeof((w)[0])}
static const ps3scale_ps2_game_patch_t g_patches[]={
 PATCH("SLUS_212.60",0xB0AE2D8Au,"Ed, Edd n Eddy: The Mis-Edventures (NTSC-U)","SLUS-21260_B0AE2D8A.pnach",PS3SCALE_PS2_PATCH_FPS,fps_ed),
 PATCH("SLUS_203.49",0x10B1F370u,"Scooby-Doo! Night of 100 Frights (NTSC-U)","SLUS-20349_10B1F370.pnach",PS3SCALE_PS2_PATCH_FPS,fps_scooby),
 PATCH("SLUS_211.83",0xE1F17139u,"Teen Titans (NTSC-U)","SLUS-21183_E1F17139.pnach",PS3SCALE_PS2_PATCH_FPS,fps_titans),
 PATCH("SLUS_214.37",0x86B490D1u,"Disney's Kim Possible - What's the Switch (NTSC-U)","SLUS-21437_86B490D1.pnach",PS3SCALE_PS2_PATCH_FPS,fps_kim),
 PATCH("SLUS_214.55",0xBAFEFC57u,"Happy Feet (NTSC-U)","SLUS-21455_BAFEFC57.pnach",PS3SCALE_PS2_PATCH_FPS,fps_happy),
 PATCH("SLUS_214.15",0xE10A865Bu,"The Ant Bully (NTSC-U)","SLUS-21415_E10A865B.pnach",PS3SCALE_PS2_PATCH_FPS,fps_ant),
 PATCH("SLUS_218.85",0xB15F0BD6u,"Indiana Jones and the Staff of Kings (NTSC-U)","SLUS-21885_B15F0BD6.pnach",PS3SCALE_PS2_PATCH_FPS,fps_indiana),
 PATCH("SLUS_206.96",0xFCC09959u,"Jimmy Neutron: Jet Fusion (NTSC-U)","SLUS-20696_FCC09959.pnach",PS3SCALE_PS2_PATCH_FPS,fps_jimmy),
 PATCH("SLUS_203.49",0x10B1F370u,"Scooby-Doo! Night of 100 Frights (NTSC-U)","SLUS-20349_10B1F370.pnach",PS3SCALE_PS2_PATCH_WIDESCREEN,ws_scooby),
 PATCH("SLUS_211.83",0xE1F17139u,"Teen Titans (NTSC-U)","SLUS-21183_E1F17139.pnach",PS3SCALE_PS2_PATCH_WIDESCREEN,ws_titans),
 PATCH("SLUS_214.37",0x86B490D1u,"Disney's Kim Possible - What's the Switch (NTSC-U)","SLUS-21437_86B490D1.pnach",PS3SCALE_PS2_PATCH_WIDESCREEN,ws_kim),
 PATCH("SLUS_206.96",0xFCC09959u,"Jimmy Neutron: Jet Fusion (NTSC-U)","SLUS-20696_FCC09959.pnach",PS3SCALE_PS2_PATCH_WIDESCREEN,ws_jimmy),
 PATCH("SLES_548.43",0xAEFD7D25u,"Crash of the Titans (PAL-R)","SLES-54843_AEFD7D25.pnach",PS3SCALE_PS2_PATCH_480P,p480_crash_titans),
 PATCH("SLES_549.75",0x278B7BEAu,"George of the Jungle (PAL-M)","SLES-54975_278B7BEA.pnach",PS3SCALE_PS2_PATCH_480P,p480_george_jungle),
 PATCH("SLES_513.93",0xD1ACD489u,"Syberia (PAL)","SLES-51393_D1ACD489.pnach",PS3SCALE_PS2_PATCH_480P,p480_syberia)
};
const ps3scale_ps2_game_patch_t *ps3scale_ps2_game_patch_find(
 const char *serial,uint32_t hash,ps3scale_ps2_patch_kind_t kind)
{
 size_t i;
 if(!serial||!serial[0]||(kind!=PS3SCALE_PS2_PATCH_FPS&&kind!=PS3SCALE_PS2_PATCH_WIDESCREEN&&kind!=PS3SCALE_PS2_PATCH_480P))return NULL;
 for(i=0;i<sizeof(g_patches)/sizeof(g_patches[0]);++i)
  if(g_patches[i].kind==kind&&strcmp(serial,g_patches[i].serial)==0&&hash==g_patches[i].pcsx2_elf_hash)return &g_patches[i];
 return NULL;
}
