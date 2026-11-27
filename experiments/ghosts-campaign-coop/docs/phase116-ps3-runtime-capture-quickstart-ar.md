# دليل سريع — التقاط أدلة PS3 الفعلية لـ Ghosts TU 1.16

> هذه الخطوة مطلوبة قبل ترحيل أي تفاعل Campaign إلى Player 2. اختبارات GitHub وGSC لا تستطيع محاكاة Local Client حقيقي أو كاميرا وHUD مستقلين.

## المتطلبات

- PS3 يعمل عليه Ghosts **BLUS31270 / TU 1.16**.
- webMAN MOD مع تفعيل PS3MAPI.
- وحدتا تحكم موصولتان قبل تشغيل اللعبة.
- كمبيوتر على نفس الشبكة.
- حزمة التطوير والالتقاط: نزّل أحدث artifact باسم ghosts-tu116-development-capture-kit من آخر GitHub Actions run ناجح على فرع المشروع. الحزمة تحتوي أدوات الالتقاط والتحقق والتقارير الحالية.

نفّذ الاختبارات في Offline فقط. لا تشغّل Pad Probe وDualShock Input Probe في الوقت نفسه.

## 1. اختبار وجود المنفذين في الوقت نفسه

نفّذ هذا الاختبار مرة داخل Campaign Offline ومرة داخل Multiplayer Offline.

1. انسخ `ghosts_campaign_pad_probe.fake.sprx` إلى `/dev_hdd0/tmp/`.
2. انسخ ملف الوضع المطابق إلى `/dev_hdd0/tmp/ghosts_probe_mode.txt`:
   - Campaign: `ghosts_probe_mode_CAMPAIGN.txt`
   - Multiplayer: `ghosts_probe_mode_MULTIPLAYER.txt`
3. ادخل الوضع المطلوب فعلياً، ثم خذ PID الحالي من:
   `http://PS3_IP/ps3mapi.ps3?PROCESS+GETCURRENTPID`
4. حمّل PRX داخل PID اللعبة:
   `http://PS3_IP/ps3mapi.ps3?MODULE+LOAD+PID_HERE+/dev_hdd0/tmp/ghosts_campaign_pad_probe.fake.sprx`
5. تحقق من وجود الوحدة باستخدام `MODULE+GETALLPRXID` ثم `GETNAME` أو `GETFILENAME`.
6. انتظر 30 ثانية، ثم انسخ السجل:
   - Campaign: `/dev_hdd0/tmp/ghosts_campaign_pad_probe.log`
   - Multiplayer: `/dev_hdd0/tmp/ghosts_multiplayer_pad_probe.log`
7. أزل الوحدة باستخدام `MODULE+UNLOAD+PID_HERE+PRX_ID` وتأكد من اختفائها من قائمة الوحدات.

الشرط المهم: يجب ظهور `port mask = 0x03` في عينة واحدة على الأقل. هذا يثبت أن طبقة Pad رأت المنفذين معاً فقط؛ ولا يثبت وجود لاعب ثانٍ داخل المحرك.

## 2. اختبار قراءة أزرار كل يد بشكل مستقل

كرر الاختبار لكل وضع، بعد التأكد من إزالة Pad Probe.

1. انسخ `ghosts_pad_input_probe.fake.sprx` إلى `/dev_hdd0/tmp/`.
2. اختر ملف الوضع نفسه في `ghosts_probe_mode.txt`.
3. حمّل الوحدة داخل PID اللعبة وتأكد من ظهورها في قائمة PRX.
4. خلال 30 ثانية، اضغط Cross على اليد الأولى وCircle على اليد الثانية في الوقت نفسه لعدة ثوانٍ، ثم بدّل الأزرار والعصي.
5. انسخ السجل:
   - Campaign: `/dev_hdd0/tmp/ghosts_campaign_pad_input_probe.log`
   - Multiplayer: `/dev_hdd0/tmp/ghosts_multiplayer_pad_input_probe.log`
6. أزل الوحدة وتأكد من اختفائها.

هذا الاختبار يثبت قراءة بيانات المنفذين فقط، ولا يثبت وجود Client أو Player Entity ثانٍ.

## 3. التقاط ذاكرة Campaign وMultiplayer

استخدم ملفات SELF المطابقة لتحديث TU 1.16، وليس ملفات القرص 1.00.

### Campaign

1. ادخل Campaign Offline فعلياً.
2. سجّل PID واسم العملية كما يرجعان من PS3MAPI.
3. انسخ `dump_campaign_tu116.bat` إلى `/dev_hdd0/tmp/`.
4. شغّله من webMAN:
   `http://PS3_IP/play.ps3/dev_hdd0/tmp/dump_campaign_tu116.bat`
