# تقرير المرحلة 82 — قياس بيانات يدَي التحكم بصورة مستقلة

## الهدف

Pad Probe v4 يثبت اتصال منافذ 0 و1 عبر `cellPadGetInfo2`، لكنه لا يقرأ الأزرار أو العصي. كان لدينا بالفعل decoder ومحول PS3 في `local_input.c` و`local_input_ps3.c`، لكن لم توجد أداة مستقلة تسجل عينات فعلية من كل منفذ وتثبت اختلاف مدخلاتهما أثناء الاتصال المتزامن.

## التنفيذ

1. أضيف PRX تشخيصي منفصل `pad_input_probe` يعيد استخدام `ghc_local_input_poll_ps3` بدلاً من كتابة decoder موازٍ.
2. المحول يقرأ حالة المنافذ بـ`cellPadGetInfo2` ثم يستدعي `cellPadGetData` للمنفذين 0 و1 المتصلين فقط.
3. يسجل probe ثلاثين عينة بفاصل ثانية، مع digital1/digital2، وbutton mask، وpressed/released edges، والعصي الخام والمطبّعة، وضغط L2/R2، وread-error masks.
4. يحتفظ بسجل منفصل لـCampaign وآخر لـMultiplayer. اختيار الوضع يظل يدوياً من `ghosts_probe_mode.txt`؛ لا توجد محاولة لاكتشاف الوضع تلقائياً.
5. لا يستدعي `cellPadInit` أو `cellPadEnd` أو أي `cellPadSet*`، ولا يكتب ذاكرة اللعبة ولا يثبت hooks. الوحدة تشخيصية ولا تنشئ Local Client أو Player Entity.
6. أضيف comparator `tools/compare_pad_input_probe_logs.py` الذي يرفض العينات الناقصة أو المكررة أو ذات read errors أو masks غير المتطابقة، ولا يعلن استقلال الإدخال إلا عند اتصال المنفذين ووجود نشاط مختلف على كليهما في العينة نفسها في الوضعين.
7. أضيفت اختبارات mock لـ`local_input_ps3.c` باستخدام واجهات PS3 SDK مصطنعة على الكمبيوتر، لتغطية القراءة المستقلة للمنفذين، وانقطاع منفذ، وفشل القراءة، وبيانات قصيرة، وفشل Pad Info.
8. أضيفت اختبارات مصدرية تؤكد أن PRX لا يهيئ أو ينهي CellPad ولا يستعمل أوامر كتابة أو hooks.

## نتائج CI

- مجموعة Python الكاملة: **452 اختباراً ناجحاً**.
- اختبارات C لمحول PS3: **PASS**.
- بناء PRX باستخدام PS3DK v0.28.0: **PASS**.
- `prx-gen check` لوحدة `ghosts_pad_input_probe.prx`: **PASS**.
- GitHub Actions run **#1437** على commit `c25a2ed070e5ff25d5a1c8ccdc34f0ee664758f5`: **6/6 jobs successful**.
- Artifact: `ghosts-dualshock-input-probe`.

## شروط التشغيل الآمن وحدود الإثبات

- هذه الوحدة تقرأ `cellPadGetData` من Thread منفصل بينما اللعبة تعمل. استخدمها في Offline فقط، وشغّلها وحدها ولا تجمعها مع Pad Probe v4 في الوقت نفسه. يجب اختبار الاستقرار على جهاز PS3 قبل اعتمادها كمسار دائم.
- نجاح CI يثبت البناء وصحة المحول في اختبارات mock فقط؛ لا يثبت أن اللعبة على الجهاز تسمح بقراءة المنفذين دون تعارض.
- نجاح المقارنة يثبت فقط أن طبقة Pad أعادت حالات أزرار مختلفة من port 0 وport 1. لا يثبت إنشاء Local Client 1 أو لاعب Campaign ثانٍ أو كاميرا/HUD مستقلة.

## الخطوة التالية

شغّل Input Probe مرة داخل Campaign Offline ومرة داخل Multiplayer Offline، مع الضغط على Cross في اليد الأولى وCircle في الثانية بالتزامن لعدة ثوانٍ، ثم بدّل الأزرار والعصي. أرسل الملفين الناتجين إلى الكمبيوتر لتحليلها. بعد إثبات استقلال port input، ننتقل إلى ربط هذه البيانات بمسار client command وNative Local Client على PS3.
