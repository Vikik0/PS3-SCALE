# تقرير المرحلة 81 — تدقيق مسارات Local Client مقابل Test Client

## السؤال

هل يمكن استخدام `SV_AddTestClient` أو `SV_SpawnTestClient` لإنشاء لاعب Campaign ثانٍ يتحكم به منفذ يد تحكم محلي؟

**القرار: لا.** في مراجع IW6 المتاحة لنسختي PS4 وXbox One، هذان المساران ينشئان Test/Bot client عبر عنوان شبكة من نوع `NA_BOT`، وليس Local Client مرتبطاً بوحدة تحكم ثانية.

## نتائج PS4 وXbox One المرجعية

تمت مقارنة pseudocode للمنصتين عند commit `513f0b6bdf0de32d16df3af914b26df01ee04ec9` في مستودع `lizardpeter/ghosts-decomp`.

| المسار | PS4 x64 | Xbox One x64 | النتيجة |
|---|---:|---:|---|
| `CL_DoesControllerIndexHaveALocalClientNum` | `0x1400AE7A0` | `0x1400AECE0` | يكتب `LOCAL_CLIENT_0` ويعيد true |
| `CL_ControllerIndexFromClientNum` | `0x1400AE790` | `0x1400AECD0` | يعيد `cl_controller_in_use` |
| `PlayerCmd_IsSplitscreenPlayer` في game_mp | `0x1401232B0` | `0x140123570` | يعيد false بعد فحص كيان اللاعب |
| `PlayerCmd_IsSplitscreenPlayerPrimary` | `0x1401232F0` | `0x1401235B0` | يقرأ حقول حالة العميل؛ لا ينشئ لاعباً |
| `SV_AddTestClient` | `0x1401F1AF0` | `0x1401F0390` | ينشئ اتصال Test/Bot |
| `SV_SpawnTestClient` | `0x1401F5120` | `0x1401F3890` | يرسل gamestate ثم يدخل Test Client إلى العالم |

### الدليل على أن Test Client ليس لاعباً محلياً

في نسختي PS4 وXbox One، مسار `SV_AddTestClient`:

1. يبحث عن slot متاح في `svs.clients`.
2. يكوّن اسماً من نوع `tcBot%d` أو اسماً مولداً للـBot/Test Client.
3. يبني أمر اتصال `connect bot%d`.
4. يضبط `netadr.type = NA_BOT` ويستخدم `svs.botport`.
5. يستدعي `SV_DirectConnect` ثم يضع علامة `testClient` على slot الناتج.

أما `GScr_AddTestClient` فيستدعي `SV_AddTestClient`، و`GScr_SpawnTestClient` يستدعي `SV_SpawnTestClient`. مسار Spawn يرسل Game State ويدخل العميل باستخدام `usercmd_s` محلي مهيأ صفرياً. هذه آلية Bot/Test Client، وليست ربط DualShock ثانٍ أو إنشاء Local Client ثانٍ.

### نتيجة Local Client في مراجع المنصتين

دالة `CL_DoesControllerIndexHaveALocalClientNum` في نسختي PS4 وXbox One تتجاهل قيمة controller index، وتضع `LOCAL_CLIENT_0` في خرجها ثم تعيد true. ودالة `CL_ControllerIndexFromClientNum` تعيد متغيراً عالمياً واحداً هو `cl_controller_in_use`.

هذا دليل على أن هذين البناءين المرجعيين لا يقدمان عبر هاتين الدالتين mapping مستقلاً إلى Local Client 1. لكنه **ليس دليلاً على سلوك نسخة PS3**؛ يجب استخراج تنفيذ PS3 الفعلي قبل اعتماد النتيجة على BLUS31270.

## قرار المشروع

- استبعاد `SV_AddTestClient` و`SV_SpawnTestClient` كحل لإنشاء اللاعب المحلي الثاني.
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