5. انسخ الملفين الناتجين إلى مجلد `campaign_dumps` على الكمبيوتر:
   - `GHOSTS_TU116_CAMPAIGN_SEG0.bin`
   - `GHOSTS_TU116_CAMPAIGN_SEG1.bin`

### Multiplayer

كرر الخطوات نفسها داخل Multiplayer Offline باستخدام `dump_multiplayer_tu116.bat`، ثم انسخ:
- `GHOSTS_TU116_MULTIPLAYER_SEG0.bin`
- `GHOSTS_TU116_MULTIPLAYER_SEG1.bin`

لا تخلط ملفات الوضعين؛ Title ID واحد مستخدم لكليهما.

## 4. التحقق على الكمبيوتر

من مجلد المشروع، شغّل التحقق على كل وضع. عدّل المسارات بما يناسب جهازك:

```powershell
python tools/ps3mapi/verify_tu116_dump.py --mode campaign --self "C:\Ghosts\TU116\default.self" --dumps "C:\Ghosts\captures\campaign_dumps" --elf "C:\Ghosts\analysis\campaign_tu116.elf" --report "C:\Ghosts\analysis\campaign_report.json"

python tools/ps3mapi/verify_tu116_dump.py --mode multiplayer --self "C:\Ghosts\TU116\default_mp.self" --dumps "C:\Ghosts\captures\multiplayer_dumps" --elf "C:\Ghosts\analysis\multiplayer_tu116.elf" --report "C:\Ghosts\analysis\multiplayer_report.json"
```

بعدها قارن سجلات Pad:

```powershell
python tools/compare_pad_probe_logs.py "C:\Ghosts\logs\campaign_pad.log" "C:\Ghosts\logs\multiplayer_pad.log" --json --output "C:\Ghosts\analysis\pad_comparison.json"

python tools/compare_pad_input_probe_logs.py "C:\Ghosts\logs\campaign_input.log" "C:\Ghosts\logs\multiplayer_input.log" --json --output "C:\Ghosts\analysis\input_comparison.json"
```

ثم شغّل التحليل الموحد:

```powershell
python tools/ps3mapi/analyze_tu116_runtime_capture.py --campaign-self "C:\Ghosts\TU116\default.self" --campaign-dumps "C:\Ghosts\captures\campaign_dumps" --multiplayer-self "C:\Ghosts\TU116\default_mp.self" --multiplayer-dumps "C:\Ghosts\captures\multiplayer_dumps" --campaign-log "C:\Ghosts\logs\campaign_pad.log" --multiplayer-log "C:\Ghosts\logs\multiplayer_pad.log" --campaign-input-log "C:\Ghosts\logs\campaign_input.log" --multiplayer-input-log "C:\Ghosts\logs\multiplayer_input.log" --output-dir "C:\Ghosts\analysis\tu116_runtime"
```

سيُنشئ التقرير `capture_summary.json` ونتائج التحليل المشتقة. لا تعتبر أي عنوان ذاكرة دليلاً على لاعب ثانٍ قبل ربطه بملاحظتين مستقلتين على الأقل، مثل إدخال اليد الثانية وتغير Player State الخاص بها.

## 5. ما الذي ترسله للتحليل؟

أرسل ملفات النتائج التالية فقط كبداية:

- `campaign_report.json` و`multiplayer_report.json`.
- `pad_comparison.json` و`input_comparison.json`.
- `capture_summary.json`.
- نسختي metadata بعد تعبئة PID واسم العملية والوضع والتاريخ.

احتفظ بملفات SEG0/SEG1 وELF الخام محلياً. لا ترسل PSN credentials أو IDPS أو PSID أو ملفات ترخيص.

## معيار الانتقال إلى دمج المهمات

لن نغيّر أي FastFile أصلي حتى يثبت الالتقاط، على نفس TU 1.16:

1. وجود Player Entity ثانٍ مسجل كـ native split-screen player داخل Campaign، وليس Multiplayer فقط.
2. استقلال client/player state بين المنفذين.
3. وجود مسار إدخال مستقل للمنفذ الثاني.
4. وجود camera/refdef وviewport وHUD مستقلين.
5. استمرار الزوج بعد الموت والـcheckpoint وإعادة تحميل المهمة.

إذا لم يظهر Native Campaign Local Client ثانٍ، فالتعديلات على GSC وحدها لن تصنعه، وسنعود لتحليل engine initialization قبل أي حقن.
