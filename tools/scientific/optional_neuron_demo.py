"""Optional offline NEURON example. Not part of the C++ controller runtime.

Install NEURON separately, then use this example to produce a normalized
voltage-feature trace for controlled playback. Its membrane parameters and
normalization are illustrative, not a validated neuron/astrocyte model.
"""
import argparse
import hashlib
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from scientific_trace_export import emit


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--epoch', type=int, default=1)
    args = ap.parse_args()
    try:
        from neuron import h
        import neuron
    except ImportError as exc:
        raise SystemExit('NEURON must be installed separately for this optional demo') from exc

    h.load_file('stdrun.hoc')
    sec = h.Section(name='anaiero_example_passive_cell')
    sec.L = 20
    sec.diam = 20
    sec.insert('pas')
    clamp = h.IClamp(sec(0.5))
    clamp.delay = 2
    clamp.dur = 12
    clamp.amp = 0.1
    volt = h.Vector().record(sec(0.5)._ref_v)
    h.dt = 0.025
    h.finitialize(-65)
    h.continuerun(28)
    raw_voltage = [float(x) for x in volt]
    if len(raw_voltage) < 112:
        raise RuntimeError('Insufficient NEURON samples')
    # This is just a documented transformation, not a physiological readout.
    # Map a selected illustrative voltage window to a 0..1000 proxy.
    values = []
    for i in range(112):
        index = min(len(raw_voltage)-1, round((i+1)*(len(raw_voltage)-1)/112))
        values.append(max(0, min(1000, round((raw_voltage[index]+85)*20))))
    raw = ('\n'.join(format(x,'.14g') for x in raw_voltage)+'\n').encode()
    model = Path(__file__).read_bytes()+str(getattr(neuron,'__version__','unknown')).encode()
    transform = b'voltage_proxy=clamp(round((mV+85)*20),0,1000); N=112; NO_CALIBRATION\n'
    text = emit('NEURON', 'ELECTRICAL_ACTIVITY', values, model, raw, transform, args.epoch)
    if args.out.exists():
        raise FileExistsError('Will not overwrite a sealed trace')
    args.out.write_bytes(text.encode('ascii'))
    print(f'Wrote {len(values)} NEURON-derived model samples; not biologically validated')


if __name__ == '__main__':
    main()
