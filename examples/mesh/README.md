# Native authenticated mesh example

This example transfers committed SW5/SIM3 observations and an immutable file
between two native Anaiero nodes. It uses a local demo CA and fresh state
directories. No private keys are included in the repository.

Run from the repository root. Build the optional mesh with OpenSSL 3 available:

```sh
cmake -S native -B build/mesh -DANAIERO_CONTROLLER_ONLY=ON \
  -DANAIERO_ENABLE_MESH=ON -DCE_WIND_BUILD_TESTS=ON \
  -DCMAKE_DISABLE_FIND_PACKAGE_Python3=TRUE -DCMAKE_BUILD_TYPE=Release
cmake --build build/mesh --config Release --parallel 2 --target ce_mesh ce_colony_runtime
```

On Windows, configure with the native MSVC generator and the OpenSSL development
package/toolchain, as shown in
[native-mesh.yml](../../.github/workflows/native-mesh.yml). The executable paths
are normally `build/mesh/Release/ce_mesh.exe` and
`build/mesh/Release/ce_colony_runtime.exe`. OpenSSL's command-line program is
used only to prepare the example certificates, not as a mesh subprocess.

## Source scopes and direction

The supplied [mixed sustained session](../runtime/mixed-sustained.cevsession)
contains these exact scopes. A scope is `SOURCE:EPOCH:CLOCK:COLONY`, not the
engine's cadence or its tick duration.

| Engine | Source | Epoch | Clock domain | Colony | Scope |
| --- | --- | --- | --- | --- | --- |
| SW5, engine 101 | 61 | 1 | 101 | 101 | `61:1:101:101` |
| SIM3, engine 201 | 71 | 1 | 201 | 201 | `71:1:201:201` |

For node B to pull node A's evidence, enrollment is deliberately directional:

| Policy stored at | Enrolled principal | Grants |
| --- | --- | --- |
| A | `node-a`, A's own certificate | Publish both scopes, permitting explicit local import |
| A | `node-b`, B's certificate | Subscribe to both scopes and read the example artifact digest |
| B | `node-a`, A's certificate | Publish both scopes, permitting B to accept observations received from A |

The client also pins A's exact certificate fingerprint. A CA-valid certificate
without the matching enrollment and grants is rejected. The fingerprints below
are SHA-256 of certificate DER bytes, normalized to lowercase without colons.
Enrollment expiry uses Unix seconds and is set to one day from setup; the demo
leaf certificates last two days. Both expiration limits are enforced.

## Linux: complete Bash walkthrough

Use Bash with OpenSSL 3 on PATH. The following creates a fresh temporary tree
and restricts newly created files to the local user:

