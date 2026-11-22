# Ghosts Pad Probe v4 — أداة تشخيص قراءة فقط

هذه وحدة PRX مستقلة لاختبار هل طبقة Pad في عملية اللعبة ترى المنفذين 0 و1 في الوقت نفسه.

## ما الذي تقرأه؟

- تستدعي cellPadGetInfo2 فقط.
- تسجل port_status وdevice_capability وdevice_type لكل منفذ.
- تأخذ 30 عينة بفاصل ثانية واحدة.
- تسجل العينة عند تغير port mask أو كل خمس عينات.
- تكتب ملف Campaign أو Multiplayer منفصلاً حسب ملف الوضع.

## ما الذي لا تفعله؟

- لا تستدعي cellPadInit أو cellPadEnd أو cellPadGetData.
- لا تقرأ أزرار التحكم أو تستهلك input reports.
- لا تكتب إلى ذاكرة اللعبة، ولا تستخدم hooks أو dvars.
- لا تنشئ Local Client أو Player Entity.

## البناء

من جذر المشروع:

~~~bash
make -C client/ps3/probes/pad_probe PS3DEV="$PS3DEV" PS3DK="$PS3DK"
~~~

الـCI يبني الوحدة بواسطة PS3DK v0.28.0 ويتحقق من PRX باستخدام prx-gen check. الناتج:
- build/ghosts_campaign_pad_probe.prx
- build/ghosts_campaign_pad_probe.fake.sprx
- build/ghosts_campaign_pad_probe.elf

هذه ملفات تشخيص وليست mod أو PKG.

## التشغيل على الجهاز

1. انسخ build/ghosts_campaign_pad_probe.fake.sprx إلى:
   /dev_hdd0/tmp/ghosts_campaign_pad_probe.fake.sprx
2. صِل وحدتي التحكم قبل تشغيل اللعبة.
3. انسخ ملف الوضع المناسب إلى /dev_hdd0/tmp/ghosts_probe_mode.txt.
4. ادخل Campaign أو Multiplayer Offline فعلياً.
5. افتح رابط PID الحالي من متصفح الكمبيوتر:
   http://PS3_IP/ps3mapi.ps3?PROCESS+GETCURRENTPID
   يجب تنفيذ ذلك وأنت داخل اللعبة، لا من XMB. سجّل رقم PID الذي يرجع.
6. حمّل وحدة التشخيص داخل عملية اللعبة:
   http://PS3_IP/ps3mapi.ps3?MODULE+LOAD+PID_HERE+/dev_hdd0/tmp/ghosts_campaign_pad_probe.fake.sprx
   استبدل PID_HERE بالرقم الذي ظهر في الخطوة السابقة.
7. تحقق من أن الوحدة حُمّلت:
   http://PS3_IP/ps3mapi.ps3?MODULE+GETALLPRXID+PID_HERE
   ثم استخدم MODULE+GETNAME أو MODULE+GETFILENAME مع PID ومعرّف الوحدة للتأكد من اسم ghosts_pad_probe.
8. انتظر 30 ثانية حتى ينتهي أخذ العينات، ثم انسخ ملف السجل المناسب إلى الكمبيوتر.
9. أزل الوحدة بعد نسخ السجل:
   http://PS3_IP/ps3mapi.ps3?MODULE+UNLOAD+PID_HERE+PRX_ID
10. افحص GETALLPRXID مرة أخرى وتأكد أن PRX_ID اختفى. إذا بقيت الوحدة في القائمة، لا تحاول إجبار إزالتها؛ انتظر ثانية وأعد طلب UNLOAD. الوحدة الآن ترفض الإيقاف (STOP_FAIL) إذا لم تستطع انتظار انتهاء Thread القراءة بأمان.

ملف Campaign:
- mode_files/ghosts_probe_mode_CAMPAIGN.txt
- الناتج: /dev_hdd0/tmp/ghosts_campaign_pad_probe.log

ملف Multiplayer:
- mode_files/ghosts_probe_mode_MULTIPLAYER.txt
- الناتج: /dev_hdd0/tmp/ghosts_multiplayer_pad_probe.log

**تنبيه:** رد MODULE LOAD قد يرجع HTTP 200 حتى لو لم تنجح عملية التحميل؛ لذلك لا تعتمد عليه وحده. افحص قائمة الوحدات واسم الوحدة عبر GETALLPRXID وGETNAME/GETFILENAME. سطر MODE في السجل مصدره ملف الوضع الذي اخترته أنت؛ الوحدة لا تكتشف Campaign أو Multiplayer تلقائياً، لذلك تأكد من الوضع قبل التحميل.

إذا كان ملف الوضع مفقوداً أو لا يحتوي إحدى الكلمتين بالضبط، لا تبدأ الوحدة sampling وتكتب سبب الرفض إلى ghosts_pad_probe_error.log. لا تشغّل ملف CAMPAIGN وأنت داخل Multiplayer أو العكس.

## المقارنة

~~~bash
python tools/compare_pad_probe_logs.py ghosts_campaign_pad_probe.log ghosts_multiplayer_pad_probe.log --json --output pad_probe_comparison.json
~~~

**تفسير مهم:** يجب أن يظهر mask يساوي 0x03 في عينة واحدة على الأقل لإثبات أن port 0 وport 1 ظهرا متصلين في الوقت نفسه. ظهور port 1 في عينة وport 0 في عينة أخرى لا يكفي.

حتى ظهور 0x03 لا يثبت أن المحرك أنشأ Local Client ثانياً أو Player Entity أو كاميرا مستقلة. هذا الاختبار يثبت حالة طبقة Pad فقط.
