# تقرير المرحلة 81 — تدقيق مسارات Local Client مقابل Test Client

## السؤال

هل يمكن استخدام `SV_AddTestClient` أو `SV_SpawnTestClient` لإنشاء لاعب Campaign ثانٍ يتحكم به منفذ يد تحكم محلي؟

**القرار: لا.** في مراجع IW6 المتاحة لنسختي PS4 وXbox One، هذان المساران ينشئان Test/Bot client عبر عنوان شبكة من نوع `NA_BOT`، وليس Local Client مرتبطاً بوحدة تحكم ثانية.

## تصحيح مصدر المراجع وحدودها

مستودع `lizardpeter/ghosts-decomp` يعرّف `iw6_ds_ps4` و`iw6_ds_xb3` في README على أنها **Windows x64 dedicated-server reference builds** مع PDB وMAP، مرتبطة بمضيفات PS4 وXbox One. هذه ليست ملفات retail console client. لذلك لا يجوز استخدام دوال `client/splitscreen.cpp` أو `PlayerCmd_IsSplitscreenPlayer` في هذه البنايات لاستنتاج عدد Local Clients أو سلوك وحدات التحكم في عميل PS3/PS4/Xbox One.

الاستنتاج الذي نحتفظ به من هذه المراجع هو فقط سلوك مسار الخادم `SV_AddTestClient` و`SV_SpawnTestClient`: فهو ينشئ Test/Bot connection من نوع `NA_BOT`، وليس تسجيل DualShock محلي ثانٍ. تمت مقارنة pseudocode عند commit `513f0b6bdf0de32d16df3af914b26df01ee04ec9`.

| المسار | PS4 x64 | Xbox One x64 | النتيجة |
|---|---:|---:|---|
| `CL_DoesControllerIndexHaveALocalClientNum` (server reference only) | `0x1400AE7A0` | `0x1400AECE0` | يكتب `LOCAL_CLIENT_0` ويعيد true؛ لا نستنتج منه سلوك عميل retail |
| `CL_ControllerIndexFromClientNum` (server reference only) | `0x1400AE790` | `0x1400AECD0` | يعيد `cl_controller_in_use`؛ لا نستنتج منه ربط يد التحكم في PS3 |
| `PlayerCmd_IsSplitscreenPlayer` في game_mp server reference | `0x1401232B0` | `0x140123570` | يعيد false في هذا البناء الخادمي فقط |
| `PlayerCmd_IsSplitscreenPlayerPrimary` (server reference only) | `0x1401232F0` | `0x1401235B0` | يقرأ حقول حالة العميل؛ لا ينشئ لاعباً |
| `SV_AddTestClient` | `0x1401F1AF0` | `0x1401F0390` | ينشئ اتصال Test/Bot من جهة الخادم |
| `SV_SpawnTestClient` | `0x1401F5120` | `0x1401F3890` | يرسل gamestate ثم يدخل Test Client إلى العالم |

### الدليل على أن Test Client ليس لاعباً محلياً

في نسختي PS4 وXbox One، مسار `SV_AddTestClient`:

1. يبحث عن slot متاح في `svs.clients`.
2. يكوّن اسماً من نوع `tcBot%d` أو اسماً مولداً للـBot/Test Client.
3. يبني أمر اتصال `connect bot%d`.
4. يضبط `netadr.type = NA_BOT` ويستخدم `svs.botport`.
5. يستدعي `SV_DirectConnect` ثم يضع علامة `testClient` على slot الناتج.

أما `GScr_AddTestClient` فيستدعي `SV_AddTestClient`، و`GScr_SpawnTestClient` يستدعي `SV_SpawnTestClient`. مسار Spawn يرسل Game State ويدخل العميل باستخدام `usercmd_s` محلي مهيأ صفرياً. هذه آلية Bot/Test Client، وليست ربط DualShock ثانٍ أو إنشاء Local Client ثانٍ.

### دوال Local Client وSplit-screen في ملفات الخادم

