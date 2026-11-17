"""Offline external-model feature exporter. Optional tool, never needed by Anaiero.

This does *not* validate a scientific model. It converts a prespecified, already
normalized trace to a SHA-sealed record that the native controller can replay.
"""
import argparse
import csv
import hashlib
from pathlib import Path

PROVIDERS = {'NEURON', 'BRAINCELL', 'PHYSICELL', 'MOOSE', 'STEPS'}
ELECTRICAL = {'NEURON', 'BRAINCELL'}


def sha(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()


def emit(provider: str, channel: str, values: list[int], model: bytes,
         raw: bytes, transform: bytes, epoch: int) -> str:
    if provider not in PROVIDERS:
        raise ValueError('Provider not supported')
    if channel not in {'ELECTRICAL_ACTIVITY', 'ENVIRONMENT_PROXY'}:
        raise ValueError('Unknown feature channel')
    if (provider in ELECTRICAL) != (channel == 'ELECTRICAL_ACTIVITY'):
        raise ValueError('Channel not compatible with provider')
    if not 1 <= epoch <= 2**32-1 or not 4 <= len(values) <= 512:
        raise ValueError('Epoch/trace length outside bounds')
    if any(not isinstance(v, int) or not 0 <= v <= 1000 for v in values):
        raise ValueError('Invalid normalized feature')
    lines = [
        'CEVSCITRACE/1', f'provider {provider}',
        'classification MODEL_OUTPUT_UNVERIFIED', f'channel {channel}',
        f'model_sha256 {sha(model)}', f'raw_sha256 {sha(raw)}',
        f'transform_sha256 {sha(transform)}', f'source_epoch {epoch}',
        ('unit DIMENSIONLESS_ACTIVITY_PROXY' if channel == 'ELECTRICAL_ACTIVITY'
         else 'unit DIMENSIONLESS_ENVIRONMENT_PROXY')
    ]
    lines += [f'sample {i} {value}' for i, value in enumerate(values, start=1)]
    content = ('\n'.join(lines) + '\nend\n').encode('ascii')
    return content.decode('ascii') + f'seal {sha(content)}\n'


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--provider', required=True, choices=sorted(PROVIDERS))
    parser.add_argument('--channel', required=True,
                        choices=['ELECTRICAL_ACTIVITY', 'ENVIRONMENT_PROXY'])
    parser.add_argument('--csv', type=Path, required=True,
                        help='CSV columns: step, normalized_proxy (0..1000)')
    parser.add_argument('--model-file', type=Path, required=True,
                        help='the actual model definition/package used')
    parser.add_argument('--transform-file', type=Path, required=True,
                        help='document the unit-to-proxy transform used')
    parser.add_argument('--epoch', type=int, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    raw = args.csv.read_bytes()
    import io
    rows = list(csv.DictReader(io.StringIO(raw.decode('utf-8'))))
    if not 4 <= len(rows) <= 512:
        raise ValueError('Outside accepted frame bounds')
    values = []
    for index, row in enumerate(rows, start=1):
        if set(row) != {'step', 'normalized_proxy'} or int(row['step']) != index:
            raise ValueError(f'Unexpected index or column at row {index}')
        values.append(int(row['normalized_proxy']))
    text = emit(args.provider, args.channel, values, args.model_file.read_bytes(),
                raw, args.transform_file.read_bytes(), args.epoch)
    if args.out.exists():
        raise FileExistsError('Will not overwrite a sealed trace')
    args.out.write_bytes(text.encode('ascii'))


if __name__ == '__main__':
    main()
