# Phase 14 — Clockwork drill player routing patch

<badge>Source patch compiled; FastFile integration pending</badge>

## 1. مصدر العمل

استُخرج المصدر الأصلي من حزمة التحليل التي وفرها المستخدم، وليس من ملف افتراضي أو من إعادة كتابة يدوية:

- الملف: `maps__clockwork_interior.gsc`
- FastFile الأصلي: `clockwork.ff`
- SHA-256 للـFastFile: `a2ac70c7349047f2eded5f45a5e46ff8278689839fe1655106263520fe639783`
- SHA-256 للمصدر الأصلي: `7fc0f02476d5c9ebbe7c28e68abe762a26a976a28587228c5027d1a5c543e1f1`
- أصل سكربت المهمة في المنطقة المفكوكة: `0x462645`
- حجم ScriptFile الأصلي المسجل في تقرير الاستخراج: 5,721 بايت للجزء المضغوط من string/stack، و15,500 بايت bytecode.

لم يتم حفظ المصدر الأصلي أو السكربت المعدّل داخل المستودع؛ تبقى ملفات اللعبة ومشتقاتها ضمن ملفات العمل المحلية للمستخدم.

## 2. التعديل الذي تم على السكربت الحقيقي

### التقاط اللاعب الذي بدأ التفاعل

- أضيف `watch_for_drill_trigger` لانتظار Flag التفعيل قبل فتح الـTrigger وتخزين الـactivator الذي يرجعه `flag_wait`.
- أصبح `handle_drill_spot` يحدد اللاعب الفعلي الذي فعّل نقطة الحفر.
- إذا كان اللاعب غير الأساسي، لا يُقبل إلا إذا طابق `level.playengineeffects` وكان native secondary split-screen player.
- يمرر `handle_drill_spot` اللاعب إلى `handle_drilling` بدلاً من الاعتماد على `level.player`.
- إذا لم يرجع الـTrigger كيان لاعب، يبقى fallback إلى Logan للحفاظ على مسار اللعب الفردي.

### ملكية اللاعب وحالته

- أصبحت `handle_drilling` تستقبل اللاعب وتقرأ ADS/Attack والسلاح والاهتزاز والصوت من الكيان نفسه.
- أصبحت `handle_misc_drill_details` و`watch_for_air_drop_death` تعملان لكل لاعب.
- نُقلت حالة التصويب على الحليف من متغير level مشترك إلى `player.ghc_aiming_at_ally`.
- حُفظت ذخيرة Frag وFlash لكل لاعب في حقول مستقلة، وأصبحت دالة الاستعادة تستقبل اللاعب.
- أضيف مدير واحد لـ`ammoCounterHide` يفحص سلاح اللاعب الأساسي والثانوي حتى لا تتعارض كتابات threadين على dvar واحد.

### تجهيز اللاعب الثاني

- أضيف مراقب native pair يراقب `level.player` و`level.playengineeffects` ويبدأ مساعدات اللاعب عند ظهور الكيان.
- يجهز اللاعب الثاني بنفس أسلحة Clockwork الموجودة في `setup_combat`: GM6 وCZ805، إضافة إلى Frag وFlash.
- تمنح `switchmsg` أداة `drill_press` للاعب الثاني مع إبقاء مشهد التقاط Logan الأصلي كما هو.
- تزال أداة `drill_press` من اللاعب الثاني بعد نهاية جزء الخزنة.

### الموت والانقطاع

- عند موت اللاعب أو اختفاء كيان اللاعب أثناء الحفر، يوقف التعديل مؤثرات وصوت الحفر، ويفك ربط اللاعب إن بقي الكيان موجوداً، ويمسح Flags التفاعل.
- يعيد فتح Trigger نقطة الحفر وينتظر Activator جديداً بدلاً من ترك المحاولة السابقة مالكة للتفاعل.
- لا يعتبر الموت أو الانقطاع إكمالاً للهدف.

## 3. نتيجة الترجمة

<badge color="success">GSC Tool compile passed</badge>

| العنصر | القيمة |
|---|---|
| GSC Tool | 1.6.2.364 |
| المصدر المعدّل | 100,159 bytes |
| SHA-256 للمصدر المعدّل | `6d686c799c8a441b69c4109c0c6689e140cc46033d2b645f4cd2760a61066e0c` |
| GSCBIN الناتج | 22,895 bytes |
| SHA-256 للـGSCBIN | `e6d5f6c291a6f75b07c36de80fed8eb98f5deb475bc75a556948790cecf88dc5` |
| Decompile round-trip | نجح |
| SHA-256 لملف decompile الناتج | `5626c749ab5608a3879b8d69f51a44b51ae306ee1fa4008e47bde3b3f8bb165b` |

نجاح الترجمة يثبت أن GSC Tool قبل صياغة السكربت وأنتج GSCBIN قابل لفك الترجمة. لا يثبت أن Retail Campaign VM نفّذه أو أن التفاعل يعمل على PS3.

## 4. ما لم يتم بعد

1. تأكيد أن MapEnts لنقطتي `drill_spot1` و`drill_spot2` يستخدم Trigger callback الذي يمرر activator إلى `flag_set`. يوجد fallback إلى Logan إذا لم يصل activator.
2. بناء أداة آمنة لاستبدال ScriptFile داخل `clockwork.ff` مع المحافظة على Asset Pool والمؤشرات وبنية الكتل والـopaque trailers والذيل غير المحلل.
3. التحقق من أن FastFile المعدّل يفتح في اللعبة/RPCS3 دون تلف.
4. إثبات أن المحرك أنشأ native second Campaign Local Client فعلاً، وأن Controller 2 والكاميرا والـHUD مستقلة.
5. اختبار المهمة على جهاز PS3 CEX TU 1.16، ثم إنشاء PKG بعد نجاح الاختبار.

## 5. قرار السلامة

لا يتم استبدال `clockwork.ff` الأصلي ولا إنشاء PKG قبل أن ينجح اختبار إعادة بناء FastFile ومقارنة جميع المؤشرات والـhashes. لا يستخدم هذا التعديل أي عنوان ذاكرة تخميني ولا ينشئ شخصية ثانية وهمية.
