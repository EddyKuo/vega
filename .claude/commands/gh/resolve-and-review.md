---
description: Full issue resolution pipeline with automated code review
---

# Resolve and Review

Automated full pipeline from issue resolution to CodeRabbit review handling.

## Arguments

`$ARGUMENTS` = Issue number

## Pipeline

### Step 1: Issue Resolution

Spawn issue-resolver agent (blocking, wait for result):

```
Task(subagent_type="issue-resolver"):
  prompt: |
    Resolve GitHub issue #${ARGUMENTS}.

    Follow the complete workflow in your system prompt.
    End with: PR_CREATED: {number} or PR_FAILED: {reason}
```

Parse result:
- Extract PR number from `PR_CREATED: (\d+)`
- If `PR_FAILED`, report failure and **stop pipeline**

### Step 2: Code Review Loop

Spawn code-review-handler agent (blocking, wait for result):

```
Task(subagent_type="code-review-handler"):
  prompt: |
    Handle CodeRabbit reviews for PR #${PR_NUMBER}.

    End with: REVIEW_COMPLETE, REVIEW_INCOMPLETE, or REVIEW_SKIPPED
```

### Step 3: Final Report

Report to user:

```
## Pipeline Complete

- Issue: #${ARGUMENTS}
- PR: #${PR_NUMBER}
- Review Status: ${REVIEW_STATUS}
- PR URL: https://github.com/${REPO}/pull/${PR_NUMBER}
```

## Error Handling

| Stage | Error | Action |
|-------|-------|--------|
| Step 1 | PR_FAILED | Stop pipeline, report reason |
| Step 2 | REVIEW_INCOMPLETE | Continue, warn user |
| Step 2 | REVIEW_SKIPPED | Continue, note no review |

## Guidelines

- Follow `@CLAUDE.md` project conventions
- Each agent runs in independent context
- Use `/tasks` to monitor agent status