```bash
umask 077
mesh_bin="$PWD/build/mesh/ce_mesh"
mesh_runtime="$PWD/build/mesh/ce_colony_runtime"
mesh_work="$(mktemp -d "${TMPDIR:-/tmp}/anaiero-mesh.XXXXXX")"
mkdir "$mesh_work/credentials"
printf 'Example state: %s\n' "$mesh_work"

openssl req -x509 -newkey rsa:2048 -sha256 -noenc -days 3 \
  -subj '/CN=Anaiero Local Demo CA' \
  -addext 'basicConstraints=critical,CA:TRUE' \
  -addext 'keyUsage=critical,keyCertSign,cRLSign' \
  -keyout "$mesh_work/credentials/ca.key" \
  -out "$mesh_work/credentials/ca.crt"

cat > "$mesh_work/credentials/leaf.ext" <<'EOF'
basicConstraints=critical,CA:FALSE
keyUsage=critical,digitalSignature,keyEncipherment
extendedKeyUsage=serverAuth,clientAuth
EOF

for mesh_node in a b; do
  openssl req -new -newkey rsa:2048 -sha256 -noenc \
    -subj "/CN=node-$mesh_node" \
    -keyout "$mesh_work/credentials/$mesh_node.key" \
    -out "$mesh_work/credentials/$mesh_node.csr"
done

openssl x509 -req -sha256 -days 2 -set_serial 1001 \
  -in "$mesh_work/credentials/a.csr" \
  -CA "$mesh_work/credentials/ca.crt" -CAkey "$mesh_work/credentials/ca.key" \
  -extfile "$mesh_work/credentials/leaf.ext" -out "$mesh_work/credentials/a.crt"
openssl x509 -req -sha256 -days 2 -set_serial 1002 \
  -in "$mesh_work/credentials/b.csr" \
  -CA "$mesh_work/credentials/ca.crt" -CAkey "$mesh_work/credentials/ca.key" \
  -extfile "$mesh_work/credentials/leaf.ext" -out "$mesh_work/credentials/b.crt"

mesh_a_pin="$(openssl x509 -in "$mesh_work/credentials/a.crt" -noout -fingerprint -sha256 | cut -d= -f2 | tr -d ':' | tr 'A-F' 'a-f')"
mesh_b_pin="$(openssl x509 -in "$mesh_work/credentials/b.crt" -noout -fingerprint -sha256 | cut -d= -f2 | tr -d ':' | tr 'A-F' 'a-f')"
mesh_expiry="$(date -u -d '+1 day' +%s)"

"$mesh_bin" configure "$mesh_work/a.cevmesh" mesh-a node-a \
  "$mesh_work/credentials/a.crt" "$mesh_work/credentials/a.key" "$mesh_work/credentials/ca.crt" \
  127.0.0.1 40451 127.0.0.1 0 none
"$mesh_bin" configure "$mesh_work/b.cevmesh" mesh-b node-b \
  "$mesh_work/credentials/b.crt" "$mesh_work/credentials/b.key" "$mesh_work/credentials/ca.crt" \
  127.0.0.1 0 127.0.0.1 40451 "$mesh_a_pin"

printf 'Anaiero immutable mesh artifact\n' > "$mesh_work/example.txt"
mesh_artifact="$(openssl dgst -sha256 "$mesh_work/example.txt" | awk '{print $NF}')"
"$mesh_bin" artifact-add "$mesh_work/a.cevmesh" "$mesh_work/a-state" "$mesh_work/example.txt"

"$mesh_bin" enroll "$mesh_work/a.cevmesh" "$mesh_work/a-state" node-a "$mesh_a_pin" "$mesh_expiry" \
  publish=61:1:101:101 publish=71:1:201:201
"$mesh_bin" enroll "$mesh_work/a.cevmesh" "$mesh_work/a-state" node-b "$mesh_b_pin" "$mesh_expiry" \
  subscribe=61:1:101:101 subscribe=71:1:201:201 "artifact=$mesh_artifact"
"$mesh_bin" enroll "$mesh_work/b.cevmesh" "$mesh_work/b-state" node-a "$mesh_a_pin" "$mesh_expiry" \
  publish=61:1:101:101 publish=71:1:201:201

"$mesh_runtime" session examples/runtime/mixed-sustained.cevsession "$mesh_work/native-state" 4
"$mesh_bin" import-session "$mesh_work/a.cevmesh" "$mesh_work/a-state" \
  examples/runtime/mixed-sustained.cevsession "$mesh_work/native-state"
```

The session command exits before import, allowing the importer to acquire the
source session/evidence writer locks. Import verifies the configuration,
manifest, engine/source/clock identities and committed observation frames.
It records the local-import ingress kind. Repeating import accepts exact old
observations without adding projection revisions.

Start A as a native process with a caller-owned local administration pipe.
The `STATE` round trip waits for the host owner to be ready without polling:

