set shell := ["zsh", "-euc"]

project := "shark-bytes"
gbdk_home := if env_var_or_default("GBDK_HOME", "") != "" { env_var_or_default("GBDK_HOME", "") } else { justfile_directory() + "/tools/gbdk" }
lcc := gbdk_home + "/bin/lcc"
rom := justfile_directory() + "/dist/" + project + ".gb"
build_artifacts := "dist/*.gb dist/*.ihx dist/*.map dist/*.noi dist/*.sym dist/*.lst dist/*.cdb dist/*.adb dist/*.asm dist/*.rst"
title_sprite_input := "src/sprites/diver_standing.piskel"
title_sprite_output := "src/sprites/diver_title_sprite"
diver_swimming_input := "src/sprites/diver_swimming.piskel"
diver_swimming_output := "src/sprites/diver_swimming_sprite"
fish_yellow_input := "src/sprites/fish_yellow.piskel"
fish_yellow_output := "src/sprites/fish_yellow_sprite"
build_sources := "src/main.c " + title_sprite_output + ".c " + diver_swimming_output + ".c " + fish_yellow_output + ".c"

# List available commands.
default:
  @just --list

# Regenerate sprite assets from source art.
sprites:
  python3 scripts/piskel_to_gbdk_sprite.py "{{title_sprite_input}}" "{{title_sprite_output}}" --symbol diver_title_sprite --scale 3
  python3 scripts/piskel_to_gbdk_sprite.py "{{diver_swimming_input}}" "{{diver_swimming_output}}" --symbol diver_swimming_sprite
  python3 scripts/piskel_to_gbdk_sprite.py "{{fish_yellow_input}}" "{{fish_yellow_output}}" --symbol fish_yellow_sprite

# Build the Game Boy ROM.
build: sprites
  mkdir -p dist
  xattr -dr com.apple.quarantine "{{gbdk_home}}" 2>/dev/null || true
  test -x "{{lcc}}" || { echo "Missing GBDK SDK at {{gbdk_home}}"; echo "Run: just sdk-install"; echo "Or set GBDK_HOME to an installed GBDK SDK root."; exit 1; }
  "{{lcc}}" -o "{{rom}}" {{build_sources}}

# Build the ROM as a basic verification step.
test: build

# Download and unpack the official GBDK SDK release.
sdk-install:
  @mkdir -p "{{justfile_directory()}}/tools"; \
  case "$(uname -s)-$(uname -m)" in \
    Darwin-arm64) asset="gbdk-macos-arm64.tar.gz" ;; \
    Darwin-x86_64) asset="gbdk-macos.tar.gz" ;; \
    Linux-x86_64) asset="gbdk-linux64.tar.gz" ;; \
    Linux-aarch64|Linux-arm64) asset="gbdk-linux-arm64.tar.gz" ;; \
    *) echo "Unsupported platform: $(uname -s)-$(uname -m)"; exit 1 ;; \
  esac; \
  tmp="$(mktemp -d)"; \
  curl -fL "https://github.com/gbdk-2020/gbdk-2020/releases/latest/download/${asset}" -o "$tmp/gbdk.tar.gz"; \
  tar -xzf "$tmp/gbdk.tar.gz" -C "$tmp"; \
  rm -rf "{{gbdk_home}}"; \
  mv "$tmp/gbdk" "{{gbdk_home}}"; \
  rm -rf "$tmp"; \
  xattr -dr com.apple.quarantine "{{gbdk_home}}" 2>/dev/null || true

# Build and open the ROM in SameBoy.
sameboy: build
  open -a SameBoy "{{rom}}"

# Remove generated build artifacts.
clean:
  rm -f {{build_artifacts}}
