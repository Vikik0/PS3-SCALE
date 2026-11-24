# تقرير المرحلة 70 — تتبع منطق Client Visibility وFOV

## الهدف

المرحلة 68 كشفت عن ربط playerpiggyback[0] وplayerpiggyback[1] بمتغيري FOV مختلفين، وعن استخدام level.player وlevel.playengineeffects في hideonclient. هذه المرحلة حولت هذه الإشارات إلى تقرير قابل للتكرار.

## التعديلات

1. أضفت audit_split_screen_client_semantics.py لتتبع showonclient وhideonclient مع الطرف المستقبل ومعرّف العميل المستخدم.
2. صنفت level.player كـ Primary Player Candidate وlevel.playengineeffects كـ Secondary Client Candidate، دون اعتبار ذلك إثباتاً نهائياً.
3. ربطت index 0 مع cg_playerFovScale0 وindex 1 مع cg_playerFovScale1.
4. تتبعت استخدام setsaveddvar مع self.sniper_degy.
5. تتبعت مقارنة self.favoriteenemy مع level.playengineeffects.
6. أضفت بصمة مصدر قبل وبعد الفحص.

## النتيجة المتوقعة من الملفات المرفوعة

- خمسة استدعاءات client visibility في Carrier ضمن playerview_playmissanim: واحد showonclient وأربعة hideonclient.
- selector level.player يظهر مرتين في hideonclient، وlevel.playengineeffects يظهر مرتين.
- ربطا FOV موجودان في monitorscavengerpickup.
- يوجد استدعاءان لـ setsaveddvar على self.sniper_degy.
- يوجد موضع واحد يقارن favoriteenemy مع playengineeffects.

## الحدود

هذه إشارات Script-side إلى client-specific rendering وFOV. لا تحدد مكان إنشاء العميل الثاني داخل Native PPU ولا تثبت تشغيل لاعبين فعليين.
