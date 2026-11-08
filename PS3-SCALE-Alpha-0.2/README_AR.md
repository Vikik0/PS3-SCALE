# PS3 SCALE — Alpha 0.2

**الهدف:** إدارة مكتبة ألعاب PS3 وPS2، وعرض معلومات كل لعبة، وتجهيز أساس آمن لإدارة تحسينات خاصة بكل عنوان. المشروع ما زال Alpha؛ لا يطبّق حالياً تعديلات رسومية على ملفات الألعاب.

## آخر بناء ناجح

- GitHub Actions: [Run #115 — Success](https://github.com/Vikik0/PS3-SCALE/actions/runs/42491128350)
- Commit: \`16c70450996626dbcd12890231fcab2f0443b41f\`
- Artifact ID: \`13621377066\`
- Artifact ZIP SHA-256: \`eb8882e3f700226759ddd92793eadbd6cf025f19d278fae464463929e92b65aa\`
- الاختبارات: Host tests PASS، وبناء ELF وSELF وPKG النهائي PASS.
- SHA-256 للحزمة \`ps3scale.gnpdrm.pkg\`: \`e4edc12a2d0b81b43da11b7ebc2510f7a8768e149cb9a1044ef2faa4c508ab7b\`

التنزيل من Run #115 ثم Artifacts. كما جُهّز USB bundle يحتوي الحزمة والأغنية كملف منفصل.

## الميزات الموجودة

- فحص مجلدات ألعاب PS3 وPS2 على الهارد الداخلي وUSB والأقراص المركّبة.
- قراءة بيانات \`PARAM.SFO\` واستخراج Title ID أو PS2 serial عند توفره.
- فصل مكتبة PS3 عن PS2، والفرز الأبجدي، والبحث من لوحة PS3.
- شاشة Scan Games بعمود مستقل للقائمة وآخر للمعلومات والغلاف، مع خط أصغر لمنع التداخل.
- SELECT أو START يفتحان لوحة البحث؛ L1 يختار PS3 وR1 يختار PS2.
- القائمة الرئيسية تعرض أيقونات Vector وحركة خلفية بطيئة وخفيفة، ويمكن إيقاف الحركة من Settings.
- Settings: Music ON/OFF، مستوى الصوت، Home Animation ON/OFF، فحص ملف تحديث على USB، إعادة فحص المكتبة، استعادة الإعدادات، ونسخ الأغنية إلى HDD.
- الإعدادات تُحفظ في \`/dev_hdd0/game/PS3SCALE1/USRDIR/settings.cfg\`.
- بعد نسخ الأغنية إلى HDD يعيد التطبيق تشغيل محرك الصوت ليحوّل التشغيل إلى النسخة الداخلية.
- Companion Tools تعرض حالة اكتشاف Apollo Save Tool وArtemis PS3 وwebMAN MOD، مع تحديث الفحص من CROSS.
- الفحص Read-Only؛ لا يكتب أو يحذف أو يعيد تسمية ملفات الألعاب.

## الصوت

الأغنية التي زوّدتني بها تعمل تلقائياً عند فتح التطبيق إذا كانت موجودة في أحد المسارات المدعومة. ملف MP3 **ليس مدمجاً داخل PKG**؛ حزمة USB تضعه منفصلاً في \`PS3SCALE/theme.mp3\`.

لنسخه إلى الهارد الداخلي:
1. اترك USB موصولاً وشغّل PS3 SCALE.
2. افتح Settings.
3. اختر **COPY MUSIC TO INTERNAL HDD** واضغط CROSS.
4. عند نجاح النسخ، يعيد التطبيق تشغيل الصوت ويستخدم:
   \`/dev_hdd0/game/PS3SCALE1/USRDIR/theme.mp3\`
5. بعد التأكد من التشغيل يمكنك فصل USB.

## التحكم

- القائمة الرئيسية: UP/DOWN، ثم CROSS للفتح، CIRCLE للخروج.
- Scan Games: UP/DOWN للتنقل، L1/R1 للتبديل بين PS3/PS2، SELECT/START للبحث، SQUARE لإعادة الفحص، CROSS للتفاصيل، CIRCLE للرجوع.
- PS2 ISO: TRIANGLE لمعاينة CONFIG فقط.
- Settings: UP/DOWN للتنقل، CROSS للتبديل/التنفيذ، LEFT/RIGHT للصوت.
- Companion Tools: CROSS لإعادة فحص حالة الأدوات، CIRCLE للرجوع.

## أدوات مرافقة

Apollo Save Tool وArtemis PS3 وwebMAN MOD تطبيقات مستقلة. PS3 SCALE لا يحمّل إضافاتها ولا يغيّر بياناتها. صفحة Companion Tools تعرض اكتشافاً محلياً مبنياً على ملفات معروفة؛ عبارة NOT DETECTED لا تثبت دائماً أن الأداة غير موجودة إذا كان تثبيتها في مسار مختلف.

راجع \`docs/COMPANION_TOOLS.md\`.

## ما لم يُنفّذ بعد

- لا يوجد كتالوج أونلاين موثّق لتعديلات الرسوم ولا تنزيل/تطبيق تلقائي للـ patches.
- لا يوجد حالياً backup/rollback لملفات اللعبة لأن التطبيق لا يعدّلها.
- لا يوجد CFW runtime plugin لتطبيق patch عند تشغيل اللعبة.
- لا توجد رسالة داخل اللعبة "Graphics applied — Enjoy!" أو صفارة بعد تطبيق رسومي؛ هذه تحتاج runtime plugin يعمل مع اللعبة ويتحقق من عنوانها وإصدارها.
- لا يوجد Universal AI Upscaler أو Frame Generation. لا نعلن تحسناً في FPS أو الدقة دون patch خاص باللعبة واختبار فعلي.
- فحص تحديث USB يكتشف حزمة في المسار المحدد؛ لا يتحقق من توقيع إصدار ولا يثبتها تلقائياً.

## التثبيت

1. نزّل \`ps3scale.gnpdrm.pkg\` من Artifact في Run #115، أو استخدم USB bundle الجاهز.
2. انسخ PKG إلى USB FAT32.
3. من XMB افتح Game → Package Manager → Install Package Files → Standard.
4. اختر \`ps3scale.gnpdrm.pkg\` ثم شغّل PS3 SCALE من قائمة الألعاب.

لا تثبّت أي patch أو CONFIG على ملفات الألعاب في هذه المرحلة.

## جهاز الاختبار المستهدف

PS3 Slim CECH-2504A، CEX CFW 4.93، Cobra 8.5. Run #115 اجتاز CI، لكن يجب إعادة اختبار نسخة الواجهة الجديدة على الجهاز نفسه قبل وصفها بأنها مختبرة على الجهاز.

## البناء والاختبار

في بيئة PS3DEV / PSL1GHT / Tiny3D:

\`\`\`sh
export PS3DEV=/usr/local/ps3dev
export PSL1GHT=$PS3DEV
export PATH=$PATH:$PS3DEV/bin:$PS3DEV/ppu/bin:$PS3DEV/spu/bin
make clean
make pkg
\`\`\`

اختبارات الكمبيوتر:

\`\`\`sh
chmod +x scripts/test_host.sh
./scripts/test_host.sh
\`\`\`

## سياسة الأمان

لا تُطبّق تعديلات مستقبلية إلا بعد مطابقة Title ID والمنطقة وإصدار التحديث، والتحقق من المصدر وSHA-256، وإنشاء نسخة احتياطية قابلة للاستعادة، وتوفير Rollback، واختبارها على الجهاز المستهدف. لا تشارك IDPS أو PSID أو MAC أو الرقم التسلسلي.
