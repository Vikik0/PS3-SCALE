# PS3 SCALE — Alpha 0.2

**هدف النسخة:** بناء أساس آمن لإدارة مكتبة ألعاب PS3 وPS2، تمهيداً لإضافة تحسينات رسومية خاصة بكل لعبة. جهاز الاختبار الأول هو CECH-2504A Slim / CEX CFW 4.93 / Cobra 8.5.

## الحالة الحالية

<details>
<summary>Build status</summary>

- GitHub Actions Run: #84 — **Success**
- Commit: `f5642796325333ee8c5af44d1295f76657998c48`
- Host-side tests: PASS
- PS3 ELF: built
- PS3 SELF: built
- Debug PKG: built
- Finalized NPDRM PKG: built and structurally validated
- Previous Alpha build launched successfully on the user's CECH-2504A / CFW 4.93 console. The vector-icon and startup-audio build has passed CI and still needs physical visual/audio verification.

</details>

ملف التثبيت النهائي هو `ps3scale.gnpdrm.pkg`، وليس نسخة `ps3scale.pkg` غير النهائية.

## ما الموجود فعلياً؟

- محلّل `PARAM.SFO` بلغة C مع فحوص حدود الذاكرة.
- ماسح مجلدات PS3 في `GAMES` و`GAMEZ`.
- قراءة الألعاب المثبتة من `/dev_hdd0/game/` حسب CATEGORY:
  - `HG`: عناوين PS3 المثبتة.
  - `2P`: PS2 Classics.
  - `2G`: تثبيت PS2 على الأنظمة التي تدعمه.
- اكتشاف ملفات ISO في `PS3ISO` و`PS2ISO`.
- استخراج PS2 serial من ISO9660 عبر `SYSTEM.CNF` عند توفره.
- استخراج PS3 Title ID كتلميح من أسماء ISO الشائعة.
- قراءة قرص Blu-ray المركّب مباشرة من `/dev_bdvd/PS3_GAME/PARAM.SFO`.
- مسح `/dev_hdd0` و`/dev_bdvd` و`/dev_usb000` إلى `/dev_usb007`.
- القائمة الرئيسية تحتوي أيقونات رسومية Vector مرسومة بـ Tiny3D: عدسة بحث لـ Scan Games، ترس لـ Settings، ورمز معلومات لـ About.\n- واجهة تعرض مكتبتين منفصلتين لـ PS3 وPS2 مع عدّاد لكل منهما، والتبديل بينهما من L1/R1.
- معاينة PS2 CONFIG للـ PS2 ISO باستخدام Triangle؛ لا تحفظ ولا تثبت أي ملف.
- الفحص Read-Only؛ لا يكتب أو يحذف أو يعيد تسمية ملفات الألعاب.

## ما الذي لم يُنفّذ بعد؟

- لا يوجد حتى الآن محرك تحسين رسومي عام أثناء اللعب.
- لا توجد بعد إدارة فعلية لتثبيت PS2 CONFIG أو PS3 patches.
- لا يوجد استخراج للأغلفة ICON0/PIC1 أو ملفات metadata الداخلية من PS3 ISO.
- تم تشغيل إصدار Alpha سابق على الجهاز؛ إصدار الأيقونات والصوت الجديد يحتاج إعادة تثبيت واختبار على الجهاز.

تطبيق GameOS عادي لا يبقى مسؤولاً عن رسم اللعبة بعد إغلاقه. لذلك يجب أن يكون تحسين PS3 حقيقياً عبر patch أو plugin خاص باللعبة ومطابقاً لإصدارها، أما PS2 فيمكن دراسة إعدادات `ps2_netemu CONFIG` الخاصة بكل لعبة. لن نعرض أزرار AI Upscale أو Frame Generation كأنها تعمل قبل وجود backend واختبار قابل للقياس.

## الصوت عند فتح التطبيق

تم دمج مشغل MP3 المتدفق، ويبدأ تلقائياً عند تشغيل PS3 SCALE ويستمر بين الصفحات ويعيد الأغنية من البداية عند نهايتها. من Settings:
- UP/DOWN: اختيار Music أو Volume.
- CROSS: تشغيل/كتم الموسيقى.
- LEFT/RIGHT: خفض/رفع الصوت.

ملف الأغنية لم يُضمّن داخل PKG بعد. انسخه مرة واحدة إلى:
`/dev_hdd0/game/PS3SCALE1/USRDIR/theme.mp3`

ويبحث التطبيق أيضاً عن الاسم الأصلي للملف في المجلد نفسه، ثم عن `PS3SCALE/theme.mp3` في أي منفذ USB من `/dev_usb000` إلى `/dev_usb007`. بعد وضعه في المسار، يعمل تلقائياً دون إنترنت.

