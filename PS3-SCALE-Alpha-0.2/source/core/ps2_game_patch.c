#include "ps3scale/ps2_game_patch.h"
#include "ps3scale/iso9660.h"
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
static const ps3scale_ps2_patch_word_t fps_devil_summoner[]={{0x0011B560u,0x10000006u}};
static const ps3scale_ps2_patch_word_t fps_manhunt[]={{0x00548960u,0x08000001u}};
static const ps3scale_ps2_patch_word_t fps_pirates[]={{0x00384CD0u,0x10000004u}};
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

/* Additional source-pinned 16:9 widescreen candidates. */
static const ps3scale_ps2_patch_word_t ws_7sins[]={
 {0x00226ABCu,0x3C02402Au},{0x00226AC0u,0x3442AAABu},
 {0x00226AC4u,0x44820800u},{0x00226AE4u,0x080BF5C4u},
 {0x002FD710u,0x46000803u},{0x002FD714u,0x3C013F40u},
 {0x002FD718u,0x4481F000u},{0x002FD71Cu,0x461E0002u},
 {0x002FD720u,0x08089ABAu}};
static const ps3scale_ps2_patch_word_t ws_shadow_hearts[]={
 {0x0028D4F0u,0x3C043F40u},{0x00282394u,0x3C023F40u},
 {0x0039FE08u,0x3C023F40u},{0x003D5704u,0x3C023F40u},
 {0x003EEC44u,0x3C023F40u}};
static const ps3scale_ps2_patch_word_t ws_tmnt_battle_nexus[]={
 {0x00234710u,0x460D6302u},{0x00234714u,0x460D6B42u}};
static const ps3scale_ps2_patch_word_t ws_obscure[]={
 {0x00249790u,0x3C023F19u},{0x00249794u,0x34428000u},
 {0x00249798u,0x44820000u},{0x001C0794u,0x3C033F2Bu},
 {0x0030FE74u,0x24067100u},{0x0030FECCu,0x24058F00u}};
static const ps3scale_ps2_patch_word_t ws_armored_core3[]={
 {0x002E8C7Cu,0x3C013F40u},{0x002E8C88u,0x44810000u},
 {0x002E8C8Cu,0x4600C602u},{0x0026DBCCu,0x3C0143D6u}};
static const ps3scale_ps2_patch_word_t ws_bionicle[]={
 {0x0010C248u,0x3C014370u},{0x0010C088u,0x3C013FAAu}};
static const ps3scale_ps2_patch_word_t ws_splinter_cell_da[]={
 {0x001F483Cu,0x3C0D3F40u}};
static const ps3scale_ps2_patch_word_t ws_lapucelle[]={
 {0x00108994u,0x3C013F40u},{0x00108998u,0x44810000u},
 {0x001089A0u,0x4600C602u},{0x00136728u,0x3C033FC0u},
 {0x0013673Cu,0x3C023FC0u},{0x00136750u,0x3C023FC0u},
 {0x00136764u,0x3C063FC0u}};

/* Additional source-pinned 480p progressive-output candidates. */
static const ps3scale_ps2_patch_word_t p480_crash_titans_pal_m[]={
 {0x0057E904u,0x3C050000u},{0x0057E90Cu,0x3C060050u},
 {0x0057E914u,0x3C070001u}};
static const ps3scale_ps2_patch_word_t p480_moh_european_pal_m[]={
 {0x0044200Cu,0x24110000u},{0x00442010u,0x24120050u},
 {0x0044201Cu,0x24130001u}};
static const ps3scale_ps2_patch_word_t p480_spongebob_bfbb_pal[]={
 {0x0026435Cu,0x24050000u},{0x00264360u,0x24060050u},
 {0x002636A8u,0x24060050u},{0x002636ACu,0x24050000u},
 {0x00102254u,0x3C090010u}};
static const ps3scale_ps2_patch_word_t p480_star_wars_iii_pal[]={
 {0x00110704u,0x24110000u},{0x00110708u,0x24120050u},
 {0x00110714u,0x24130001u}};
static const ps3scale_ps2_patch_word_t p480_commandos[]={
 {0x00100DA0u,0x34050050u},{0x00100DA4u,0x24030002u},
 {0x00100DA8u,0x0000000Cu},{0x00100DACu,0x03E00008u}};
static const ps3scale_ps2_patch_word_t p480_punisher_pal_si[]={
 {0x001A0C14u,0x3C050000u},{0x001A0C1Cu,0x3C060050u},
 {0x001A0C24u,0x3C070001u}};
static const ps3scale_ps2_patch_word_t p480_driver_parallel_lines[]={
 {0x004B5790u,0x00000002u},{0x004B03C8u,0x00000003u},
 {0x00403588u,0x00000000u}};
