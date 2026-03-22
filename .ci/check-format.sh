#!/usr/bin/env bash

# The -e is not set because we want to get all the mismatch format at once

set -u -o pipefail

# In CI environment, require all formatting tools to be present
# Set CI=true to enforce tool presence (GitHub Actions sets this automatically)
REQUIRE_TOOLS="${CI:-false}"

C_FORMAT_EXIT=0
SH_FORMAT_EXIT=0

# On pull requests, check only changed files so pre-existing formatting
# debt does not block unrelated PRs.  On push (to main) or local runs
# without GITHUB_BASE_REF, check the full tree.
if [ -n "${GITHUB_BASE_REF:-}" ]; then
    echo "PR mode: checking only files changed vs origin/${GITHUB_BASE_REF}"
    if git fetch --depth=1 origin "${GITHUB_BASE_REF}" 2> /dev/null \
        && CHANGED=$(git diff --name-only --diff-filter=d "origin/${GITHUB_BASE_REF}"...HEAD 2> /dev/null); then
        # If formatting config changed, check the full tree against the new rules.
        if printf '%s\n' "$CHANGED" | grep -qE '^\.(clang-format|editorconfig)$'; then
            echo "Formatting config changed; checking full tree"
            CHANGED=$(git ls-files)
        fi
    else
        echo "Warning: cannot compute PR diff, falling back to full-tree check" >&2
        CHANGED=$(git ls-files)
    fi
else
    CHANGED=$(git ls-files)
fi

# Filter changed files into language-specific sets
C_SOURCES=()
while IFS= read -r file; do
    [ -n "$file" ] || continue
    case "$file" in
        include/*.h | src/*.c | src/*.h | ports/*.c | ports/*.h | \
            ports/*/*.c | ports/*/*.h | \
            ports/*/*/*.c | ports/*/*/*.h)
            C_SOURCES+=("$file")
            ;;
    esac
done <<< "$CHANGED"

if [ ${#C_SOURCES[@]} -gt 0 ]; then
    if command -v clang-format-20 > /dev/null 2>&1; then
        echo "Checking C files with clang-format-20..."
        clang-format-20 -n --Werror "${C_SOURCES[@]}"
        C_FORMAT_EXIT=$?
    elif command -v clang-format > /dev/null 2>&1; then
        echo "Checking C files with clang-format..."
        clang-format -n --Werror "${C_SOURCES[@]}"
        C_FORMAT_EXIT=$?
    else
        if [ "$REQUIRE_TOOLS" = "true" ]; then
            echo "ERROR: clang-format not found (required in CI)" >&2
            C_FORMAT_EXIT=1
        else
            echo "Skipping C format check: clang-format not found" >&2
        fi
    fi
fi

SH_SOURCES=()
while IFS= read -r file; do
    [ -n "$file" ] || continue
    case "$file" in
        *.sh | .ci/*.sh | scripts/*.sh)
            SH_SOURCES+=("$file")
            ;;
    esac
done <<< "$CHANGED"

if [ ${#SH_SOURCES[@]} -gt 0 ]; then
    if command -v shfmt > /dev/null 2>&1; then
        echo "Checking shell scripts..."
        MISMATCHED_SH=$(shfmt -l "${SH_SOURCES[@]}")
        if [ -n "$MISMATCHED_SH" ]; then
            echo "The following shell scripts are not formatted correctly:"
            printf '%s\n' "$MISMATCHED_SH"
            shfmt -d "${SH_SOURCES[@]}"
            SH_FORMAT_EXIT=1
        fi
    else
        if [ "$REQUIRE_TOOLS" = "true" ]; then
            echo "ERROR: shfmt not found (required in CI)" >&2
            SH_FORMAT_EXIT=1
        else
            echo "Skipping shell script format check: shfmt not found" >&2
        fi
    fi
fi

# Use logical OR to avoid exit code overflow (codes are mod 256)
if [ $C_FORMAT_EXIT -ne 0 ] || [ $SH_FORMAT_EXIT -ne 0 ]; then
    exit 1
fi
exit 0
