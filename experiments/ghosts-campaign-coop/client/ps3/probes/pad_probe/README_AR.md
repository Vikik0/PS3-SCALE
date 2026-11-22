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

1. انسخ ملف الوضع المناسب إلى /dev_hdd0/tmp/ghosts_probe_mode.txt.
2. صِل وحدتي التحكم قبل تشغيل اللعبة.
3. ادخل Campaign أو Multiplayer Offline.
4. حمّل PRX داخل PID اللعبة عبر webMAN/PS3MAPI.
5. انتظر 30 ثانية ثم انسخ السجل المناسب إلى الكمبيوتر.

ملف Campaign:
- mode_files/ghosts_probe_mode_CAMPAIGN.txt
- الناتج: /dev_hdd0/tmp/ghosts_campaign_pad_probe.log

ملف Multiplayer:
- mode_files/ghosts_probe_mode_MULTIPLAYER.txt
- الناتج: /dev_hdd0/tmp/ghosts_multiplayer_pad_probe.log

إذا كان ملف الوضع مفقوداً أو لا يحتوي إحدى الكلمتين بالضبط، لا تبدأ الوحدة sampling وتكتب سبب الرفض إلى ghosts_pad_probe_error.log.

## المقارنة

~~~bash
python tools/compare_pad_probe_logs.py ghosts_campaign_pad_probe.log ghosts_multiplayer_pad_probe.log --json --output pad_probe_comparison.json
~~~

**تفسير مهم:** يجب أن يظهر mask يساوي 0x03 في عينة واحدة على الأقل لإثبات أن port 0 وport 1 ظهرا متصلين في الوقت نفسه. ظهور port 1 في عينة وport 0 في عينة أخرى لا يكفي.

حتى ظهور 0x03 لا يثبت أن المحرك أنشأ Local Client ثانياً أو Player Entity أو كاميرا مستقلة. هذا الاختبار يثبت حالة طبقة Pad فقط.