## ملفات البناء

GitHub Actions artifact:
- [PS3-SCALE-Alpha-0.2-build — Run #84](https://github.com/Vikik0/PS3-SCALE/actions/runs/42485612038)
- Artifact ID: `13619530789` (افتح Run #84 ثم Artifacts لتنزيل الحزمة).
- يحتوي على `ps3scale.elf`, `ps3scale.self`, `ps3scale.pkg`, `ps3scale.gnpdrm.pkg`.

SHA-256 للحزمة النهائية:
```
cd7810f4ad88bf3d7d086a1ec8203eb847988b1abc4516af09e71f680a8249fb
```

Content ID:
```
UP0001-PS3SCALE1_00-0000000000000000
```

## التثبيت التجريبي على CFW

1. نزّل `ps3scale.gnpdrm.pkg` من GitHub Actions artifact.
2. انسخ الملف إلى USB بنظام FAT32.
3. من XMB افتح **Game → Package Manager → Install Package Files → Standard**.
4. اختر `ps3scale.gnpdrm.pkg`.
5. بعد التثبيت شغّل PS3 SCALE من قائمة الألعاب.

هذا تثبيت للتطبيق فقط. لا تثبّت أي game patch أو CONFIG في هذه المرحلة. بعد التشغيل نختبر الخروج إلى XMB، والفحص مرتين، وعدم وجود تكرار، وبقاء ملفات الألعاب دون تغيير.

## جهاز الاختبار

- Model: CECH-2504A (Slim)
- Firmware: 4.93
- CFW: CEX
- Cobra: 8.5
- المساحة الفارغة التي أبلغ عنها المستخدم: قرابة 10 GB

لا يحتاج المشروع إلى PSN. ولا نحتاج IDPS أو PSID أو MAC أو الرقم التسلسلي لتطويره؛ لا تشارك هذه المعرفات.

## بناء محلياً

المشروع يعتمد على PS3DEV / PSL1GHT وTiny3D وlibfont3d. في بيئة PS3 toolchain:

```sh
export PS3DEV=/usr/local/ps3dev
export PSL1GHT=$PS3DEV
export PATH=$PATH:$PS3DEV/bin:$PS3DEV/ppu/bin:$PS3DEV/spu/bin
make clean
make pkg
```

اختبار الكمبيوتر فقط:

```sh
chmod +x scripts/test_host.sh
./scripts/test_host.sh
```

## مسارات البحث

- PS3 folder games: `/dev_hdd0/GAMES`, `/dev_hdd0/GAMEZ`
- PS3 installed titles: `/dev_hdd0/game`
- PS3 ISO: `/dev_hdd0/PS3ISO`
- PS2 ISO: `/dev_hdd0/PS2ISO`
- USB: `/dev_usb000` إلى `/dev_usb007`

ملاحظة: PS2 ISO يحاول استخراج الرقم التسلسلي من `SYSTEM.CNF`. أما PS3 ISO فيُستخرج Title ID من اسم الملف فقط إذا كان مكتوباً فيه. قراءة بيانات PS3 ISO الداخلية واستخراج ICON0/PIC1 مرحلة لاحقة.

## التحكم في Alpha

- L1: فتح مكتبة PS3.
- R1: فتح مكتبة PS2.
- D-Pad Up/Down: اختيار لعبة.
- Square: إعادة الفحص.
- Triangle: معاينة PS2 CONFIG عند اختيار PS2 ISO (معاينة فقط).
- Circle: خروج.

## سياسة حماية الملفات

Alpha 0.2 لا تطبّق patches ولا تغيّر أي ملف لعبة. قبل تفعيل أي تعديل مستقبلي يجب:
1. مطابقة Title ID والمنطقة وإصدار التحديث.
2. التحقق من SHA-256 والحجم.
3. إنشاء نسخة احتياطية قابلة للاستعادة.
4. توفير Rollback.
5. عرض المصدر ومستوى الثقة والمشاكل المعروفة.
6. اختبار التعديل على الجهاز المستهدف قبل تصنيفه Hardware Verified.

راجع `docs/RESEARCH_NOTES.md`, `docs/GRAPHICS_ENGINE_REALITY.md`, `docs/PS2_ENGINE.md`, `docs/PATCH_TRUST_POLICY.md`، `docs/WEBMAN_MAPI_INTEGRATION.md`، `docs/UI_FLOW.md`، و`docs/GAME_ENHANCEMENT_WORKFLOW.md`.\n\nتم دمج محرك تشغيل MP3، لكن ملف الأغنية نفسه ما زال يحتاج نسخاً مرة واحدة إلى المسار الموضح أعلاه. التفاصيل في `docs/AUDIO_THEME.md` و`third_party/mpg123/README.md`.