تظهر في ملفات dedicated-server pseudocode دوال تحمل أسماء Local Client وSplit-screen، لكن هذا لا يجعلها دوال عميل retail. القيم التي تعيدها (`LOCAL_CLIENT_0` أو false) قد تكون جزءاً من بيئة الخادم أو stubs خاصة بها. لذلك تم تسجيلها للرجوع فقط، ولا نستخدمها لاستنتاج أن PS3 أو PS4 retail يملك Local Client واحداً أو أن `issplitscreenplayer()` يعيد false على الجهاز.

## قرار المشروع

- استبعاد `SV_AddTestClient` و`SV_SpawnTestClient` كحل لإنشاء اللاعب المحلي الثاني؛ الدليل هنا يخص مسار الخادم في مراجع Windows x64.
- إبقاء GSC Campaign bridge على مبدأ إعادة استخدام زوج اللاعبين الذي أنشأه محرك اللعبة أصلاً، والتحقق من هوية primary/secondary قبل ربط aliases.
- أضيف اختبار يمنع إدخال `spawnTestClient` أو `addTestClient` أو `addBot` إلى local-session bridge مستقبلاً.
- عدم نقل أي عنوان PS4/Xbox x64 إلى PRX الخاص بـPS3؛ اختلاف المعمارية والعناوين يمنع ذلك.

## المراجع العامة

- [PS4 splitscreen pseudocode](https://github.com/lizardpeter/ghosts-decomp/blob/513f0b6bdf0de32d16df3af914b26df01ee04ec9/analysis/iw6_ds_ps4/pseudocode/client/splitscreen.cpp.c)
- [Xbox One splitscreen pseudocode](https://github.com/lizardpeter/ghosts-decomp/blob/513f0b6bdf0de32d16df3af914b26df01ee04ec9/analysis/iw6_ds_xb3/pseudocode/client/splitscreen.cpp.c)
- [PS4 server client pseudocode](https://github.com/lizardpeter/ghosts-decomp/blob/513f0b6bdf0de32d16df3af914b26df01ee04ec9/analysis/iw6_ds_ps4/pseudocode/server_mp/sv_client_mp.cpp.c)
- [Xbox One server client pseudocode](https://github.com/lizardpeter/ghosts-decomp/blob/513f0b6bdf0de32d16df3af914b26df01ee04ec9/analysis/iw6_ds_xb3/pseudocode/server_mp/sv_client_mp.cpp.c)
- [PS4 multiplayer script commands](https://github.com/lizardpeter/ghosts-decomp/blob/513f0b6bdf0de32d16df3af914b26df01ee04ec9/analysis/iw6_ds_ps4/pseudocode/game_mp/g_client_script_cmd_mp.cpp.c)
- [Xbox One multiplayer script commands](https://github.com/lizardpeter/ghosts-decomp/blob/513f0b6bdf0de32d16df3af914b26df01ee04ec9/analysis/iw6_ds_xb3/pseudocode/game_mp/g_client_script_cmd_mp.cpp.c)
- [PS4 GSC server wrappers](https://github.com/lizardpeter/ghosts-decomp/blob/513f0b6bdf0de32d16df3af914b26df01ee04ec9/analysis/iw6_ds_ps4/pseudocode/game_mp/g_scr_main_mp.cpp.c)
- [Xbox One GSC server wrappers](https://github.com/lizardpeter/ghosts-decomp/blob/513f0b6bdf0de32d16df3af914b26df01ee04ec9/analysis/iw6_ds_xb3/pseudocode/game_mp/g_scr_main_mp.cpp.c)

## ما لا يزال مطلوباً على PS3

لا يمكن اعتماد هذه الدوال أو عناوينها كـPS3 evidence. المطلوب بعد استلام Campaign/Multiplayer SEG0/SEG1 هو تشغيل Ghidra PPU على runtime ELF الفعلي ومقارنة نظائر:
`CL_DoesControllerIndexHaveALocalClientNum`,
`CL_ControllerIndexFromClientNum`,
`SV_AddTestClient`,
`SV_SpawnTestClient`,
`SV_ClientThink`,
و`PlayerCmd_IsSplitscreenPlayer`.

**الحالة:** تدقيق مرجعي مكتمل، لكن لا يوجد بعد إثبات أن PS3 ينشئ Local Client 1 داخل Campaign أو أن Player 2 قابل للعب.