static const ps3scale_ps2_patch_word_t p480_killswitch[]={
 {0x003513A4u,0x10620005u},{0x0029E8D4u,0x24050280u},
 {0x0029E8D8u,0x240601C0u}};
static const ps3scale_ps2_patch_word_t p480_space_chimps[]={
 {0x004213B4u,0x24110000u},{0x004213B8u,0x24120050u},
 {0x004213C4u,0x24130001u}};
static const ps3scale_ps2_patch_word_t p480_cold_fear[]={
 {0x0025FA24u,0x10620005u},{0x001449C8u,0x240301C0u}};

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
 PATCH("SLPM_662.46",0x67D76160u,"Shin Megami Tensei - Devil Summoner 1","SLPM-66246_67D76160.pnach",PS3SCALE_PS2_PATCH_FPS,fps_devil_summoner),
 PATCH("SLUS_208.27",0x38DEA143u,"Manhunt (NTSC-U)","SLUS-20827_38DEA143.pnach",PS3SCALE_PS2_PATCH_FPS,fps_manhunt),
 PATCH("SLUS_211.10",0x632E3976u,"Pirates of the Caribbean: The Legend of Jack Sparrow (NTSC-U)","SLUS-21110_632E3976.pnach",PS3SCALE_PS2_PATCH_FPS,fps_pirates),
 PATCH("SLUS_203.49",0x10B1F370u,"Scooby-Doo! Night of 100 Frights (NTSC-U)","SLUS-20349_10B1F370.pnach",PS3SCALE_PS2_PATCH_WIDESCREEN,ws_scooby),
 PATCH("SLUS_211.83",0xE1F17139u,"Teen Titans (NTSC-U)","SLUS-21183_E1F17139.pnach",PS3SCALE_PS2_PATCH_WIDESCREEN,ws_titans),
 PATCH("SLUS_214.37",0x86B490D1u,"Disney's Kim Possible - What's the Switch (NTSC-U)","SLUS-21437_86B490D1.pnach",PS3SCALE_PS2_PATCH_WIDESCREEN,ws_kim),
 PATCH("SLUS_206.96",0xFCC09959u,"Jimmy Neutron: Jet Fusion (NTSC-U)","SLUS-20696_FCC09959.pnach",PS3SCALE_PS2_PATCH_WIDESCREEN,ws_jimmy),
 PATCH("SLES_548.43",0xAEFD7D25u,"Crash of the Titans (PAL-R)","SLES-54843_AEFD7D25.pnach",PS3SCALE_PS2_PATCH_480P,p480_crash_titans),
 PATCH("SLES_549.75",0x278B7BEAu,"George of the Jungle (PAL-M)","SLES-54975_278B7BEA.pnach",PS3SCALE_PS2_PATCH_480P,p480_george_jungle),
 PATCH("SLES_513.93",0xD1ACD489u,"Syberia (PAL)","SLES-51393_D1ACD489.pnach",PS3SCALE_PS2_PATCH_480P,p480_syberia),
 /* 16:9 widescreen — pinned PCSX2 word-only sections. */
 PATCH("SLES_532.80",0x52DEB87Bu,"7 Sins (PAL-M)","SLES-53280_52DEB87B.pnach",PS3SCALE_PS2_PATCH_WIDESCREEN,ws_7sins),
 PATCH("SLUS_203.47",0x8BE3D7B2u,"Shadow Hearts (NTSC-U)","SLUS-20347_8BE3D7B2.pnach",PS3SCALE_PS2_PATCH_WIDESCREEN,ws_shadow_hearts),
 PATCH("SLES_530.17",0xC7375C85u,"Teenage Mutant Ninja Turtles 2: Battle Nexus (PAL-M5)","SLES-53017_C7375C85.pnach",PS3SCALE_PS2_PATCH_WIDESCREEN,ws_tmnt_battle_nexus),
 PATCH("SLUS_207.77",0xA06047D6u,"Obscure (NTSC-U)","SLUS-20777_A06047D6.pnach",PS3SCALE_PS2_PATCH_WIDESCREEN,ws_obscure),
 PATCH("SLES_513.99",0xDE5AF9A8u,"Armored Core 3 (PAL-E)","SLES-51399_DE5AF9A8.pnach",PS3SCALE_PS2_PATCH_WIDESCREEN,ws_armored_core3),
 PATCH("SLUS_208.18",0x8371B27Bu,"Bionicle (NTSC-U)","SLUS-20818_8371B27B.pnach",PS3SCALE_PS2_PATCH_WIDESCREEN,ws_bionicle),
 PATCH("SLES_538.27",0x6BD0E9C2u,"Tom Clancy's Splinter Cell: Double Agent (PAL-M3)","SLES-53827_6BD0E9C2.pnach",PS3SCALE_PS2_PATCH_WIDESCREEN,ws_splinter_cell_da),
 PATCH("SLES_529.78",0x474EC389u,"La Pucelle: Tactics (PAL)","SLES-52978_474EC389.pnach",PS3SCALE_PS2_PATCH_WIDESCREEN,ws_lapucelle),
 /* 480p progressive output — separate from FPS and widescreen. */
 PATCH("SLES_548.41",0xECA6BFC5u,"Crash of the Titans (PAL-M)","SLES-54841_ECA6BFC5.pnach",PS3SCALE_PS2_PATCH_480P,p480_crash_titans_pal_m),
 PATCH("SLES_533.32",0xBB70FFB9u,"Medal of Honor: European Assault (PAL-M)","SLES-53332_BB70FFB9.pnach",PS3SCALE_PS2_PATCH_480P,p480_moh_european_pal_m),
 PATCH("SLES_536.23",0x9ADA98B4u,"SpongeBob SquarePants: Battle for Bikini Bottom (PAL-F)","SLES-53623_9ADA98B4.pnach",PS3SCALE_PS2_PATCH_480P,p480_spongebob_bfbb_pal),
 PATCH("SLES_531.55",0x99AD19EEu,"Star Wars: Episode III - Revenge of the Sith (PAL-M3)","SLES-53155_99AD19EE.pnach",PS3SCALE_PS2_PATCH_480P,p480_star_wars_iii_pal),
 PATCH("SLES_527.68",0x5E1161DFu,"Commandos: Strike Force (PAL-M)","SLES-52768_5E1161DF.pnach",PS3SCALE_PS2_PATCH_480P,p480_commandos),
 PATCH("SLES_530.49",0x5AC5D875u,"The Punisher (PAL-S-I)","SLES-53049_5AC5D875.pnach",PS3SCALE_PS2_PATCH_480P,p480_punisher_pal_si),
 PATCH("SLES_540.27",0xD720770Du,"Driver: Parallel Lines (PAL-M)","SLES-54027_D720770D.pnach",PS3SCALE_PS2_PATCH_480P,p480_driver_parallel_lines),
 PATCH("SCES_521.24",0x91A65EAEu,"kill.switch (PAL-M)","SCES-52124_91A65EAE.pnach",PS3SCALE_PS2_PATCH_480P,p480_killswitch),
 PATCH("SLES_553.40",0xC423A2DCu,"Space Chimps (PAL-M)","SLES-55340_C423A2DC.pnach",PS3SCALE_PS2_PATCH_480P,p480_space_chimps),
 PATCH("SLES_531.58",0xE38A0AB6u,"Cold Fear (PAL-M)","SLES-53158_E38A0AB6.pnach",PS3SCALE_PS2_PATCH_480P,p480_cold_fear)

};
#ifdef PS3SCALE_HOST_TEST
size_t ps3scale_ps2_game_patch_catalogue_count(void)
{
 return sizeof(g_patches) / sizeof(g_patches[0]);
}

