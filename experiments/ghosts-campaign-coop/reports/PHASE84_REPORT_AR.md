# تقرير المرحلة 84 — ملكية أحداث المهمات ومخاطر تكرارها

## الهدف

المرحلة 83 حسّنت تدقيق بيانات يدَي التحكم. في هذه المرحلة رجعت إلى أرشيف المهمات الذي أرسله المستخدم، وراجعت الدوال التسع الأعلى أولوية في Black Ice وCarrier وCornered على مستوى الحدث الكامل، لا على عدد مراجع `level.player` فقط.

الهدف هو منع تكرار أحداث المهمة أو نسخ حالة عالمية إلى لاعب واحد عند إضافة لاعب Campaign ثانٍ.

## مصدر البيانات

تم استخدام أرشيف `Ghosts_Campaign_Missions_Phase31.zip` الذي يحتوي ملفات GSC المفكوكة من:

| المهمة | FastFile | الحجم | GSC |
|---|---|---:|---:|
| Black Ice | `black_ice.ff` | 91,355,961 bytes | 76 |
| Carrier | `carrier.ff` | 87,088,544 bytes | 118 |
| Cornered | `cornered.ff` | 93,933,243 bytes | 55 |
| **المجموع** | | | **249** |

أداة الاستخراج: GSC Tool 1.6.2.364. اجتازت السكربتات الـ249 فك الترجمة وإعادة التجميع. هذا لا يثبت التطابق الثنائي أو التشغيل داخل اللعبة، ولم يتم تعديل أي FastFile في هذه المرحلة.

## مصفوفة الدوال الحرجة

الأرقام التالية محسوبة من أجسام الدوال نفسها بعد تجاهل التعليقات والنصوص الثابتة. "Flags" تعني استدعاءات واجهات mission flag، و"Level events" تعني `level notify/waittill/endon`.

| المهمة / الدالة | مراجع اللاعب | Flags | Dvars | Rigs / Links | آثار مشتركة إضافية |
|---|---:|---:|---:|---:|---|
| Black Ice — `player_fail_finale_instakill` | 26 | 1 | 5 | 1 / 1 | 3 earthquakes، و`missionfailed()` |
| Black Ice — `ascend_logic` | 22 | 4 | 0 | 3 / 1 | 5 level events، حالة صعود/حبال/حلفاء مشتركة |
| Black Ice — `event_player_turns_key` | 17 | 1 | 0 | 1 / 1 | 3 level events، تفاعل Console واحد |
| Carrier — `knockback_hesh` | 51 | 3 | 0 | 2 / 4 | 5 slow-motion calls، autosave واحد |
| Carrier — `cut_the_rope` | 22 | 0 | 0 | 1 / 1 | `self` هو rope trigger، و`active_rope` على اللاعب |
| Carrier — `victory_choppers_land` | 20 | 1 | 0 | 1 / 2 | autosave واحد وlevel event واحد |
| Cornered — `gun_down_trigger_internal` | 78 | 2 | 1 | 0 / 0 | 3 كتابات إلى `level.has_hidden_weapon_equipped` |
| Cornered — `fall_player` | 47 | 16 | 3 | 2 / 4 | mission failure، autosave، 3 world-physics calls |
| Cornered — `upload_virus_enter` | 44 | 2 | 4 | 0 / 0 | 3 كتابات إلى `level.allow_fire` |

### Black Ice

- `player_fail_finale_instakill`: تسلسل فشل نهائي واحد؛ العلم `flag_player_dying_on_rig` يمنع إعادة الدخول، ثم تُشغّل دالة `missionfailed()`. يجب ألا ينشأ تسلسل فشل ثانٍ للاعب الثاني. يمكن فصل overlay والكاميرا والـrumble وربط اللاعب، لكن فشل المهمة والـdvars واهتزاز العالم تبقى مرة واحدة.
- `ascend_logic`: المنسق يدير الحبال والحلفاء والـlauncher والـflags المشتركة، بينما يقرأ ضغط اللاعب على Attack ويستخدم حقولاً مثل `level.player.waitill_damage_or_death`. لا يمكن نسخه للاعب الثاني؛ يلزم منسق واحد مع handler منفصل للكاميرا والـrig والإدخال لكل لاعب، على نفس timeline.
- `event_player_turns_key`: console واحد وعلم mission واحد. يجب أن يملك لاعب واحد التفاعل، مع تمرير كيان اللاعب صراحةً إلى دالة الأنيميشن/التحكم عند إعادة الهيكلة.

### Carrier

- `knockback_hesh`: تسلسل سقوط/انزلاق Sparrow يغيّر slow motion ويكتب flags ويطلب autosave. يبقى حدثاً واحداً. يمكن إضافة actor rig متزامن للاعب الثاني فقط بعد وجود Local Client فعلي.
- `cut_the_rope`: الدالة تعتمد على `self` بوصفه rope trigger وعلى `var_0` ككيان التفاعل، وتكتب `level.player.active_rope`. يجب إبقاء ملكية rope على trigger نفسه مع قفل يمنع لاعبين من قص الحبل نفسه، وتخزين حالة اللاعب على كيان اللاعب المختار.
- `victory_choppers_land`: مشهد فوز/هبوط ينتهي بـautosave؛ المنسق يُنفّذ مرة واحدة. إن وُجد لاعب ثانٍ، يضاف له rig مستقل على نفس المشهد، لا نسخة ثانية من autosave أو نهاية المهمة.

### Cornered