```bash
coproc ANAIERO_DEMO { "$mesh_bin" serve-stdio "$mesh_work/a.cevmesh" "$mesh_work/a-state"; }
mesh_server_pid="$ANAIERO_DEMO_PID"
printf 'STATE\n' >&"${ANAIERO_DEMO[1]}"
IFS= read -r mesh_ready <&"${ANAIERO_DEMO[0]}"
printf '%s\n' "$mesh_ready"

"$mesh_bin" sync "$mesh_work/b.cevmesh" "$mesh_work/b-state" sw5-display 61:1:101:101
"$mesh_bin" sync "$mesh_work/b.cevmesh" "$mesh_work/b-state" sim3-display 71:1:201:201
"$mesh_bin" fetch-artifact "$mesh_work/b.cevmesh" "$mesh_work/b-state" "$mesh_artifact"
"$mesh_bin" inspect "$mesh_work/b.cevmesh" "$mesh_work/b-state"

# Reconnect using the same durable subscription. No new source observations
# have been produced, so this should add no projection revisions.
"$mesh_bin" sync "$mesh_work/b.cevmesh" "$mesh_work/b-state" sw5-display 61:1:101:101

# The two initial enrollments put A's policy at revision 2.
printf 'REVOKE node-b 2\n' >&"${ANAIERO_DEMO[1]}"
IFS= read -r mesh_revocation <&"${ANAIERO_DEMO[0]}"
printf '%s\n' "$mesh_revocation"
if "$mesh_bin" sync "$mesh_work/b.cevmesh" "$mesh_work/b-state" sw5-display 61:1:101:101; then
  printf 'Unexpected: revoked peer was accepted\n' >&2
else
  printf 'Expected: revoked peer cannot open the mesh session\n'
fi

printf 'STOP\n' >&"${ANAIERO_DEMO[1]}"
IFS= read -r mesh_stopped <&"${ANAIERO_DEMO[0]}"
printf '%s\n' "$mesh_stopped"
wait "$mesh_server_pid"
"$mesh_bin" maintain "$mesh_work/a.cevmesh" "$mesh_work/a-state"
"$mesh_bin" maintain "$mesh_work/b.cevmesh" "$mesh_work/b-state"
```

The example uses a local pipe for automated setup. For normal same-user IPC,
run this foreground command instead of `serve-stdio`:

```bash
"$mesh_bin" serve "$mesh_work/a.cevmesh" "$mesh_work/a-state" "$mesh_work/admin.sock"
```

From another terminal, use the same executable and actual state path:

```bash
"$mesh_bin" query "$mesh_work/admin.sock" STATE
"$mesh_bin" query "$mesh_work/admin.sock" 'REVOKE node-b 2'
"$mesh_bin" query "$mesh_work/admin.sock" STOP
```

## Windows: native PowerShell walkthrough

Use PowerShell 7, native MSVC-built executables and OpenSSL 3 on PATH. These
commands use native Windows processes; WSL and Python are not involved.

