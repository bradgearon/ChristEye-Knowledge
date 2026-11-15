import tempfile,unittest,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from anaiero.core import UsageEvent,Ledger,sha256
from anaiero.bus import WindBus,WindMessage,WindError
from anaiero.memory import MemoryBank
from anaiero.store import ContentStore
from anaiero.crystal import compile_digital_crystal,clone_digital_crystal,verify_digital_crystal,replay_to_memory,CrystalError
from anaiero.roots import macro_recall,retention_index,RootAssessment
from anaiero.lotus import Lotus,ActionRequest
from anaiero.curriculum import Curriculum,Lesson
from anaiero.care import CareScheduler,ServicePlan,PhysicalActionUnsupported

def event(i="evt-001",scope="shareable",export=True,training=True):
    return UsageEvent(event_id=i,project_id="project1",kind="demonstration",
                      content="Remember source and context for a book note",
                      source_id="source-note",scope=scope,
                      use_for_training=training,allow_export=export)

class AnaieroPublicPrototypeTests(unittest.TestCase):
    def test_hash_ledger_and_revocation(self):
        led=Ledger();led.append(event());self.assertTrue(led.verify())
        with self.assertRaises(ValueError):led.append(event())
        led.revoke("evt-001")
        self.assertTrue(led.verify())
        self.assertEqual(led.events(),[])

    def test_export_consent_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            with self.assertRaises(CrystalError):
                compile_digital_crystal([event(scope="local",export=False)],Path(td)/"p","anaiero.music.crystal",export=True)

    def test_training_consent_required(self):
        with tempfile.TemporaryDirectory() as td:
            with self.assertRaises(CrystalError):
                compile_digital_crystal([event(training=False)],Path(td)/"p","anaiero.music.crystal")

    def test_compile_verify_clone_replay(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/"p";c=Path(td)/"clone"
            compile_digital_crystal([event()],p,"anaiero.music.crystal",export=True)
            clone_digital_crystal(p,c)
            self.assertEqual(len(verify_digital_crystal(c)[1]),1)
            bank=MemoryBank()
            self.assertEqual(replay_to_memory(c,bank)["new_events"],1)
            self.assertEqual(replay_to_memory(c,bank)["new_events"],0)
            self.assertTrue(bank.recall("book note"))

    def test_tampered_crystal_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/"p"
            compile_digital_crystal([event()],p,"anaiero.music.crystal")
            (p/"assets"/"experiences.json").write_text("{}")
            with self.assertRaises(CrystalError):verify_digital_crystal(p)

    def test_physical_crystal_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/"p"
            compile_digital_crystal([event()],p,"anaiero.music.crystal")
            m=json.loads((p/"manifest.json").read_text())
            m["physical_carrier"]=True
            (p/"manifest.json").write_text(json.dumps(m))
            with self.assertRaises(CrystalError):verify_digital_crystal(p)

    def test_content_store_integrity_and_paths(self):
        with tempfile.TemporaryDirectory() as td:
            st=ContentStore(Path(td))
            snap=st.snapshot({"folder/note.txt":b"hello"})
            self.assertEqual(st.restore(snap)["folder/note.txt"],b"hello")
            with self.assertRaises(ValueError):st.snapshot({"../escape":b"x"})

    def test_lotus_vetoes_physical_actions(self):
        result=Lotus().evaluate(ActionRequest("dispense_medium","core","evidence",.9,0,.1))
        self.assertEqual(result["state"],"VETO")
        result=Lotus().evaluate(ActionRequest("train_digital","skill","evidence",.9,-.25,.1))
        self.assertEqual(result["state"],"HOLD")

    def test_lotus_unresolved_and_stage(self):
        self.assertEqual(Lotus().evaluate(ActionRequest("train_digital","skill","evidence",.01,0,.2))["state"],"UNRESOLVED")
        self.assertEqual(Lotus().evaluate(ActionRequest("train_digital","skill","evidence",.9,0,.2))["state"],"STAGE_DIGITAL")

    def test_roots_macro_recall_and_ratio_guard(self):
        score,per=macro_recall(["X","Y","HOLD"],["X","Y","HOLD"])
        self.assertEqual(score,1.0)
        self.assertIsNone(retention_index(1/3,1/3))
        self.assertEqual(RootAssessment("r1",.85,.8,"24h",{},.7).evaluate()["status"],"CANDIDATE")

    def test_curriculum_requires_acyclic_dependencies(self):
        a=Lesson("bread","BREAD",(),"source-a","assessment-a")
        b=Lesson("meat","MEAT",("bread",),"source-b","assessment-b")
        self.assertEqual(len(Curriculum([a,b]).available(set())),1)
        with self.assertRaises(ValueError):
            Curriculum([Lesson("a","BREAD",("b",),"s","e"),Lesson("b","MEAT",("a",),"s","e")])

    def test_windbus_authorization_idempotency(self):
        seen=[]
        bus=WindBus(2)
        bus.register("lotus",{"observer"},{"observation"},lambda m:seen.append(m.event_id))
        msg=WindMessage(event_id="wind-01",source="observer",recipient="lotus",kind="observation",payload_digest="a"*64,expires_at=100)
        self.assertTrue(bus.publish(msg,now=5))
        self.assertFalse(bus.publish(msg,now=6))
        self.assertEqual(len(bus.drain(now=7)),1)
        self.assertEqual(seen,["wind-01"])
        with self.assertRaises(WindError):
            bus.publish(WindMessage("wind-02","unknown","lotus","observation","a"*64,expires_at=100),now=5)

    def test_windbus_rejects_expired_and_queue_overflow(self):
        bus=WindBus(1)
        bus.register("lotus",{"observer"},{"observation"},lambda m:None)
        with self.assertRaises(WindError):
            bus.publish(WindMessage("expired","observer","lotus","observation","a"*64,expires_at=1),now=2)
        bus.publish(WindMessage("x1","observer","lotus","observation","a"*64,expires_at=100),now=1)
        with self.assertRaises(WindError):
            bus.publish(WindMessage("x2","observer","lotus","observation","a"*64,expires_at=100),now=1)

    def test_care_scheduler_cannot_dispense(self):
        c=CareScheduler()
        c.register(ServicePlan("s1","prep1","qualified-SOP","operator",10,5))
        self.assertEqual(len(c.due(11)),1)
        with self.assertRaises(PhysicalActionUnsupported):c.dispense()
        with self.assertRaises(ValueError):c.record_external_service("s1","wrong","qualified-SOP",11)
        self.assertFalse(c.record_external_service("s1","operator","qualified-SOP",11)["late"])

if __name__=="__main__":unittest.main()
