#!/usr/bin/env bash
# CI tools for collecting, aggregating, and formatting test results.
#
# Commands:
#   collect-data <rtos> <target> <os> <test_output>
#       Parse raw `make check` output into structured result files.
#
#   aggregate <results_dir> <output_file>
#       Merge per-job result files into a single TOML summary.
#
#   format-summary <toml_file>
#       Render a GitHub-flavored Markdown report for $GITHUB_STEP_SUMMARY.
#
#   post-comment <toml_file> <pr_number>
#       Post the formatted report as a PR comment (requires GITHUB_TOKEN).

set -euo pipefail

# Status symbols
sym_pass="✅"
sym_fail="❌"
sym_skip="⏭️"

get_symbol()
{
    case "$1" in
        passed) echo "$sym_pass" ;;
        failed) echo "$sym_fail" ;;
        *) echo "$sym_skip" ;;
    esac
}

# Minimal TOML helpers (no external deps).
# get_value <section> <key> <file>
get_value()
{
    awk -v sec="$1" -v key="$2" '
        /^\[/ { in_sec = ($0 == "[" sec "]") }
        in_sec && /=/ {
            k = $0; sub(/[[:space:]]*=.*/, "", k); gsub(/"/, "", k)
            nkey = key; gsub(/"/, "", nkey)
            if (k != nkey) next
            sub(/^[^=]*=[[:space:]]*"?/, ""); sub(/"?$/, ""); print; exit
        }
    ' "$3"
}

# get_section_keys <section> <file> -- prints key=value lines
get_section_keys()
{
    awk -v sec="$1" '
        /^\[/ { in_sec = ($0 == "[" sec "]") ; next }
        in_sec && /=/ { print }
    ' "$2"
}

# ── collect-data ─────────────────────────────────────────────────────
collect_data()
{
    local rtos="$1" target="$2" runner_os="$3" raw_output="$4"
    local outdir="test-results"
    mkdir -p "$outdir"

    echo "$rtos" > "$outdir/rtos"
    echo "$target" > "$outdir/target"
    echo "$runner_os" > "$outdir/runner_os"

    # Parse per-test results from make check output.
    # Format: "  tm_basic_processing ...                OK" or "FAIL"
    local test_name status
    local passed=0 failed=0 total=0
    while IFS= read -r line; do
        # Match lines like: "  tm_foo ...    OK" or "  tm_foo ...    FAIL"
        if echo "$line" | grep -qE '^[[:space:]]+tm_[^[:space:]]+[[:space:]]+\.\.\.'; then
            test_name=$(echo "$line" | awk '{print $1}')
            if echo "$line" | grep -q 'OK$'; then
                status="passed"
                passed=$((passed + 1))
            else
                status="failed"
                failed=$((failed + 1))
            fi
            total=$((total + 1))
            echo "${test_name}=${status}" >> "$outdir/tests_data"
        fi
        # Capture counter values: "Time Period Total: 12345"
        if echo "$line" | grep -q 'Time Period Total'; then
            echo "$line" >> "$outdir/counters"
        fi
    done <<< "$raw_output"

    echo "$passed" > "$outdir/passed"
    echo "$failed" > "$outdir/failed"
    echo "$total" > "$outdir/total"

    # Overall status
    if [ "$failed" -eq 0 ] && [ "$total" -gt 0 ]; then
        echo "passed" > "$outdir/status"
    else
        echo "failed" > "$outdir/status"
    fi
}

# ── aggregate ────────────────────────────────────────────────────────
aggregate()
{
    local results_dir="$1" output_file="$2"
    local commit_sha="${GITHUB_SHA:-$(git rev-parse HEAD 2> /dev/null || echo unknown)}"
    local commit_short="${commit_sha:0:7}"
    local repo="${GITHUB_REPOSITORY:-local}"
    local timestamp
    timestamp=$(date -u +"%Y-%m-%dT%H:%M:%SZ")

    local overall="passed"
    local job_count=0

    cat > "$output_file" << EOF
[summary]
status = "pending"
timestamp = "$timestamp"
commit_sha = "$commit_sha"
commit_short = "$commit_short"
repository = "$repo"

EOF

    # Iterate over each downloaded result set.
    for jobdir in "$results_dir"/*/; do
        [ -d "$jobdir" ] || continue
        # Skip directories without collected test data.
        [ -f "$jobdir/status" ] || continue
        local rtos target runner_os status passed failed total
        rtos=$(cat "$jobdir/rtos" 2> /dev/null || echo "unknown")
        target=$(cat "$jobdir/target" 2> /dev/null || echo "unknown")
        runner_os=$(cat "$jobdir/runner_os" 2> /dev/null || echo "unknown")
        status=$(cat "$jobdir/status" 2> /dev/null || echo "failed")
        passed=$(cat "$jobdir/passed" 2> /dev/null || echo 0)
        failed=$(cat "$jobdir/failed" 2> /dev/null || echo 0)
        total=$(cat "$jobdir/total" 2> /dev/null || echo 0)

        # Section name: rtos-target-os (e.g. threadx-posix-ubuntu-24.04)
        local section="${rtos}-${target}-${runner_os}"
        section=$(echo "$section" | tr '[:upper:]' '[:lower:]' | tr ' ' '-')

        cat >> "$output_file" << EOF
[$section]
rtos = "$rtos"
target = "$target"
runner_os = "$runner_os"
status = "$status"
passed = $passed
failed = $failed
total = $total

EOF

        # Per-test results
        if [ -f "$jobdir/tests_data" ]; then
            echo "[$section.tests]" >> "$output_file"
            while IFS='=' read -r name result; do
                echo "\"$name\" = \"$result\"" >> "$output_file"
            done < "$jobdir/tests_data"
            echo "" >> "$output_file"
        fi

        [ "$status" = "passed" ] || overall="failed"
        job_count=$((job_count + 1))
    done

    # No artifacts at all means something went wrong upstream.
    if [ "$job_count" -eq 0 ]; then
        overall="failed"
        echo "Warning: no test result artifacts found in $results_dir" >&2
    fi

    # Patch overall status
    sed -i.bak "s/^status = \"pending\"/status = \"$overall\"/" "$output_file"
    rm -f "${output_file}.bak"
}

# ── format-summary ───────────────────────────────────────────────────
format_summary()
{
    local toml="$1"
    local overall commit_short commit_sha repo timestamp
    overall=$(get_value summary status "$toml")
    commit_short=$(get_value summary commit_short "$toml")
    commit_sha=$(get_value summary commit_sha "$toml")
    repo=$(get_value summary repository "$toml")
    timestamp=$(get_value summary timestamp "$toml")

    local badge
    if [ "$overall" = "passed" ]; then
        badge="$sym_pass All tests passed"
    else
        badge="$sym_fail Some tests failed"
    fi

    cat << EOF
## Thread-Metric Benchmark Results

**${badge}** &mdash; [\`${commit_short}\`](https://github.com/${repo}/commit/${commit_sha}) &mdash; ${timestamp}

### Summary

| RTOS | Target | OS | Status | Passed | Failed | Total |
|------|--------|----|--------|--------|--------|-------|
EOF

    # Collect section names (skip summary, skip .tests subsections)
    local sections
    sections=$(grep -E '^\[[a-z]' "$toml" | grep -v '^\[summary' | grep -v '\.tests\]' | sed 's/\[//;s/\]//')

    for sec in $sections; do
        local rtos target runner_os status passed failed total sym
        rtos=$(get_value "$sec" rtos "$toml")
        target=$(get_value "$sec" target "$toml")
        runner_os=$(get_value "$sec" runner_os "$toml")
        status=$(get_value "$sec" status "$toml")
        passed=$(get_value "$sec" passed "$toml")
        failed=$(get_value "$sec" failed "$toml")
        total=$(get_value "$sec" total "$toml")
        sym=$(get_symbol "$status")
        echo "| ${rtos} | ${target} | ${runner_os} | ${sym} ${status} | ${passed} | ${failed} | ${total} |"
    done

    echo ""
    echo "### Per-Test Results"
    echo ""

    # Collect all unique test names across all jobs for the header.
    local all_tests=()
    for sec in $sections; do
        if get_section_keys "${sec}.tests" "$toml" > /dev/null 2>&1; then
            while IFS='=' read -r k _; do
                k=$(echo "$k" | sed 's/^"//;s/"[[:space:]]*$//')
                # Add if not already present
                local found=0
                for existing in "${all_tests[@]+"${all_tests[@]}"}"; do
                    [ "$existing" = "$k" ] && found=1 && break
                done
                [ "$found" -eq 0 ] && all_tests+=("$k")
            done < <(get_section_keys "${sec}.tests" "$toml")
        fi
    done

    if [ ${#all_tests[@]} -gt 0 ]; then
        # Build header
        printf "| Test |"
        for sec in $sections; do
            local rtos target
            rtos=$(get_value "$sec" rtos "$toml")
            target=$(get_value "$sec" target "$toml")
            printf " %s/%s |" "$rtos" "$target"
        done
        echo ""

        # Separator
        printf "%s" "|------|"
        for sec in $sections; do
            printf "%s" "------|"
        done
        echo ""

        # Rows
        for t in "${all_tests[@]}"; do
            # Pretty name: strip tm_ prefix
            local pretty="${t#tm_}"
            printf "| \`%s\` |" "$pretty"
            for sec in $sections; do
                local val
                val=$(get_value "${sec}.tests" "\"$t\"" "$toml" 2> /dev/null || echo "skipped")
                [ -z "$val" ] && val="skipped"
                local sym
                sym=$(get_symbol "$val")
                printf " %s |" "$sym"
            done
            echo ""
        done
    fi
}

# ── post-comment ─────────────────────────────────────────────────────
post_comment()
{
    local toml="$1" pr_number="$2"
    local body
    body=$(format_summary "$toml")

    local tmpfile
    tmpfile=$(mktemp)
    trap 'rm -f "$tmpfile"' RETURN
    echo "$body" > "$tmpfile"
    gh pr comment "$pr_number" --body-file "$tmpfile"
}

# ── main dispatch ────────────────────────────────────────────────────
cmd="${1:-}"
shift || true

case "$cmd" in
    collect-data)
        [ $# -ge 4 ] || {
            echo "Usage: ci-tools.sh collect-data <rtos> <target> <os> <output>"
            exit 1
        }
        collect_data "$@"
        ;;
    aggregate)
        [ $# -ge 2 ] || {
            echo "Usage: ci-tools.sh aggregate <results_dir> <output_file>"
            exit 1
        }
        aggregate "$@"
        ;;
    format-summary)
        [ $# -ge 1 ] || {
            echo "Usage: ci-tools.sh format-summary <toml_file>"
            exit 1
        }
        format_summary "$@"
        ;;
    post-comment)
        [ $# -ge 2 ] || {
            echo "Usage: ci-tools.sh post-comment <toml_file> <pr_number>"
            exit 1
        }
        post_comment "$@"
        ;;
    *)
        echo "Usage: ci-tools.sh {collect-data|aggregate|format-summary|post-comment} [args...]"
        exit 1
        ;;
esac
