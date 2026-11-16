#!/usr/bin/env python3
"""Create a pinned SIM3 runner configuration from an existing CEVLM image.

The VLM image is referenced by hash, not executed as a biological template.
For real research samples, do not store raw genomic content in this example.
"""
import hashlib
from pathlib import Path
import argparse


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def make_config(base: Path, profile: Path, output: Path, source_type: str = "synthetic") -> None:
    base, profile, output = base.resolve(), profile.resolve(), output.resolve()
    out_root = output.parent
    def relative(path: Path) -> str:
        parts = path.relative_to(out_root).parts
        if not parts or any(p in {"..", "."} for p in parts):
            raise ValueError("source files must be within the output directory tree")
        return "/".join(parts)
    base_rel, profile_rel = relative(base), relative(profile)
    profile_source = profile.read_text(encoding="utf-8")
    if not profile_source.startswith("CEVSOURCE/1\n"):
        raise ValueError("source profile must have a CEVSOURCE/1 heading")
    header = next((line.strip() for line in base.read_text(encoding="utf-8").splitlines()
                   if line.strip() and not line.lstrip().startswith("#")), None)
    if header not in {"CEVLM/1", "CEVLM/2"}:
        raise ValueError("base image must identify its CEVLM version")
    if source_type not in {"synthetic", "blood", "hair", "engineered"}:
        raise ValueError("unsupported source type")
    if f"\nsource {source_type}\n" not in "\n" + profile_source:
        raise ValueError("source profile must declare the selected source type")
    template = f"""CEVRUN/1
name connected-gardens
source {source_type} {profile_rel} {digest(profile)}
colony crest {base_rel} {digest(base)}
colony blood {base_rel} {digest(base)}
seed crest 1 crest_progenitor 10 12 2 0
seed crest 2 blood_carrier 3 5 2 0
seed blood 3 neuron 20 17 4 0
link crest blood food 3
link crest blood data 4
link blood crest stimubus 2
process crest support 0 5 1
process crest divide 1 4 2
process blood teach 3 8 2
process blood foreign_stress 0 13 3
end
"""
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(template, encoding="utf-8", newline="\n")
    print(f"Created {output} with pinned source hashes")


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("base_avlm", type=Path)
    p.add_argument("source_profile", type=Path)
    p.add_argument("output_config", type=Path)
    p.add_argument("--source-type", choices=["synthetic", "blood", "hair", "engineered"], default="synthetic")
    args = p.parse_args()
    make_config(args.base_avlm, args.source_profile, args.output_config, args.source_type)
