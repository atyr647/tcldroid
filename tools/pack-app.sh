#!/usr/bin/env bash
# Turn a Tcl/Tk app folder into its own installable Android APK.
#   usage: tools/pack-app.sh APPDIR [--name "My App"] [--id com.example.myapp]
#                                   [--version 1.0] [--code 1]
# APPDIR must contain main.tcl. Everything in it is shipped; icon.png (if present)
# becomes the launcher icon. The app runs with APPDIR as its working directory,
# APPDIR and APPDIR/lib on auto_path, and $env(HOME) pointing at private storage.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
[ $# -ge 1 ] || { sed -n '2,7p' "$0" | sed 's/^# \?//'; exit 2; }
dir=$1; shift
name=""; id=""; version="1.0"; code="1"
while [ $# -gt 0 ]; do
  case $1 in
    --name) name=$2; shift 2;; --id) id=$2; shift 2;;
    --version) version=$2; shift 2;; --code) code=$2; shift 2;;
    *) echo "unknown option $1" >&2; exit 2;;
  esac
done
[ -d "$dir" ] || { echo "not a folder: $dir" >&2; exit 1; }
[ -f "$dir/main.tcl" ] || { echo "$dir/main.tcl is missing (the app's entry point)" >&2; exit 1; }
[ -n "$name" ] || name=$(basename "$(cd "$dir" && pwd)")
if [ -z "$id" ]; then
  slug=$(printf %s "$name" | tr 'A-Z' 'a-z' | tr -c 'a-z0-9' '_' | sed 's/__*/_/g;s/^_//;s/_$//')
  case $slug in [a-z]*) ;; *) slug="app_$slug";; esac
  id="dev.tcldroid.app.$slug"
fi
[[ $id =~ ^[A-Za-z][A-Za-z0-9_]*(\.[A-Za-z][A-Za-z0-9_]*)+$ ]] || { echo "invalid application id '$id' (like com.example.myapp)" >&2; exit 1; }
[[ $code =~ ^[1-9][0-9]*$ ]] || { echo "--code must be a positive integer" >&2; exit 1; }
echo "Packing '$name' ($id) $version [$code] from $dir"
APP_DIR=$dir APP_NAME=$name APP_ID=$id APP_VERSION=$version APP_CODE=$code exec "$here/build-android-host.sh"
