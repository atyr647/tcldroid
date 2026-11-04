#!/usr/bin/env bash
# Turn a Tcl/Tk app folder into its own installable Android APK.
#   usage: tools/pack-app.sh APPDIR [--name "My App"] [--id com.example.myapp]
#                                   [--version 1.0] [--code 1]
#                                   [--orientation portrait|landscape|sensor|auto]
#                                   [--permission android.permission.CAMERA ...] [--abis "arm64-v8a"]
#                                   [--icon-bg '#RRGGBB'] [--backup]
#                                   [--splash '#0b0f19'] [--keystore my.jks --key-alias key0]
#                                   [--release] [--aab]
# Signing: without --keystore the APK carries the Android debug key (fine for your own phone;
# an app can only be updated in place by an APK signed with the SAME key and the same --id, with
# a higher --code). Make a key once with tools/make-keystore.sh and keep it safe. Passwords come
# from the environment (TCLDROID_KS_PASS, optional TCLDROID_KEY_PASS) or are asked for.
# APPDIR may also be a zip-based Starkit/Starpack file (MyApp.kit): it is unpacked first (see docs/starkit.md).
# APPDIR must contain main.tcl. Everything in it is shipped; icon.png (if present)
# becomes the launcher icon. The app runs with APPDIR as its working directory,
# APPDIR and APPDIR/lib on auto_path, and $env(HOME) pointing at private storage.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
[ $# -ge 1 ] || { sed -n '2,20p' "$0" | sed 's/^# \?//'; exit 2; }
dir=$1; shift
name=""; id=""; version="1.0"; code="1"; ks=""; alias=""; build=debug; aab=""; splash="#FFFFFF"; orient=auto; perms=""; abis=""; iconbg="#FFFFFF"; backup=0
while [ $# -gt 0 ]; do
  case $1 in
    --name) name=$2; shift 2;; --id) id=$2; shift 2;;
    --version) version=$2; shift 2;; --code) code=$2; shift 2;;
    --splash) splash=$2; shift 2;;
    --orientation) orient=$2; shift 2;; --permission) perms="$perms $2"; shift 2;;
    --abis) abis=$2; shift 2;; --icon-bg) iconbg=$2; shift 2;; --backup) backup=1; shift;;
    --keystore) ks=$2; shift 2;; --key-alias) alias=$2; shift 2;;
    --release) build=release; shift;; --aab) aab=1; build=release; shift;;
    *) echo "unknown option $1" >&2; exit 2;;
  esac
done
if [ -f "$dir" ]; then	# a Starkit/Starpack file: unpack it next to the output and pack the folder
  kit=$dir
  dir=$(mktemp -d "${TMPDIR:-/tmp}/tcldroid-kit.XXXXXX")
  python3 -I "$here/unkit.py" "$kit" "$dir" || exit 1
  [ -n "$name" ] || name=$(basename "$kit" | sed 's/\.[^.]*$//')
fi
[ -d "$dir" ] || { echo "not a folder: $dir" >&2; exit 1; }
[ -f "$dir/main.tcl" ] || { echo "$dir/main.tcl is missing (the app's entry point)" >&2; exit 1; }
[ -n "$name" ] || name=$(basename "$(cd "$dir" && pwd)")
if [ -z "$id" ]; then
  slug=$(printf %s "$name" | tr 'A-Z' 'a-z' | tr -c 'a-z0-9' '_' | sed 's/__*/_/g;s/^_//;s/_$//')
  case $slug in [a-z]*) ;; *) slug="app_$slug";; esac
  id="dev.tcldroid.app.$slug"
fi
[[ $id =~ ^[A-Za-z][A-Za-z0-9_]*(\.[A-Za-z][A-Za-z0-9_]*)+$ ]] || { echo "invalid application id '$id' (like com.example.myapp)" >&2; exit 1; }
[[ $splash =~ ^#[0-9A-Fa-f]{6}$ ]] || { echo "--splash must look like #RRGGBB" >&2; exit 1; }
[[ $code =~ ^[1-9][0-9]*$ ]] || { echo "--code must be a positive integer" >&2; exit 1; }
if [ -n "$ks" ]; then
  [ -f "$ks" ] || { echo "keystore not found: $ks" >&2; exit 1; }
  [ -n "$alias" ] || { echo "--keystore needs --key-alias" >&2; exit 1; }
  if [ -z "${TCLDROID_KS_PASS:-}" ]; then
    [ -t 0 ] || { echo "set TCLDROID_KS_PASS (keystore password)" >&2; exit 1; }
    read -r -s -p "Keystore password: " TCLDROID_KS_PASS; echo
  fi
  export TCLDROID_KS_PASS TCLDROID_KEY_ALIAS="$alias"
elif [ "$build" = release ]; then
  echo "--release/--aab need --keystore (see tools/make-keystore.sh)" >&2; exit 1
fi
echo "Packing '$name' ($id) $version [$code] from $dir"
APP_DIR=$dir APP_NAME=$name APP_ID=$id APP_VERSION=$version APP_CODE=$code APP_ORIENTATION=$orient APP_PERMISSIONS="$perms" APP_ABIS="$abis" APP_ICON_BG=$iconbg APP_BACKUP=$backup APP_SPLASH=$splash APP_KEYSTORE=$ks APP_BUILD=$build APP_AAB=$aab exec "$here/build-android-host.sh"
