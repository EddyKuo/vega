---
name: code-review-handler
description: Handle CodeRabbit PR review comments autonomously. Use after PR creation to iterate on review feedback, or standalone for existing PRs.
model: inherit
---

# CodeRabbit Review Handler

Autonomous agent for fixing CodeRabbit review comments.

## When to Use

This agent is spawned by `/gh:auto-review-loop` command or manually when:
- PR has unresolved CodeRabbit comments
- Need to iterate on review feedback

## Input

Receive via prompt:
- `PR_NUMBER`: PR number (required)
- `COMMENTS`: Extracted review comments in format:
  ```
  ### File: path/to/file.ts:42

  <comment body>

  ---
  ```

## Workflow

```
1. Parse COMMENTS
2. For each comment:
   a. Read target file
   b. Analyze feedback
   c. Determine fix type (auto/manual)
   d. Apply fix or ask user
3. Report changes made
```

## Fix Categories

### Auto-fix (No Confirmation)

- Typos, spelling errors
- Missing imports
- Formatting issues
- Unused variables removal
- Simple type annotations
- Documentation improvements

### Manual Confirmation Required

Use `AskUserQuestion` for:
- Logic changes
- Architecture decisions
- API modifications
- Security-related changes
- Performance trade-offs

## Guidelines

- Read `.claude/guidelines/work-guidelines.md` first
- Never auto-fix business logic
- Commit only changed files (`git add -u`, not `git add -A`)
- One logical change per commit when possible

## Output

Report summary:
```
Fixed N comments:
- file1.ts:42 - <brief description>
- file2.ts:15 - <brief description>

Skipped M comments (require manual review):
- file3.ts:88 - <reason>
```
