import unittest,tempfile
from pathlib import Path
from anaiero_p02b.telemetry import Board,TelemetryGateway,make_frame,sign
from anaiero_p02b.supervisor import ReadOnlySupervisor,HashChainedLedger,ForbiddenActuation
from anaiero_p02b.scientists import StudyHandoff

class P02BSupervisorTests(unittest.TestCase):
    def setUp(self):
        self.b=Board("sim","rev-a","f"*64,frozenset({"temperature_c"}),"cal","simulated")
        self.key=b"dev-test-key"
        self.g=TelemetryGateway({"sim":self.b},{"sim":self.key})
        self.t=1000000

    def frame(self,**changes):
        args=dict(board=self.b,key=self.key,sequence=1,observed_ms=self.t-100,
                  metric="temperature_c",value=22.0,unit="degC")
        args.update(changes)
        return make_frame(**args)

    def test_accept_and_duplicate(self):
        frame=self.frame()
        self.assertEqual(self.g.receive(frame,now_ms=self.t)["outcome"],"accepted")
        self.assertEqual(self.g.receive(frame,now_ms=self.t)["reason"],"duplicate_or_out_of_order")

    def test_stale_future_and_forgery(self):
        self.assertEqual(self.g.receive(self.frame(observed_ms=0),now_ms=self.t)["reason"],"stale_or_future_sample")
        self.assertEqual(self.g.receive(self.frame(observed_ms=self.t+5000),now_ms=self.t)["reason"],"stale_or_future_sample")
        frame=self.frame();frame["value"]=99
        self.assertEqual(self.g.receive(frame,now_ms=self.t)["reason"],"authentication_failed")

    def test_command_and_unknown_sensor(self):
        frame=self.frame();frame["command"]="feed"
        self.assertEqual(self.g.receive(frame,now_ms=self.t)["reason"],"invalid_fields_or_command")
        self.assertEqual(self.g.receive(self.frame(metric="lactate"),now_ms=self.t)["reason"],"undeclared_measurement")

    def test_fault_latches_and_actuation_forbidden(self):
        s=ReadOnlySupervisor();s.enter_observation()
        with self.assertRaises(ForbiddenActuation):s.request("stimulate")
        s.receive_receipt({"outcome":"quarantined","reason":"auth"})
        self.assertEqual(s.mode,"FAULT_LATCHED")
        with self.assertRaises(ForbiddenActuation):s.acknowledge_fault(human_reviewed=False)
        s.acknowledge_fault(human_reviewed=True)
        self.assertEqual(s.mode,"OBSERVE_ONLY")

    def test_audit_chain(self):
        with tempfile.TemporaryDirectory() as td:
            path=Path(td)/"ledger.jsonl";ledger=HashChainedLedger(path)
            ledger.append({"event":"test"});ledger.append({"event":"later"})
            self.assertTrue(HashChainedLedger.verify(path))

    def test_scientific_handoff_signatures(self):
        bad=StudyHandoff("study","question",None,False,False,True,True,False,True)
        self.assertFalse(bad.ready_for_institutional_review())
        ok=StudyHandoff("study","question","line",True,True,True,True,True,True)
        self.assertTrue(ok.ready_for_institutional_review())

if __name__=="__main__":unittest.main()
