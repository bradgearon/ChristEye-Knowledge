#!/usr/bin/env bash
# Managed systemd lifecycle. State/packages are never removed by this helper.
set -euo pipefail
action= name=AnaieroModular controller= state= endpoint= period=100 account= mesh= plan=
fail() { printf '%s\n' "$*" >&2; exit 1; }
[[ $# -ge 1 ]] || fail 'Use Install|Render|Switch|Reconcile|Start|Stop|Status|Remove and named options.'
action=$1; shift
while [[ $# -gt 0 ]]; do
  [[ $# -ge 2 ]] || fail 'Every option requires a value.'
  case "$1" in
    --name) name=$2;; --controller) controller=$2;; --state) state=$2;;
    --endpoint) endpoint=$2;; --period) period=$2;; --user) account=$2;;
    --mesh) mesh=$2;; --plan) plan=$2;; *) fail "Unknown option: $1";;
  esac
  shift 2
done
[[ "$name" =~ ^[A-Za-z0-9_-]{1,64}$ ]] || fail 'Invalid service name.'
[[ -n "$endpoint" ]] || endpoint="/run/anaiero-$name/control.sock"
unit="/etc/systemd/system/$name.service"
config="/etc/anaiero/services/$name.settings"
safe() { [[ -n "$1" && "$1" != *$'\n'* && "$1" != *$'\r'* ]] || fail 'Empty or multiline service setting.'; }
absolute() { safe "$1"; [[ "$1" == /* ]] || fail 'Service paths must be absolute.'; }
quote_systemd() {
  local value=$1
  value=${value//\\/\\\\}; value=${value//\"/\\\"}; value=${value//%/%%}; value=${value//\$/\$\$}
  printf '"%s"' "$value"
}
load() {
  [[ -f "$config" && ! -L "$config" ]] || fail 'No managed service settings exist.'
  [[ $(wc -c < "$config") -le 16384 ]] || fail 'Service settings exceed 16 KiB.'
  local values=()
  mapfile -t values < "$config"
  [[ ${#values[@]} -eq 8 && ${values[0]} == ANAIERO-SYSTEMD/1 && ${values[1]} == "$name" ]] || fail 'Invalid managed service settings.'
  account=${values[2]}; endpoint=${values[3]}; state=${values[4]}; period=${values[5]}; mesh=${values[6]}
  [[ -n "$controller" ]] || controller=${values[7]}
}
validate() {
  absolute "$controller"; absolute "$state"
  [[ -x "$controller" && -d "$state" ]] || fail 'Controller or retained state directory is unavailable.'
  absolute "$endpoint"
  [[ ${#endpoint} -le 100 && "$period" =~ ^[0-9]+$ && "$period" -ge 1 && "$period" -le 3600000 ]] || fail 'Invalid endpoint or period.'
  [[ "$account" =~ ^[A-Za-z_][A-Za-z0-9_-]*[$]?$ ]] || fail 'An explicit native service account is required.'
  [[ -z "$mesh" ]] || absolute "$mesh"
}
selected() {
  executable=$(session_command "$controller" package-selected "$state") || fail 'Canonical package selection is unavailable.'
  absolute "$executable"
  session_command "$executable" check-selected "$state" >/dev/null || fail 'Selected executable failed exact inventory/state admission.'
}
session_command() {
  # The service account remains the canonical store writer; root only owns
  # the registration and settings. Avoid root-owned state after a switch.
  if [[ $EUID -eq 0 && "$account" != root ]]; then runuser -u "$account" -- "$@"; else "$@"; fi
}
render() {
  printf '# ANAIERO-MANAGED/1\n[Unit]\nDescription=Anaiero canonical modular session\nAfter=network.target\n\n[Service]\nType=notify\nNotifyAccess=main\nUser=%s\nRuntimeDirectory=anaiero-%s\nRuntimeDirectoryMode=0700\nUMask=0077\nNoNewPrivileges=true\n' "$account" "$name"
  printf 'ExecStart='; quote_systemd "$executable"; printf ' start '; quote_systemd "$endpoint"; printf ' '; quote_systemd "$state"; printf ' %s' "$period"
  if [[ -n "$mesh" ]]; then printf ' --mesh '; quote_systemd "$mesh"; fi
  printf '\nKillSignal=SIGTERM\nTimeoutStopSec=120\nRestart=no\n\n[Install]\nWantedBy=multi-user.target\n'
}
stopped() {
  local status
  if ! status=$(systemctl show "$name.service" --property=ActiveState --value); then
    [[ ! -e "$unit" ]] || fail 'Cannot inspect the installed systemd service.'
    status=inactive
  fi
  [[ "$status" == inactive || "$status" == failed ]] || fail 'Stop/drain this service before changing its selected executable.'
}
save_settings() {
  local temporary
  mkdir -p /etc/anaiero/services
  temporary=$(mktemp "/etc/anaiero/services/$name.XXXXXXXX")
  (umask 077; printf 'ANAIERO-SYSTEMD/1\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n' "$name" "$account" "$endpoint" "$state" "$period" "$mesh" "$controller" > "$temporary")
  chmod 0600 "$temporary"; mv -f -- "$temporary" "$config"
}
publish_unit() {
  if [[ -e "$unit" ]]; then
    [[ ! -L "$unit" && $(head -n 1 "$unit") == '# ANAIERO-MANAGED/1' ]] || fail 'Refusing to replace an unmanaged service unit.'
  fi
  local temporary
  temporary=$(mktemp "/etc/systemd/system/$name.XXXXXXXX")
  render > "$temporary"; chmod 0644 "$temporary"; mv -f -- "$temporary" "$unit"
  systemctl daemon-reload
}
case "$action" in Install|Render) ;; *) load;; esac
if [[ "$action" == Render ]]; then validate; selected; render; exit 0; fi
[[ $EUID -eq 0 ]] || fail 'Native systemd service changes require root.'
lock="/run/lock/anaiero-$name"
mkdir "$lock" 2>/dev/null || fail 'Another lifecycle operation holds the service lock.'
trap 'rmdir -- "$lock"' EXIT
case "$action" in
  Install)
    validate; selected; id "$account" >/dev/null
    [[ ! -e "$unit" && ! -e "$config" ]] || fail 'Managed service already exists; use stopped Reconcile after an interrupted install.'
    save_settings
    publish_unit
    printf 'Installed stopped service %s; package and state are preserved.\n' "$name"
    ;;
  Switch|Reconcile)
    validate; stopped
    [[ "$action" != Switch || -n "$plan" ]] || fail 'Switch requires the immutable reviewed activation plan.'
    if [[ -n "$plan" ]]; then absolute "$plan"; session_command "$controller" package-activate "$state" "$plan"; fi
    selected; publish_unit; controller=$executable; save_settings
    printf 'Reconciled stopped service %s to %s; use Start.\n' "$name" "$executable"
    ;;
  Start)
    validate; stopped; selected
    temporary=$(mktemp); trap 'rm -f -- "$temporary"; rmdir -- "$lock"' EXIT
    render > "$temporary"; cmp -s "$temporary" "$unit" || fail 'Unit and canonical selection differ; run stopped Reconcile.'
    systemctl start "$name.service"; systemctl is-active --quiet "$name.service"
    ;;
  Stop) systemctl stop "$name.service"; stopped;;
  Status) systemctl show "$name.service" --property=ActiveState,SubState,User,ExecStart;;
  Remove)
    systemctl stop "$name.service"; stopped
    [[ ! -L "$unit" && $(head -n 1 "$unit") == '# ANAIERO-MANAGED/1' ]] || fail 'Refusing to remove an unmanaged unit.'
    rm -- "$unit" "$config"; systemctl daemon-reload
    printf 'Removed service registration; all package directories and canonical evidence remain.\n'
    ;;
  *) fail 'Unknown lifecycle action.';;
esac