- `gun_down_trigger_internal`: حفظ السلاح والذخيرة يتم على حقول اللاعب (`equipped_weapon`, `equipped_hide_weapon`, `unearned_icon_col` وغيرها)، لكن `level.has_hidden_weapon_equipped` و`ammoCounterHide` مشتركان. لذلك لا يجوز تشغيل نسختين متزامنتين؛ حالة السلاح لكل لاعب، أما قفل الحدث وواجهة HUD المشتركة فتحتاج إلى منسق واحد.
- `fall_player`: أكثر الدوال حساسية؛ تجمع حركة اللاعب والكاميرا والـrumble مع `level.fall_arms_and_legs` و`level.fall_path_rig` وflags والـautosave وتغيير جاذبية العالم والانفجارات و`missionfailed()`. القرار: فصل World-collapse coordinator واحد عن handlers خاصة بالكاميرا والتحكم والـrig لكل لاعب. تبقى الجاذبية والـphysics والـcheckpoint والفشل مرة واحدة.
- `upload_virus_enter`: الجهاز و`virus_dynamic_dof` والـupload flags مشتركة، بينما السلاح وthermal-vision وviewmodel تخص اللاعب. كتابات `level.allow_fire` وdvars قد تؤثر على P2 أيضاً؛ يجب حسم ملكية هذه الحالة قبل السماح بتفاعل متزامن.

## أنماط تعدد اللاعبين الموجودة فعلياً في السكربتات

1. **Carrier — `monitorscavengerpickup`**: يمر على `level.playerpiggyback` ست مرات، ينشئ HUD لكل لاعب، ويضبط `cg_playerFovScale0` و`cg_playerFovScale1` بحسب ترتيب اللاعبين. لكنه يغيّر `sm_sunShadowCenter` كـdvar مشترك اعتماداً على اللاعب الذي يصوّب؛ هذا مثال واضح على مزج حالة لكل لاعب مع إعداد عالمي.
2. **Carrier — `get_different_favoriteenemy`**: يقارن `self.favoriteenemy` مع `level.playengineeffects` ثم يرجع اللاعب الآخر من `level.playerpiggyback`. هذا مسار اختيار لاعب موجود، وليس إنشاء لاعب أو Local Client جديد.
3. **Cornered — `courtyard_rorke` و`bar_enemies`**: بعض تفاعلات trigger ما زالت تستخدم `level.player`، ثم تُطبّق stealth/visibility على جميع عناصر `level.playerpiggyback`. هذا نمط مختلط يجب الحفاظ عليه بدلاً من استبداله بالكامل.
4. **Black Ice — `player_saw_kill`**: يوجد فحص `issplitscreen()`، لكن `player_scuba()` يعمل فقط عندما `self == level.player` في Split Screen؛ أما صوت التنفس فيُشغّل بعد الشرط. هذا دعم جزئي وليس معالجة كاملة للاعبين.

**قاعدة مهمة:** `level.playerpiggyback` قائمة كيانات لاعبين موجودة، و`level.playengineeffects` مرجع ثانوي منفصل حسب تحليل الـcommon loader. لا تنشئ هذه المتغيرات Local Client ثانياً ولا تربط Controller 2.

## القرار الهندسي

- لا استبدال شامل لـ`level.player`.
- نمط التنفيذ المستقبلي: **منسق حدث واحد للمهمة + handlers مستقلة لكل لاعب**.
- حالة اللاعب التي يمكن فصلها: السلاح والذخيرة، الحركة، الكاميرا، viewmodel، rumble، HUD المحلي والـactor rig.
- حالة المهمة التي تبقى مشتركة: flags، checkpoints، autosave، mission failure، slow motion، world physics، الأجهزة، AI، objective triggers، والأنيميشن المشترك.
- تفاعلات rope وconsole وweapon-hide وvirus-upload تحتاج single-owner lock.
- يبقى Logan هو Player 1، وHesh NPC؛ لا يتم تحويله إلى Player 2 ولا إنشاء نسخة Hesh ثانية.

## تحديث أداة التدقيق

تم توسيع `gsc_player_state_ownership_audit.py` لتسجيل:
- كتابات حقول اللاعب مقابل كتابات حقول `level`.
- mission failure وautosave وslow motion وworld physics.
- level-wide notify/waittill/endon.
- عدد animation rigs.
- `event_risk_flags` و`event_ownership_pattern` لكل دالة.

هذا تصنيف ثابت للمراجعة؛ لا يقرر تلقائياً أن أي حقل عالمي أو محلي يمكن نسخه.

## CI والحدود

- GitHub Actions run [#1470](https://github.com/Vikik0/PS3-SCALE/actions/runs/45505931126): **6/6 jobs successful**.
- **469 اختباراً** ناجحاً، بما فيها اختبارات تدقيق side effects الجديدة.
- PS3 CEX PRX bootstrap وبناء Pad Probe وDualShock Input Probe و`prx-gen check`: PASS.
- لم يتم تعديل FastFiles أو تشغيل هذه المهمات على جهاز PS3 في هذه المرحلة.
- لا يوجد ادعاء بوجود Native Local Client 1 أو Campaign Player 2 قابل للعب.

## البوابة التالية

الخطوة التي تفتح التنفيذ الفعلي ليست تعديل GSC إضافياً؛ بل إثبات إنشاء وربط Local Client 1 على PS3 من خلال PPU runtime dumps المطابقة، مع سجلات إدخال Campaign وMultiplayer. بعد ذلك فقط يمكن تحويل هذا التقرير إلى patch plan لكل حدث دون كسر حالة المهمة المشتركة.
