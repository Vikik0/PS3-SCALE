# أدوات PS3MAPI — التقاط runtime من TU 1.16

هذه الأدوات تكمل تحليل ملفات SELF الذي تم في المرحلة السابعة. الهدف الحصول على **نسخة تحليلية من مقاطع الذاكرة التي حمّلها محرك PS3 فعلياً** من نسخة المستخدم، ثم مقارنتها بين Campaign وMultiplayer.

كل عمليات الالتقاط هنا قراءة فقط. لا يوجد patch أو كتابة لذاكرة اللعبة، ولا يتم فك SELF باستخدام مفاتيح خارجية.

## الملفات المطلوبة على جهاز الكمبيوتر

من مجلد تحديث BLUS31270 المثبت على الجهاز، احتفظ بنسختين منفصلتين:

- default.self الخاص بـCampaign.
- default_mp.self الخاص بـMultiplayer.

لا تستخدم ملفات القرص 1.00 بدلاً من ملفات TU 1.16.

## 1. التقاط Campaign

1. تأكد أن Ghosts يعمل وأنك داخل Campaign فعلياً، وليس القائمة أو Multiplayer.
2. فعّل PS3MAPI في إعدادات webMAN MOD.
3. احصل على PID الحالي واسم العملية وسجّلهما:
   - http://PS3_IP/ps3mapi.ps3?PROCESS+GETCURRENTPID
   - http://PS3_IP/ps3mapi.ps3?PROCESS+GETNAME+PID_HERE
   استبدل PID_HERE بالقيمة الفعلية، واحتفظ باسم العملية حرفياً مع وسم Campaign. هذا metadata تشخيصي فقط؛ لا نفترض أن الاسم يميز الوضعين تلقائياً.
4. انسخ dump_campaign_tu116.bat إلى /dev_hdd0/tmp/.
4. افتح المتصفح على الكمبيوتر واستخدم:
   http://PS3_IP/play.ps3/dev_hdd0/tmp/dump_campaign_tu116.bat
5. انتظر إشعار انتهاء الالتقاط.
7. انسخ الملفين التاليين إلى مجلد campaign_dumps على الكمبيوتر:
   - GHOSTS_TU116_CAMPAIGN_SEG0.bin
   - GHOSTS_TU116_CAMPAIGN_SEG1.bin

**مهم:** Campaign وMultiplayer يستخدمان Title ID نفسه؛ لذلك السكربت يدوي ولا يُثبت داخل wm_ingame. لا تشغّل سكربت Campaign وأنت داخل Multiplayer.

## 2. التقاط Multiplayer

1. ادخل إلى Multiplayer Offline مع بقاء نفس نسخة BLUS31270 وTU 1.16.
2. احصل على PID الحالي واسم العملية باستخدام PROCESS GETCURRENTPID وPROCESS GETNAME كما في Campaign، وسجّل الاسم حرفياً مع وسم Multiplayer. الاسم حقل تشخيصي وليس auto-detection.
3. انسخ dump_multiplayer_tu116.bat إلى /dev_hdd0/tmp/.
3. افتح:
   http://PS3_IP/play.ps3/dev_hdd0/tmp/dump_multiplayer_tu116.bat
4. انسخ:
   - GHOSTS_TU116_MULTIPLAYER_SEG0.bin
   - GHOSTS_TU116_MULTIPLAYER_SEG1.bin

لا تخلط ملفات Campaign مع Multiplayer. عناوين الذاكرة قد تتشابه، لكن أحجام المقاطع وبنية الملفين مختلفة.

## 3. التحقق وإعادة بناء ELF تحليلي

من مجلد experiments/ghosts-campaign-coop:

~~~powershell
python tools/ps3mapi/verify_tu116_dump.py --mode campaign --self "D:\Ghosts\TU116\default.self" --dumps "D:\Ghosts\captures\campaign_dumps" --elf "D:\Ghosts\analysis\campaign_tu116.elf" --report "D:\Ghosts\analysis\campaign_tu116_report.json"
~~~

ثم Multiplayer:

~~~powershell
python tools/ps3mapi/verify_tu116_dump.py --mode multiplayer --self "D:\Ghosts\TU116\default_mp.self" --dumps "D:\Ghosts\captures\multiplayer_dumps" --elf "D:\Ghosts\analysis\multiplayer_tu116.elf" --report "D:\Ghosts\analysis\multiplayer_tu116_report.json"
~~~

