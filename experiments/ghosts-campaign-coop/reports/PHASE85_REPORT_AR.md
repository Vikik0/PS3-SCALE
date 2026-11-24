# تقرير المرحلة 85 — قفل ملكية أحداث Campaign القابل لإعادة الاستخدام

## الهدف

حوّلت توصية المرحلة 84 إلى واجهة GSC عامة لإدارة ملكية تفاعلات المهمة المشتركة، وربطت دورة حياتها بجسر Local Session الحالي. الغرض منع لاعبين من بدء نفس تفاعل rope/console/weapon-hide/virus-upload في الوقت نفسه، دون فرض قفل عالمي يمنع أحداثاً مستقلة من العمل بالتوازي.

## التصميم

أُضيفت الدوال التالية إلى `ghc_campaign_local_session.gsc`:

| الدالة | الوظيفة |
|---|---|
| `ghc_campaign_event_lock_init` | يهيئ 16 Event IDs ومنفذي لاعب، ويعمل بصورة idempotent |
| `ghc_campaign_event_lock_player_slot` | يطابق كيان اللاعب مع slot 0 أو 1 في الزوج الأصلي |
| `ghc_campaign_event_lock_acquire` | يحجز Event ID للاعب واحد بعد التحقق من الجلسة والزوج الأصلي |
| `ghc_campaign_event_lock_release` | يسمح للمالك فقط بتحرير الحدث، مع خيار تسجيل اكتماله |
| `ghc_campaign_event_lock_abort_slot/player` | يحرر الحجز دون تسجيل اكتمال عند الإلغاء أو الانقطاع |
| `ghc_campaign_event_lock_clear_active` | يحرر جميع الحجوزات النشطة عند إبطال الجلسة |
| `ghc_campaign_event_lock_owner` | يعيد slot المالك أو -1 |
| `ghc_campaign_event_lock_active_for_player` | يعيد Event ID النشط للاعب أو -1 |
| `ghc_campaign_event_lock_is_complete` | يفحص اكتمال الحدث |
| `ghc_campaign_event_lock_is_owner` | يتحقق من أن اللاعب الحالي هو مالك الحدث |
| `ghc_campaign_event_lock_reset` | يعيد فتح حدث مكتمل فقط إذا لم يكن محجوزاً |

## قواعد الملكية

- Event IDs من 0 إلى 15؛ ويجب أن يستخدم كل من يستطيع تشغيل الهدف المشترك الرقم نفسه.
- لا يملك الحدث الواحد إلا لاعب واحد في اللحظة نفسها.
- لا ينفذ اللاعب أكثر من تفاعل واحد في الوقت نفسه.
- يمكن للاعبين امتلاك Event IDs مختلفة بالتوازي؛ لا يوجد global active lock يمنع كل الأحداث الأخرى.
- بعد release مع completed=1 يصبح الحدث مكتملاً ولا يمكن حجزه مرة أخرى حتى reset صريح.
- الإلغاء أو الموت أو الانقطاع يستخدم abort/release مع completed=0؛ لا يتحول الإلغاء إلى نجاح.
- لا يوجد wait/yield داخل عمليات الحجز والتحرير؛ تتم مراجعة الشروط وتحديث الملكية في المقطع نفسه.

## ربط دورة الحياة

- بعد التحقق من native player pair، ينفذ Local Session تهيئة Event Lock.
- عند rebind أو stale-session invalidation، تُحرر الحجوزات النشطة للمنفذين قبل إزالة aliases.
- حالة اكتمال الأحداث تبقى محفوظة عبر rebind، بينما لا تبقى ملكية نشطة للاعب قديم.
- acquire/release يتحققان من أن native pair ما زال هو نفسه. إذا تغيّر، يُبطل Local Session وتُحرر الحجوزات بدلاً من استخدام handles قديمة.
- أضيفت حواجز fail-closed إذا استُدعيت release أو is_owner قبل تهيئة القفل.

## الاختبارات والبناء

- GitHub Actions run [#1494](https://github.com/Vikik0/PS3-SCALE/actions/runs/45508312714): **6/6 jobs successful**.
- **480 اختباراً** ناجحاً.
- GSC Tool 1.6.2.364: compile + decompile round-trip ناجح.
- حجم `ghc_campaign_local_session.gscbin`: **3,680 bytes**.
- Source SHA-256: `7156ac56906d361de9348af9c558ea4a6bb04720596e156cca31d78d3b10e3a4`.
- Bytecode SHA-256: `2ba80b1be5497a522c08b519a5bd7b8eecbd2b435963313c790075f060850967`.
- manifest يثبت hash المصدر والـbytecode والحجم.

## حدود التنفيذ

- Event Lock مهيأ ومربوط بدورة حياة Local Session، لكنه **لم يُربط بعد** بدوال Black Ice أو Carrier أو Cornered الأصلية.
- لا يتم إنشاء Local Client أو لاعب أو Controller؛ القفل يعمل فقط بعد أن يثبت الجسر وجود زوج Native صالح.
- لم يتم حقن GSCBIN في FastFile retail ولم يُختبر على PS3 فعلي.
- لا يثبت وجود لاعب Campaign ثانٍ قابل للعب أو split-screen يعمل داخل اللعبة.

## طريقة استخدامه عند بدء ربط أحداث المهمات

1. خصص Event ID ثابتاً لكل هدف مشترك؛ مثلاً rope أو console أو virus upload.
2. استخدم نفس الرقم في كل trigger أو script يمكنه بدء الهدف نفسه.
3. استدعِ acquire قبل تعطيل حركة اللاعب أو بدء الأنيميشن.
4. عند النجاح، استدعِ release مع completed=1.
5. عند الإلغاء أو الموت أو disconnect، استدعِ abort_player أو release مع completed=0.
6. لا تستخدم Event IDs مختلفة لحدثين يتشاركان dvar أو flag أو جهازاً عالمياً؛ وإلا سيسمح القفل بتزامنهما.

## البوابة التالية

بعد وصول سجلات Input Probe الفعلية وPPU runtime dumps المطابقة، يمكن ربط هذا القفل بأحداث المهمات واحداً تلو الآخر، مع إبقاء mission coordinator مشتركاً وحالة الكاميرا/السلاح/الحركة خاصة بكل لاعب.