```powershell
$ErrorActionPreference = 'Stop'
$meshBin = (Resolve-Path 'build/mesh/Release/ce_mesh.exe').Path
$meshRuntime = (Resolve-Path 'build/mesh/Release/ce_colony_runtime.exe').Path
$meshWork = Join-Path ([IO.Path]::GetTempPath()) ('anaiero-mesh-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path "$meshWork/credentials" | Out-Null
Write-Host "Example state: $meshWork"

openssl req -x509 -newkey rsa:2048 -sha256 -noenc -days 3 -subj '/CN=Anaiero Local Demo CA' -addext 'basicConstraints=critical,CA:TRUE' -addext 'keyUsage=critical,keyCertSign,cRLSign' -keyout "$meshWork/credentials/ca.key" -out "$meshWork/credentials/ca.crt"
@'
basicConstraints=critical,CA:FALSE
keyUsage=critical,digitalSignature,keyEncipherment
extendedKeyUsage=serverAuth,clientAuth
'@ | Set-Content -Encoding ascii "$meshWork/credentials/leaf.ext"
foreach ($meshNode in @('a', 'b')) {
  openssl req -new -newkey rsa:2048 -sha256 -noenc -subj "/CN=node-$meshNode" -keyout "$meshWork/credentials/$meshNode.key" -out "$meshWork/credentials/$meshNode.csr"
}
openssl x509 -req -sha256 -days 2 -set_serial 1001 -in "$meshWork/credentials/a.csr" -CA "$meshWork/credentials/ca.crt" -CAkey "$meshWork/credentials/ca.key" -extfile "$meshWork/credentials/leaf.ext" -out "$meshWork/credentials/a.crt"
openssl x509 -req -sha256 -days 2 -set_serial 1002 -in "$meshWork/credentials/b.csr" -CA "$meshWork/credentials/ca.crt" -CAkey "$meshWork/credentials/ca.key" -extfile "$meshWork/credentials/leaf.ext" -out "$meshWork/credentials/b.crt"
$meshAPin = ((openssl x509 -in "$meshWork/credentials/a.crt" -noout -fingerprint -sha256) -split '=')[-1].Replace(':', '').ToLowerInvariant()
$meshBPin = ((openssl x509 -in "$meshWork/credentials/b.crt" -noout -fingerprint -sha256) -split '=')[-1].Replace(':', '').ToLowerInvariant()
$meshExpiry = [DateTimeOffset]::UtcNow.AddDays(1).ToUnixTimeSeconds()

& $meshBin configure "$meshWork/a.cevmesh" mesh-a node-a "$meshWork/credentials/a.crt" "$meshWork/credentials/a.key" "$meshWork/credentials/ca.crt" 127.0.0.1 40451 127.0.0.1 0 none
& $meshBin configure "$meshWork/b.cevmesh" mesh-b node-b "$meshWork/credentials/b.crt" "$meshWork/credentials/b.key" "$meshWork/credentials/ca.crt" 127.0.0.1 0 127.0.0.1 40451 $meshAPin
[IO.File]::WriteAllText("$meshWork/example.txt", "Anaiero immutable mesh artifact`n", [Text.UTF8Encoding]::new($false))
$meshArtifact = (Get-FileHash -Algorithm SHA256 "$meshWork/example.txt").Hash.ToLowerInvariant()
& $meshBin artifact-add "$meshWork/a.cevmesh" "$meshWork/a-state" "$meshWork/example.txt"
& $meshBin enroll "$meshWork/a.cevmesh" "$meshWork/a-state" node-a $meshAPin $meshExpiry publish=61:1:101:101 publish=71:1:201:201
& $meshBin enroll "$meshWork/a.cevmesh" "$meshWork/a-state" node-b $meshBPin $meshExpiry subscribe=61:1:101:101 subscribe=71:1:201:201 "artifact=$meshArtifact"
& $meshBin enroll "$meshWork/b.cevmesh" "$meshWork/b-state" node-a $meshAPin $meshExpiry publish=61:1:101:101 publish=71:1:201:201
& $meshRuntime session examples/runtime/mixed-sustained.cevsession "$meshWork/native-state" 4
& $meshBin import-session "$meshWork/a.cevmesh" "$meshWork/a-state" examples/runtime/mixed-sustained.cevsession "$meshWork/native-state"

$meshStart = [Diagnostics.ProcessStartInfo]::new($meshBin)
$meshStart.UseShellExecute = $false
$meshStart.RedirectStandardInput = $true
$meshStart.RedirectStandardOutput = $true
foreach ($meshArgument in @('serve-stdio', "$meshWork/a.cevmesh", "$meshWork/a-state")) {
  $meshStart.ArgumentList.Add($meshArgument)
}
$meshServer = [Diagnostics.Process]::Start($meshStart)
$meshServer.StandardInput.AutoFlush = $true
$meshServer.StandardInput.WriteLine('STATE')
$meshServer.StandardOutput.ReadLine()

& $meshBin sync "$meshWork/b.cevmesh" "$meshWork/b-state" sw5-display 61:1:101:101
& $meshBin sync "$meshWork/b.cevmesh" "$meshWork/b-state" sim3-display 71:1:201:201
& $meshBin fetch-artifact "$meshWork/b.cevmesh" "$meshWork/b-state" $meshArtifact
& $meshBin inspect "$meshWork/b.cevmesh" "$meshWork/b-state"
& $meshBin sync "$meshWork/b.cevmesh" "$meshWork/b-state" sw5-display 61:1:101:101

$meshServer.StandardInput.WriteLine('REVOKE node-b 2')
$meshServer.StandardOutput.ReadLine()
& $meshBin sync "$meshWork/b.cevmesh" "$meshWork/b-state" sw5-display 61:1:101:101
if ($LASTEXITCODE -eq 0) { throw 'Unexpected: revoked peer was accepted' }

