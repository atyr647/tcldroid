# Shared helpers; source this, don't execute it.
set -euo pipefail

TCLDROID_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export TCLDROID_ROOT
# shellcheck source=../upstream/versions.env
. "$TCLDROID_ROOT/upstream/versions.env"

CACHE_DIR="${TCLDROID_CACHE:-$TCLDROID_ROOT/.cache}"      # downloads
BUILD_DIR="${TCLDROID_BUILD:-$TCLDROID_ROOT/build}"       # sources + build trees
export CACHE_DIR BUILD_DIR

# Toolchain location: honour an existing NDK, else use the one fetch-ndk.sh installs.
NDK_HOME="${ANDROID_NDK_HOME:-$CACHE_DIR/android-ndk-$NDK_RELEASE}"
export NDK_HOME

log() { printf '\033[1m==> %s\033[0m\n' "$*" >&2; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }

# fetch URL DEST ALGO HASH  -- download once, verify always.
fetch() {
  local url="$1" dest="$2" algo="$3" want="$4" got
  mkdir -p "$(dirname "$dest")"
  if [ ! -f "$dest" ]; then
    log "downloading $(basename "$dest")"
    curl -fsSL --retry 3 -o "$dest.part" "$url"
    mv "$dest.part" "$dest"
  fi
  got="$(${algo}sum "$dest" | cut -d' ' -f1)"
  [ "$got" = "$want" ] || die "$algo mismatch for $dest: expected $want, got $got"
}

# abi_triple ABI -> clang target triple prefix and Tcl/Tk --host triple.
abi_triple() {
  case "$1" in
    arm64-v8a) echo aarch64-linux-android ;;
    x86_64)    echo x86_64-linux-android ;;
    armeabi-v7a) echo armv7a-linux-androideabi ;;
    x86)       echo i686-linux-android ;;
    *) die "unsupported ABI: $1" ;;
  esac
}
