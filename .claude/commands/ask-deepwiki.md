---
description: Deep query on GitHub repositories using DeepWiki
---

# DeepWiki Repository Query

Query GitHub repositories in-depth using the DeepWiki MCP to get comprehensive answers.

## Arguments

`$ARGUMENTS` parsing:
- Format: `owner/repo "question"` or `owner/repo question text`
- Repository: Extract `owner/repo` pattern (before first space or quote)
- Question: Remaining text after repository

Examples:
- `/ask-deepwiki facebook/react "How does the reconciliation algorithm work?"`
- `/ask-deepwiki vercel/next.js explain the app router architecture`

## Execution

### 1. Parse Arguments

```
Input: $ARGUMENTS
Extract:
  - REPO_NAME: owner/repo format
  - QUESTION: remaining text
```

### 2. Query DeepWiki

```
mcp__deepwiki__ask_question({
  repoName: [REPO_NAME],
  question: [QUESTION]
})
```

### 3. Multi-Query Expansion (if needed)

If initial response is insufficient:
1. Decompose into sub-questions
2. Query in parallel using multiple `mcp__deepwiki__ask_question` calls
3. Synthesize results

## Error Handling

| Error | Action |
|-------|--------|
| Invalid repo format | Request correct `owner/repo` format |
| Repository not found | Verify repository exists on GitHub |
| Empty question | Request specific question |
| DeepWiki unavailable | Suggest alternative (direct GitHub exploration) |

## Guidelines

- Follow CLAUDE.md project guidelines
- Write clear, specific questions
- Iterate and refine as needed

