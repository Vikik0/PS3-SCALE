# تقرير المرحلة 68 — تحليل الملفات الفعلية المرفوعة

## التحقق من المدخلات

تم فحص ملفات FastFile الثلاثة المرفوعة ومطابقة الحجم وSHA-256 وMagic وإصدار TU مع القيم المثبتة. كما تطابقت بصمات الملفات مع تقارير الاستخراج داخل أرشيف Phase 31.

| المهمة | ملفات GSC | تعريفات الدوال | دوال Player Container | مراجع playerpiggyback | مراجع playengineeffects |
|---|---:|---:|---:|---:|---:|
| Black Ice | 76 | 1,217 | 0 | 0 | 0 |
| Carrier | 118 | 1,537 | 4 | 9 | 4 |
| Cornered | 55 | 1,550 | 9 | 10 | 0 |

## نتائج Call Graph

- تم العثور على 13 تعريفاً دقيقاً حسب الملف واسم الدالة، دون أهداف مفقودة أو مكررة.
- يوجد 14 موقع استدعاء وارد، ولا توجد Function Pointer references مباشرة إلى الدوال المستهدفة.
- دالة واحدة قابلة للوصول المباشر من Script Roots الحالية، وثماني دوال عند احتساب مسارات Callback، وخمس دوال لم يظهر لها مسار من هذه الجذور.
- مراجع disable_stealth_system الثلاثة تعتمد على Unique Symbol Fallback لأن ربط Namespace بمسار الملف غير محسوم؛ لذلك ثقتها أقل من المراجع التي تطابقت داخل الملف نفسه.

## أدلة Split-Screen المهمة

1. في Carrier، تفحص monitorscavengerpickup أن حجم level.playerpiggyback يساوي 2، ثم تربط العنصرين 0 و1 بالمتغيرين cg_playerFovScale0 وcg_playerFovScale1.
2. الدالة get_different_favoriteenemy تختار أحد عناصر playerpiggyback حسب المقارنة مع level.playengineeffects.
3. الدالة playerview_playmissanim تستخدم level.playengineeffects وlevel.player كمعرّفين منفصلين في hideonclient ضمن مسار أنيميشن Player Drone.

هذه مؤشرات قوية على وجود منطق per-client أو split-screen، لكنها لا تثبت وحدها أن المصفوفة تحتوي لاعبين محليين Native أو أن Campaign تهيئ Player 2.

## حدود الاستخراج

تقارير FastFile الثلاثة تصنف الاستخراج VALIDATED_PARTIAL. توقفت Black Ice وCarrier بسبب invalid_or_non_deflate_trailing_data، وتوقفت Cornered عند zero_terminator. لذلك النتائج تخص ملفات GSC الـ249 المستخرجة، ولا تعني أن كامل محتوى كل FastFile استُخرج.

## المرحلة التالية

المرحلة 69 ستبحث عن عمليات إنشاء أو إسناد level.playerpiggyback وlevel.playengineeffects في كل ملفات GSC المتاحة، وتفصل بين القراءة/الكتابة من السكربتات وبين المتغيرات التي قد يتم إعدادها من shared scripts أو native engine.
