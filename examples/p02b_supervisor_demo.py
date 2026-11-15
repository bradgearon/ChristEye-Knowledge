"""Offline P02B telemetry demo, using only synthetic values."""
from anaiero_p02b.telemetry import Board,TelemetryGateway,make_frame
from anaiero_p02b.supervisor import ReadOnlySupervisor

if __name__=="__main__":
    board=Board("sim","rev-a","f"*64,frozenset({"temperature_c"}),"sim-cal","simulated")
    key=b"local-demo-only"
    gateway=TelemetryGateway({"sim":board},{"sim":key})
    supervisor=ReadOnlySupervisor()
    supervisor.enter_observation()
    receipt=gateway.receive(make_frame(board=board,key=key,sequence=1,observed_ms=990,
                       metric="temperature_c",value=22.0,unit="degC"),now_ms=1000)
    supervisor.receive_receipt(receipt)
    print({"mode":supervisor.mode,"receipt":receipt})
