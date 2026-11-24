# Ghosts DualShock Input Probe v1 — قراءة أزرار المنفذين

هذه وحدة تشخيص منفصلة عن Pad Probe v4. هدفها اختبار أن عملية Ghosts تستطيع قراءة بيانات أزرار/عصي المنفذين 0 و1 بصورة مستقلة، وليس إنشاء Local Client أو لاعب ثانٍ.

## ما الذي تقرأه؟

- تعيد استخدام `ghc_local_input_poll_ps3` الموجود في PRX الرئيسي.
- يستدعي المحول `cellPadGetInfo2` ثم `cellPadGetData` للمنفذين 0 و1 المتصلين فقط.
- يسجل digital1/digital2، الأزرار المحوّلة، pressed/released، قيم العصي الخام والمطبّعة، وضغط L2/R2.
- يأخذ 30 عينة بفاصل ثانية واحدة.
- يسجل Campaign وMultiplayer في ملفين منفصلين حسب ملف الوضع اليدوي.

## ما الذي لا تفعله؟

- لا تستدعي `cellPadInit` أو `cellPadEnd`.
- لا تستدعي أي `cellPadSet*` أو دالة rumble.
- لا تكتب إلى ذاكرة اللعبة، ولا تستخدم hooks أو dvars.
- لا تنشئ Local Client أو Player Entity ولا تغير توجيه إدخال اللعبة.
- لا تعدّل حالة الأزرار؛ تستدعي واجهات القراءة فقط التي يستخدمها محول الإدخال الحالي.

**تنبيه تشغيلي:** هذا الاختبار يقرأ بيانات المنفذين من Thread منفصل أثناء تشغيل اللعبة. استخدمه في Offline فقط، وشغّل نسخة واحدة من أدوات Pad Probe في كل مرة. لا تشغّل v4 وInput Probe معاً. يجب التحقق من الاستقرار على جهاز PS3 قبل اعتماد القراءة كمسار إدخال دائم.

## البناء

من جذر المشروع:

~~~bash
make -C client/ps3/probes/pad_input_probe PS3DEV="$PS3DEV" PS3DK="$PS3DK"
~~~

ينتج:
- `build/ghosts_pad_input_probe.prx`
- `build/ghosts_pad_input_probe.fake.sprx`
- `build/ghosts_pad_input_probe.elf`

هذه ملفات تشخيص وليست mod أو PKG.

## التشغيل

1. انسخ `build/ghosts_pad_input_probe.fake.sprx` إلى `/dev_hdd0/tmp/ghosts_pad_input_probe.fake.sprx`.
2. صِل وحدتي التحكم قبل تشغيل اللعبة.
3. انسخ ملف الوضع المناسب إلى `/dev_hdd0/tmp/ghosts_probe_mode.txt`.
4. ادخل Campaign أو Multiplayer Offline فعلياً.
5. سجّل PID الحالي من:
   `http://PS3_IP/ps3mapi.ps3?PROCESS+GETCURRENTPID`
6. حمّل الوحدة داخل PID اللعبة:
   `http://PS3_IP/ps3mapi.ps3?MODULE+LOAD+PID_HERE+/dev_hdd0/tmp/ghosts_pad_input_probe.fake.sprx`
7. تحقق من وجود الوحدة عبر `MODULE+GETALLPRXID` ثم `GETNAME` أو `GETFILENAME`.
8. خلال 30 ثانية، أمسك Cross على يد التحكم الأولى وCircle على اليد الثانية في الوقت نفسه لعدة ثوانٍ، ثم بدّل الأزرار والعصي. لا تضغط زر PS ولا تفصل أي يد أثناء العينة.
9. انسخ السجل المناسب إلى الكمبيوتر، ثم أزل الوحدة عبر `MODULE+UNLOAD+PID_HERE+PRX_ID` وتحقق من اختفائها.

ملفات السجل:
- Campaign: `/dev_hdd0/tmp/ghosts_campaign_pad_input_probe.log`
- Multiplayer: `/dev_hdd0/tmp/ghosts_multiplayer_pad_input_probe.log`

إذا لم يتصل منفذ، أو فشل `cellPadGetData`، يسجل probe read_error ولا يخترع ضغطات أزرار. سطر MODE يصف ملف الوضع الذي اختاره المشغّل، ولا يكتشف الوضع تلقائياً.

## تحليل السجلين

~~~bash
python tools/compare_pad_input_probe_logs.py ghosts_campaign_pad_input_probe.log ghosts_multiplayer_pad_input_probe.log --json --output pad_input_comparison.json
~~~

النتيجة القوية تتطلب ظهور المنفذين معاً، وقراءة ناجحة لكليهما، ونشاط أزرار على كل منفذ، ووجود عينة واحدة على الأقل تختلف فيها حالة أزرار المنفذين أثناء اتصالهما معاً.

حتى نجاح الاختبار يثبت فقط أن طبقة `cellPadGetData` أعادت بيانات مستقلة من port 0 وport 1. لا يثبت وجود Local Client ثانٍ أو Player Entity أو كاميرا أو HUD مستقلة.
