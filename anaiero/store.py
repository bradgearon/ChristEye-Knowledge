"""Content-addressed digital filesystem with atomic objects and snapshots."""
from pathlib import Path
import json
from .core import sha256,canonical_bytes

class ContentStore:
    def __init__(self,root:Path):
        self.root=Path(root)
        (self.root/"objects").mkdir(parents=True,exist_ok=True)
        (self.root/"snapshots").mkdir(exist_ok=True)

    def put(self,data:bytes)->str:
        digest=sha256(data)
        path=self.root/"objects"/digest
        if not path.exists():
            tmp=path.with_suffix(".tmp")
            tmp.write_bytes(data)
            tmp.replace(path)
        return digest

    def get(self,digest:str)->bytes:
        if len(digest)!=64 or any(c not in "0123456789abcdef" for c in digest):
            raise ValueError("invalid digest")
        data=(self.root/"objects"/digest).read_bytes()
        if sha256(data)!=digest: raise ValueError("integrity failure")
        return data

    def snapshot(self,files:dict[str,bytes])->str:
        manifest={"schema_version":"2.0","items":{}}
        for name,content in sorted(files.items()):
            if not name or name.startswith("/") or ".." in Path(name).parts or "\\" in name:
                raise ValueError("unsafe path")
            manifest["items"][name]=self.put(content)
        raw=canonical_bytes(manifest)
        snap_id=sha256(raw)
        (self.root/"snapshots"/(snap_id+".json")).write_bytes(raw)
        return snap_id

    def restore(self,snap_id:str)->dict[str,bytes]:
        if len(snap_id)!=64: raise ValueError("bad snapshot id")
        raw=(self.root/"snapshots"/(snap_id+".json")).read_bytes()
        if sha256(raw)!=snap_id: raise ValueError("snapshot integrity failure")
        doc=json.loads(raw)
        return {path:self.get(digest) for path,digest in doc["items"].items()}