const ps3scale_ps2_game_patch_t *ps3scale_ps2_game_patch_catalogue_at(size_t index)
{
 if (index >= ps3scale_ps2_game_patch_catalogue_count())
  return NULL;
 return &g_patches[index];
}
#endif

const ps3scale_ps2_game_patch_t *ps3scale_ps2_game_patch_find(
 const char *serial,uint32_t hash,ps3scale_ps2_patch_kind_t kind)
{
 size_t i;
 if(!serial||!serial[0]||(kind!=PS3SCALE_PS2_PATCH_FPS&&kind!=PS3SCALE_PS2_PATCH_WIDESCREEN&&kind!=PS3SCALE_PS2_PATCH_480P))return NULL;
 for(i=0;i<sizeof(g_patches)/sizeof(g_patches[0]);++i)
  if(g_patches[i].kind==kind&&strcmp(serial,g_patches[i].serial)==0&&hash==g_patches[i].pcsx2_elf_hash)return &g_patches[i];
 return NULL;
}

int ps3scale_ps2_game_patch_elf_addresses_valid(
 const ps3scale_ps2_game_patch_t *patch,
 const unsigned char *elf_data, size_t elf_size)
{
 size_t i;
 if(!patch||!elf_data||elf_size==0||!patch->words||!patch->word_count)return 0;
 for(i=0;i<patch->word_count;++i){
  uint32_t original_word=0;
  if(ps3scale_ps2_elf_read_exec_word(elf_data,elf_size,
      patch->words[i].ee_address,&original_word)!=0)return 0;
 }
 return 1;
}