الأداة تتأكد من:
- تطابق entry point وPT_LOAD مع خريطة TU 1.16 المحفوظة.
- وجود كل المقاطع المطلوبة.
- حجم كل dump مطابق لتقريب webMAN إلى 64 KiB.
- المقاطع التنفيذية ليست فارغة أو مملوءة بأصفار.
- عدم الكتابة فوق SELF أو ملفات الإدخال.

تُنشئ الأداة ELF64 big-endian PowerPC64 للتحليل فقط. تحتفظ بـPT_LOAD والـentry point، ولا تعيد إنشاء section headers. منطقة BSS تُحفظ عبر p_memsz ولا تُملأ ببيانات مختلقة.

## 4. تحليل ELF الناتج

بعد نجاح التحقق، استخدم أداة المشروع:

~~~powershell
python tools/ps3_elf_inspect.py "D:\Ghosts\analysis\campaign_tu116.elf" --title-id BLUS31270 --output "D:\Ghosts\analysis\campaign_elf_report.json"
python tools/ps3_elf_inspect.py "D:\Ghosts\analysis\multiplayer_tu116.elf" --title-id BLUS31270 --output "D:\Ghosts\analysis\multiplayer_elf_report.json"
~~~

ثم شغّل ماسح النصوص:

~~~powershell
python tools/ps3mapi/scan_runtime_strings.py "D:\Ghosts\analysis\campaign_tu116.elf" --json "D:\Ghosts\analysis\campaign_strings.json"
python tools/ps3mapi/scan_runtime_strings.py "D:\Ghosts\analysis\multiplayer_tu116.elf" --json "D:\Ghosts\analysis\multiplayer_strings.json"
~~~

وجود نص مثل isSplitScreen أو localclient دليل بحث فقط، وليس إثباتاً أن الدالة قابلة للاستدعاء أو أن Campaign يدعم لاعباً ثانياً.

## حدود مهمة

- ملفات dump تحتوي مقاطع executable/data من عملية اللعبة؛ احتفظ بها محلياً ولا ترفعها إلى GitHub.
- لا ترسل PSN credentials أو IDPS أو PSID أو ملفات ترخيص.
- لا تستخدم عنواناً من Multiplayer كـCampaign hook قبل تحليل calling convention وTOC وسياق التنفيذ.
- ظهور Controller 2 في cellPadGetInfo2 لا يثبت وجود Local Client ثانٍ.
- لا نعدّل EBOOT أو ننتج PKG قبل إثبات مسار native local-client على نفس TU 1.16.

## مرجع صيغة PS3MAPI

صيغة webMAN MOD موثقة كالتالي:
getmem.ps3mapi?proc=<id>&addr=<start>&len=<length>&dump=<size>.
عند حذف proc يستخدم webMAN PID اللعبة الحالية أثناء اللعب. قيمة dump هي حجم البيانات بالبايت (أو MB عندما تكون القيمة صغيرة)، ويكتب webMAN المقاطع على دفعات 64 KiB. السكربتات أعلاه تعتمد هذا السلوك وتتحقق من حجم الملفات قبل إعادة البناء.

المراجع:
- https://github.com/aldostools/webMAN-MOD/wiki/Web-Commands
- https://github.com/aldostools/webMAN-MOD/blob/master/include/ps3mapi/ps3mapi.h
- https://github.com/aldostools/webMAN-MOD/blob/master/include/ps3mapi/debug_mem.h


## تحقق إضافي من اكتمال الحفظ

تم تشديد سكربتي الالتقاط: بعد نقل كل `mem_dump.bin` إلى اسم المقطع النهائي، ينفذ السكربت `abort if not exist` على الملف الناتج نفسه. لا يصل إلى ملف `DUMP_DONE` إلا بعد نجاح فحص وجود SEG0 وSEG1 معاً. إذا فشل أمر إعادة التسمية، يتوقف السكربت ولا يعلن اكتمال الالتقاط.

ملف `DUMP_DONE` هو علامة انتهاء للسكربت فقط، وليس إثباتاً على صحة المحتوى أو حجمه. يجب تمرير الملفات على `verify_tu116_dump.py`؛ فهو يتحقق من حجم 64 KiB padded، ومطابقة PT_LOAD، ومحتوى المقطع التنفيذي قبل إعادة البناء.

لإعادة الالتقاط بعد نجاح سابق، احذف ملف `GHOSTS_TU116_<MODE>_DUMP_DONE` من `/dev_hdd0` أولاً، ثم أعد تشغيل السكربت داخل الوضع الصحيح. لا تحذف ملفات المقاطع إلا إذا كنت تنوي إعادة التقاطها.
