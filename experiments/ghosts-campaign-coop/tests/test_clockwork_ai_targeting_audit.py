import unittest

from tools.clockwork_ai_targeting_audit import audit


INTERIOR = r'''
discovery_guys_wakeup()
{
    foreach ( var_1 in level.discovery_guys )
    {
        var_1 getenemyinfo( level.player );
    }
}
'''

NVG = r'''
begin_interior()
{
    var_7 = maps\_utility::get_closest_ai( level.player.origin, "axis" );
    var_7 maps\clockwork_code::char_dialog_add_and_go( "clockwork_spg1_heyyoucantpark" );
}
'''

CODE = r'''
attack_targets( var_0, var_1, var_2 )
{
    var_0[var_7] getenemyinfo( var_1[var_6] );
    var_1[var_6] thread fight_back();
}
listen_player_press_x()
{
    var_1 = distance( level.player.origin, self.origin );
    if ( var_1 < 2000 )
        thread maps\clockwork_audio::chase_crashmix( self.origin );
}
'''


class ClockworkAiTargetingAuditTests(unittest.TestCase):
    def test_baseline_ai_targeting_classification_passes(self):
        report = audit(INTERIOR, NVG, CODE)
        self.assertEqual(report["status"], "PASS", report["checks"])
        self.assertEqual(len(report["checks"]), 5)

    def test_discovery_ai_primary_only_target_is_detected(self):
        changed = INTERIOR.replace("getenemyinfo( level.player )", "getenemyinfo( level.playengineeffects )")
        report = audit(changed, NVG, CODE)
        self.assertFalse(report["checks"]["discovery_ai_has_one_primary_only_enemy_target_call"])
        self.assertEqual(report["status"], "BLOCKED")

    def test_shared_ally_attack_targets_must_not_be_player_scoped(self):
        changed = CODE.replace("var_0[var_7] getenemyinfo( var_1[var_6] )", "var_0[var_7] getenemyinfo( level.player )")
        report = audit(INTERIOR, NVG, changed)
        self.assertFalse(report["checks"]["shared_ally_attack_targets_ai_group_not_player"])

    def test_dialogue_selection_must_not_be_reclassified_as_enemy_aggro(self):
        changed = NVG.replace('char_dialog_add_and_go( "clockwork_spg1_heyyoucantpark" )', "getenemyinfo( level.player );")
        report = audit(INTERIOR, changed, CODE)
        self.assertFalse(report["checks"]["intro_closest_ai_call_is_dialogue_only"])

    def test_snowmobile_audio_distance_must_remain_noncombat(self):
        changed = CODE.replace("chase_crashmix( self.origin )", "getenemyinfo( level.player )")
        report = audit(INTERIOR, NVG, changed)
        self.assertFalse(report["checks"]["snowmobile_player_distance_is_audio_only"])

    def test_missing_discovery_target_call_blocks_audit(self):
        changed = INTERIOR.replace("var_1 getenemyinfo( level.player );", "")
        report = audit(changed, NVG, CODE)
        self.assertFalse(report["checks"]["discovery_ai_has_one_primary_only_enemy_target_call"])


if __name__ == "__main__":
    unittest.main()
