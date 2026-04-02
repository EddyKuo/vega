---
description: Initialize and configure GitHub Project board
---

# Initialize GitHub Project

Create a GitHub Project board and configure default fields. Follow project guidelines in `@CLAUDE.md`.

## Prerequisites

The `gh` CLI requires project scope. Add it with:
```bash
gh auth refresh -s project --hostname github.com
```

## Arguments

- Project name (optional): Uses repository name as default if not provided

## Workflow

1. **Pre-check**
   - Verify project scope with `gh auth status`
   - If missing, instruct user to run `gh auth refresh -s project --hostname github.com`
   - Get owner with `gh repo view --json nameWithOwner,owner -q ".owner.login"`

2. **Check existing projects**
   - List existing projects with `gh project list --owner <owner> --format json`
   - If a project with the same name exists, use AskUserQuestion to determine action:
     - Use existing project
     - Create new project (with different name)
     - Cancel operation

3. **Create project**
   - Run `gh project create --owner <owner> --title "<project-name>" --format json`
   - Save the generated project number

4. **Verify Status field**
   - Check fields with `gh project field-list <project-number> --owner <owner> --format json`
   - GitHub Project provides default Status field (Todo, In Progress, Done)
   - Use default fields if available

5. **Create Priority field (optional)**
   - Ask user via AskUserQuestion whether to create Priority field
   - If yes, create:
     ```bash
     gh project field-create <project-number> --owner <owner> \
       --name "Priority" \
       --data-type "SINGLE_SELECT" \
       --single-select-options "High,Medium,Low"
     ```

6. **Link repository to project**
   - Get current repository name with `gh repo view --json name -q .name`
   - Link with `gh project link <project-number> --owner <owner> --repo <repo>`
   - Note: `--repo` takes only the repository name (not owner/repo format)

7. **Output results**
   - Summarize created project information
   - Provide web URL: `gh project view <project-number> --owner <owner> --web`

## Output Example

```
GitHub Project initialization complete!

Project: my-project
Number: 5
URL: https://github.com/users/<username>/projects/5

Default fields:
- Status: Todo, In Progress, Done
- Priority: High, Medium, Low (optionally added)

Linked repository: owner/repo

Next steps:
- Add issue: gh project item-add 5 --owner <owner> --url <issue-url>
- View board: gh project view 5 --owner <owner> --web
```

> See [Work Guidelines](../guidelines/work-guidelines.md)