$meshServer.StandardInput.WriteLine('STOP')
$meshServer.StandardOutput.ReadLine()
$meshServer.WaitForExit()
& $meshBin maintain "$meshWork/a.cevmesh" "$meshWork/a-state"
& $meshBin maintain "$meshWork/b.cevmesh" "$meshWork/b-state"
```

For normal same-user named-pipe administration, replace the redirected process
with the following foreground server and query it from a second terminal:

```powershell
& $meshBin serve "$meshWork/a.cevmesh" "$meshWork/a-state" anaiero-mesh-demo
& $meshBin query anaiero-mesh-demo STATE
& $meshBin query anaiero-mesh-demo 'REVOKE node-b 2'
& $meshBin query anaiero-mesh-demo STOP
```

The short endpoint is converted to the native Windows named-pipe path by the
existing host adapter. Keep each command's `CONFIG` and `STATE` together.
Offline commands cannot open a state directory while its server owns it.

## Additional commands and policy updates

`ce_mesh --help` lists the exact argument order. `configure` writes a new sealed
configuration; `seal-config` and `seal-policy` validate and seal a local LF draft
into a new file. None overwrites an existing output. Policy changes retain
existing principals as tombstones and increment each changed peer generation.

| Command | Use |
| --- | --- |
| `ce_mesh policy CONFIG STATE` | Print the current sealed policy while the state is offline |
| `ce_mesh enroll CONFIG STATE PRINCIPAL CERT_SHA256 EXPIRES grant...` | Create or replace that peer's complete grant set |
| `ce_mesh import-policy CONFIG STATE POLICY EXPECTED_REVISION` | Import an explicit offline policy update using compare-and-swap |
| `ce_mesh query ENDPOINT 'IMPORT "POLICY" EXPECTED_REVISION'` | Apply the same update through the running local administration owner |
| `ce_mesh push CONFIG STATE SUBSCRIPTION SOURCE:EPOCH:CLOCK:COLONY` | Publish a local complete source stream to the configured server |
| `ce_mesh sync CONFIG STATE SUBSCRIPTION SOURCE:EPOCH:CLOCK:COLONY` | Pull the configured server's authorized source stream |
| `ce_mesh maintain CONFIG STATE` | Verify/maintain the offline mesh stores and advance eligible archives |

For live `IMPORT`, the policy filename is a double-quoted command field. Prefix
each literal backslash or double quote inside that field with a backslash.
This command escaping is separate from the shell's quoting of the complete
`query` argument. Windows forward-slash paths are also supported. For example,
in PowerShell, replace the example endpoint, policy path and revision with
your running server's values:

```powershell
& $meshBin query anaiero-mesh-demo 'IMPORT "C:/mesh policies/rotated policy.cevpeers" 3'
```

The equivalent quoted filename with escaped Windows separators is
`"C:\\mesh policies\\rotated policy.cevpeers"`. The offline `import-policy`
command takes its filename as a separate shell argument and does not use
the `IMPORT` command field's escaping.

`enroll` replaces the principal's complete grant set, so include its retained
grants when adding an artifact or rotating a certificate. It does not silently
merge a partial grant list. Read the current policy revision before an update;
the tutorial's literal revision `2` is valid only for its fresh two-enrollment
server state.

For `push`, the local policy grants the remote principal subscription access,
and the remote policy grants the local principal publication authority for
that same scope. A receiving source/epoch retains its first principal owner;
do not forward a previously received stream under a different principal while
pretending to be its original owner. Begin a distinct source epoch when
ownership changes.

After a policy replacement or certificate rotation, reconnect before further
requests. Local admin changes invalidate existing application handles even
when the underlying TLS connection has not yet closed. No network request can
enroll a peer, replace policy, run a solver or execute a transferred artifact.

The certificate commands follow OpenSSL's official
[request/certificate generation](https://docs.openssl.org/3.0/man1/openssl-req/)
and [X.509 signing/fingerprint](https://docs.openssl.org/3.0/man1/openssl-x509/)
interfaces. These local certificates demonstrate trust wiring and expiry;
operational deployments provide their own controlled CA and key lifecycle.
See [CE-SW-35](../../docs/software/CE-SW-35_authenticated_native_mesh.md) for
durability, wire semantics, resource limits and current verification boundaries.
