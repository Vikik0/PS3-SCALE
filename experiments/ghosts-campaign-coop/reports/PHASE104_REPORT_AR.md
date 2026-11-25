# تقرير المرحلة 104 — مدقق أدلة زوج اللاعبين الأصليين

أضفت أداة تقرأ ملف JSON يحتوي على ملاحظات Runtime فعلية من Campaign وتتحقق من ثبات زوج اللاعبين قبل اعتبار البيانات متوافقة مع شروط Player 2.

تتحقق الأداة من:
- BLUS31270 وTU 01.16 ووضع Campaign.
- وجود لاعبين محليين بالضبط.
- تمييز Local Client 0 و1 ومنفذي التحكم 0 و1.
- اختلاف عناوين gentity وgclient وplayer state.
- وجود primary واحد وsecondary واحد.
- تطابق `level.player` مع الأساسي و`playengineeffects` مع الثانوي.
- استقلال مراجع Camera وHUD وارتباطها بكيان اللاعب الصحيح.
- خمس عينات متتالية مستقرة، مع كشف دورة انقطاع ثم إعادة ربط بشكل منفصل.

**مهم:** الأداة تتحقق من اتساق البيانات المقدمة فقط، ولا تستطيع إثبات أن مصدرها PS3 فعلي أو منع تزوير ملف JSON. لذلك يظهر دائماً `evidence_authenticity = NOT_AUTHENTICATED_BY_THIS_TOOL`.

## الاستخدام

```bash
python tools/ps3mapi/validate_campaign_player_pair_capture.py --capture path/to/campaign-player-capture.json
```

قالب الإدخال موجود في:

`tools/ps3mapi/CAMPAIGN_NATIVE_PLAYER_CAPTURE_TEMPLATE.json`

## نتائج الاختبارات

- Python: **558/558**.
- Event Lock C: **21/21**.
- GSC parser C: **10/10**.
- GSC loader C: **12/12**.
- Session monitor C: **14/14**.
- PS3 PRX وGSC compile/decompile وقراءات التحكم: نجاح.
- CI #1592: **7/7 ناجح**.
- Capture Kit: **57/57**، الحجم **537,725 bytes**.
- SHA-256: `7bf6595fdb0780bc442b130c403c7c3d7404659f14f11419b7246ad4bac1b02e`.
- Artifact ID: `15072824482`.

لم يتم تقديم Runtime Capture حقيقي حتى الآن، ولذلك لا تزال جاهزية التفاعلات الأصلية الأربعة **0/4**.
