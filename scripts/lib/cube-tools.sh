#!/bin/bash

FEB_CUBE=""
FEB_TOOLS_SOURCE="system PATH"

feb_find_cube() {
    [ -n "$FEB_CUBE" ] && return 0

    if command -v cube &>/dev/null; then
        FEB_CUBE="$(command -v cube)"
        return 0
    fi

    local candidate
    while IFS= read -r candidate; do
        if [ -x "$candidate" ]; then
            FEB_CUBE="$candidate"
            return 0
        fi
    done < <(
        printf '%s\n' "$HOME/.local/stm32cube/bin/cube" "$HOME/.local/stm32cube/bin/cube.exe"
        compgen -G "$HOME/.vscode/extensions/stmicroelectronics.stm32cube-ide-core-*/resources/binaries/*/*/cube*" | sort -V -r
    )
    return 1
}

# Usage: feb_use_cube_tools <project-dir> <cube-command>...
# e.g.   feb_use_cube_tools "$REPO_ROOT" cmake programmer
feb_use_cube_tools() {
    local project_dir="$1"
    shift
    feb_find_cube || return 1

    # Older setup.sh/README versions exported this to the STM32CubeCLT folder,
    # which hides every bundle from cube (and from VS Code's cube-cmake).
    case "${CUBE_BUNDLE_PATH:-}" in
        *[Ss][Tt][Mm]32[Cc]ube[Cc][Ll][Tt]*)
            echo "WARNING: CUBE_BUNDLE_PATH=$CUBE_BUNDLE_PATH points at STM32CubeCLT, not the cube bundles folder." >&2
            echo "         Remove 'export CUBE_BUNDLE_PATH=...' from your shell profile and restart VS Code." >&2
            ;;
    esac

    local tool resolved
    for tool in "$@"; do
        resolved=$("$FEB_CUBE" --cwd "$project_dir" --resolve "$tool" 2>/dev/null \
            | sed -n "s/^[[:space:]]*PATH='\(.*\)'\$/\1/p")
        [ -z "$resolved" ] && return 1
        if command -v cygpath &>/dev/null; then
            resolved="$(cygpath -u -p "$resolved")"
        fi
        export PATH="$resolved"
    done
    FEB_TOOLS_SOURCE="STM32Cube bundles"
}

feb_print_cube_install_hint() {
    echo "  Install the STM32Cube for Visual Studio Code extension (it provides the 'cube' CLI):"
    echo "    code --install-extension stmicroelectronics.stm32-vscode-extension"
    echo "  Then download the pinned toolchain from the repo root:"
    echo "    cube bundle install --project"
    echo "  'cube' lives in ~/.local/stm32cube/bin once the extension has run, add it to PATH:"
    echo "    zsh/bash: export PATH=\"\$HOME/.local/stm32cube/bin:\$PATH\""
    echo "    fish:     fish_add_path ~/.local/stm32cube/bin"
}
