---
description: Automatically fix CodeRabbit PR review comments until resolved
---

# Auto PR Review Loop

Automatically fix and push until CodeRabbit review comments reach zero.

## Arguments

`$ARGUMENTS` parsing:
- **No arguments**: Use current branch's PR
- **PR number**: `/gh:auto-review-loop 123` - Specify PR

## Safety Limits

| Limit | Value | Purpose |
|-------|-------|---------|
| MAX_ITERATIONS | 10 | Prevent infinite loops |
| POLL_TIMEOUT | 300s | Wait for checks to complete |
| POLL_INTERVAL | 30s | Status check frequency |

## Workflow

```
┌───────────────────────────────────────────────────────────────┐
│  1. Init                                                      │
│       ↓                                                       │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │  2. Wait for All Checks (gh pr checks)                  │  │
│  │       ↓                                                 │  │
│  │  3. Check Unresolved Threads (GraphQL)                  │  │
│  │       ↓                                                 │  │
│  │  4. Fix, Commit, Push                                   │  │
│  │       ↓                                                 │  │
│  │  [loop back to 2 until resolved or max iterations]      │  │
│  └─────────────────────────────────────────────────────────┘  │
│       ↓                                                       │
│  5. Output result                                             │
└───────────────────────────────────────────────────────────────┘
```

### Step 1: Initialize

```bash
PR_NUMBER=${ARGUMENTS:-$(gh pr view --json number -q .number 2>/dev/null)}
REPO=$(gh repo view --json nameWithOwner -q .nameWithOwner)
ITERATION=1
MAX_ITERATIONS=10
```

### Step 2: Wait for All Checks

Check CodeRabbit and Actions status at once with `gh pr checks`.

```bash
wait_for_all_checks() {
  local elapsed=0

  while [ $elapsed -lt 300 ]; do
    CHECKS=$(gh pr checks $PR_NUMBER 2>/dev/null)

    # pending/in_progress 있으면 대기
    if echo "$CHECKS" | grep -qiE "pending|in_progress|running"; then
      sleep 30
      elapsed=$((elapsed + 30))
      continue
    fi

    # fail 있으면 에러 (CodeRabbit fail은 제외 - 리뷰 코멘트가 있다는 의미)
    if echo "$CHECKS" | grep -v "CodeRabbit" | grep -qi "fail"; then
      return 2  # CI failed
    fi

    return 0  # All checks completed
  done

  return 1  # Timeout
}
```

### Step 3: Check Unresolved Threads (GraphQL)

Check unresolved CodeRabbit review comment count. This is the ground truth.

```bash
check_unresolved_threads() {
  RESULT=$(gh api graphql -f query='query {
    repository(owner: "'"${REPO%/*}"'", name: "'"${REPO#*/}"'") {
      pullRequest(number: '"$PR_NUMBER"') {
        reviewThreads(first: 100) {
          nodes {
            isResolved
            path
            line
            comments(first: 1) {
              nodes {
                author { login }
                body
              }
            }
          }
        }
      }
    }
  }')

  # Filter: unresolved + coderabbit
  ALL_UNRESOLVED=$(echo "$RESULT" | jq '[.data.repository.pullRequest.reviewThreads.nodes[] |
    select(.isResolved == false) |
    select(.comments.nodes[0].author.login | ascii_downcase | contains("coderabbit"))]')

  # Exclude nitpick and minor for auto-fix
  UNRESOLVED=$(echo "$ALL_UNRESOLVED" | jq '[.[] |
    select(.comments.nodes[0].body |
      (contains("[nitpick]") or contains("nitpick") or contains("[minor]")) | not)]')

  UNRESOLVED_COUNT=$(echo "$UNRESOLVED" | jq 'length')

  # Count skipped issues (nitpick + minor)
  SKIPPED_COUNT=$(($(echo "$ALL_UNRESOLVED" | jq 'length') - UNRESOLVED_COUNT))

  # Extract comments for fixing
  COMMENTS=$(echo "$UNRESOLVED" | jq -r '.[] |
    "### File: \(.path):\(.line)\n\n\(.comments.nodes[0].body)\n\n---\n"')
}
```

### Step 4: Fix, Commit, Push

```bash
fix_and_push() {
  # For each comment: analyze and fix
  # Follow code-review.md guidelines for auto-fix vs manual

  # After fixes applied:
  if [ -z "$(git status --porcelain)" ]; then
    return 1  # No changes made
  fi

  git add -u
  git commit -m "fix: address CodeRabbit review (iteration $ITERATION)"
  git push
}
```

### Main Loop

```bash
while [ $ITERATION -le $MAX_ITERATIONS ]; do
  # Step 2: Wait for all checks
  wait_for_all_checks
  WAIT_RESULT=$?

  if [ $WAIT_RESULT -eq 1 ]; then
    echo "REVIEW_SKIPPED: checks not completing within timeout"
    exit 0
  elif [ $WAIT_RESULT -eq 2 ]; then
    echo "REVIEW_BLOCKED: CI checks failed"
    exit 1
  fi

  # Step 3: Check unresolved threads
  check_unresolved_threads

  if [ "$UNRESOLVED_COUNT" -eq 0 ]; then
    if [ "$SKIPPED_COUNT" -gt 0 ]; then
      echo "REVIEW_COMPLETE: $SKIPPED_COUNT minor/nitpick issue(s) skipped - manual review recommended"
    else
      echo "REVIEW_COMPLETE: all comments resolved"
    fi
    exit 0
  fi

  # Step 4: Fix and push
  # ... fix logic here using COMMENTS ...

  fix_and_push || {
    echo "REVIEW_COMPLETE: no changes needed"
    exit 0
  }

  ITERATION=$((ITERATION + 1))
done

echo "REVIEW_INCOMPLETE: max iterations ($MAX_ITERATIONS) reached, $UNRESOLVED_COUNT comments remaining"
exit 1
```

## Output Format

End with exactly one of:

| Output | Meaning |
|--------|---------|
| `REVIEW_COMPLETE: all comments resolved` | Success |
| `REVIEW_COMPLETE: N minor/nitpick issue(s) skipped - manual review recommended` | Only minor/nitpick issues remain |
| `REVIEW_COMPLETE: no changes needed` | No fixes required |
| `REVIEW_INCOMPLETE: max iterations reached, N comments remaining` | Hit limit |
| `REVIEW_SKIPPED: checks not completing within timeout` | Timeout |
| `REVIEW_BLOCKED: CI checks failed` | CI failure |

## Fix Guidelines

Read `.claude/commands/code-review.md` for:
- Auto-fix criteria (typos, imports, formatting)
- Manual confirmation criteria (logic changes, architecture)
- Never auto-fix business logic

## Error Handling

| Error | Response |
|-------|----------|
| PR not found | Exit with error |
| API rate limit | Increase interval, retry |
| Git push failed | Report and stop |
| CI checks failed | Exit with REVIEW_BLOCKED |
