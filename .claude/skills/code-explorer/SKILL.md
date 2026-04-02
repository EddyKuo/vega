---
name: code-explorer
description: Search and analyze code repositories on GitHub and Hugging Face (Models, Datasets, Spaces). This skill should be used when exploring open-source projects, finding implementation references, discovering ML models/datasets, or analyzing how others have solved similar problems.
---

# Code Explorer

## Overview

To search and analyze code repositories across GitHub and Hugging Face platforms, use this skill. It enables discovering implementation patterns, finding relevant ML models/datasets, and exploring demo applications (Spaces) for learning and reference.

## Supported Platforms

| Platform | Search Targets | Tool |
|----------|---------------|------|
| **GitHub** | Repositories, Code | `gh` CLI |
| **Hugging Face** | Models, Datasets, Spaces | `uvx hf` CLI + `huggingface_hub` API |

## Search Quality Principles (Required)

### 1. Verify Current Date

```bash
date +%Y-%m-%d
```
Years in examples below are for reference. Use **current year** in actual searches.

### 2. Natural Query Expansion

Transform simple keywords into queries that reflect your actual intent:

| Intent | Query Style | Example |
|--------|-------------|---------|
| **Find implementation** | "how to" + task + framework | `"how to run object detection inference in pytorch"` |
| **Compare options** | "best" / "vs" / "comparison" | `"YOLOv8 vs RT-DETR accuracy comparison {current_year}"` |
| **Learn concept** | "tutorial" / "explained" / "example" | `"vision transformer attention mechanism tutorial with code"` |
| **Find demo** | task + "demo" / "app" + platform | `"object detection gradio demo huggingface spaces"` |
| **Troubleshoot** | error message / "why" / "fix" | `"CUDA out of memory error batch size 1 pytorch"` |

Write queries as you would ask a person, not as keyword lists.

### 3. Apply Multi-Query

If hard to find at once, search from **2-3 perspectives**:

```bash
# Model name focus
gh search repos "qwen2-vl" --sort stars

# Feature focus
gh search repos "vision language open vocabulary detection" --sort stars

# Implementation focus
gh search repos "vl model gradio demo inference" --sort stars
```

### 4. Use Filters

```bash
# Recent + quality filter (apply current year)
gh search repos "keyword" stars:>50 pushed:>{current_year-1}-01-01 --language python
```

### 5. Pre-Search Checklist

- [ ] Verified current date?
- [ ] Formulated natural query?
- [ ] Searched with 2-3 query variations if needed?
- [ ] Applied appropriate filters (language, stars, date)?

---

## Workflow Decision Tree

```
User wants to explore code/resources
    |
    +-- Looking for code implementations?
    |   +-- Use GitHub search
    |       +-- scripts/search_github.py (or gh CLI directly)
    |       +-- Analyze repo structure, README, key files
    |
    +-- Looking for ML resources?
        +-- Use Hugging Face search
            +-- scripts/search_huggingface.py (search via API)
            +-- uvx hf download (download files)
```

## Scripts

**Always run scripts with `--help` first** to see usage. These scripts handle common search workflows reliably.

### Available Scripts

- `scripts/search_github.py` - GitHub repository search using gh CLI
- `scripts/search_huggingface.py` - Hugging Face search (models, datasets, spaces)

### Quick Examples

```bash
# GitHub search
python scripts/search_github.py "object detection" --limit 10 --help

# Hugging Face search
python scripts/search_huggingface.py "qwen vl" --type models --help
```

## GitHub Search

### Finding Starter Templates

Search for boilerplates, starters, and scaffolding projects to jumpstart development:

```bash
# Template/boilerplate search
gh search repos "fastapi boilerplate production ready" --sort stars
gh search repos "react typescript starter template" --sort stars
gh search repos "pytorch lightning project template" --sort stars

# Cookiecutter templates (Python ecosystem)
gh search repos "cookiecutter ml project" --sort stars
gh search repos "cookiecutter fastapi" --sort stars
```

| Keyword | What You Get |
|---------|--------------|
| `boilerplate` | Production-ready project structure with best practices |
| `starter`, `starter-kit` | Minimal setup to get running quickly |
| `template` | Reusable project scaffolding |
| `scaffold` | Code generation base |
| `cookiecutter` | Python templating system projects |

### Curated Lists (awesome-*)

`awesome-*` repositories are community-curated lists of high-quality resources. Start here before deep-diving:

```bash
# Find curated lists
gh search repos "awesome object-detection" --sort stars
gh search repos "awesome vision-language" --sort stars
gh search repos "awesome gradio" --sort stars

# Check freshness (avoid outdated lists)
gh search repos "awesome pytorch" --sort updated
```

**Why use awesome lists:**
- Pre-vetted quality (community-curated)
- Categorized by use case (models, tools, tutorials, papers)
- Often includes resources not easily found via search
- Good entry point before targeted searching

**Tip:** After finding an awesome list, navigate to specific sections for your use case rather than browsing the entire list.

### Using gh CLI Directly

```bash
# Search repositories by keyword
gh search repos "open vocabulary detection" --sort stars --limit 10

# Filter by language
gh search repos "gradio app" --language python --limit 5

# View repository details
gh repo view owner/repo

# Search code within repositories
gh search code "Qwen2VL" --extension py
```

### Repository Analysis

To analyze a found repository:

1. Review README.md for usage instructions
2. Identify main entry points (app.py, main.py, inference.py)
3. Check dependencies (requirements.txt, pyproject.toml)
4. Study implementation patterns in source files

## Hugging Face Search

### Search (via script or Python API)

```bash
# Search models
python scripts/search_huggingface.py "object detection" --type models --limit 10

# Search datasets
python scripts/search_huggingface.py "coco" --type datasets --limit 5

# Search spaces (demos)
python scripts/search_huggingface.py "gradio demo" --type spaces --limit 10

# Search all types
python scripts/search_huggingface.py "qwen vl" --type all
```

### Download (via uvx hf)

```bash
# Download space source code (use /tmp/ for temporary analysis)
uvx hf download <space_id> --repo-type space --include "*.py" --local-dir /tmp/<space_name>

# Download model files
uvx hf download <repo_id> --include "*.json" --local-dir /tmp/<model_name>

# Download to project directory (when needed permanently)
uvx hf download <repo_id> --local-dir ./my-model
```

**Note**: Always use `--local-dir /tmp/` for temporary code analysis to avoid cluttering the project.

### Common Search Patterns

```bash
# Find models for specific task
python scripts/search_huggingface.py "open vocabulary detection" --type models
python scripts/search_huggingface.py "qwen2 vl" --type models
python scripts/search_huggingface.py "grounding dino" --type models

# Find demo applications
python scripts/search_huggingface.py "object detection demo" --type spaces
python scripts/search_huggingface.py "gradio image" --type spaces
```

### Analyzing a Space

To understand how a Space is implemented:

1. Find the space: `python scripts/search_huggingface.py "keyword" --type spaces`
2. Download source: `uvx hf download <space_id> --repo-type space --include "*.py" --include "requirements.txt" --local-dir /tmp/<space_name>`
3. Or view online: `https://huggingface.co/spaces/{space_id}/tree/main`
4. Focus on `app.py` for main logic
5. Check `requirements.txt` for dependencies

## Example Use Cases

### Find Qwen3-VL Open Vocab Detection Code

```bash
# Search GitHub
gh search repos "qwen vl object detection" --sort stars
gh search code "Qwen2VL" --extension py

# Search Hugging Face
python scripts/search_huggingface.py "qwen2-vl" --type models
python scripts/search_huggingface.py "qwen vl" --type spaces
```

### Find Gradio Demo Patterns

```bash
# Search spaces using Gradio
python scripts/search_huggingface.py "gradio object detection" --type spaces

# Download a space to study
uvx hf download username/space-name --repo-type space --include "*.py" --local-dir /tmp/space-name
```

### Find Pre-trained Detection Models

```bash
python scripts/search_huggingface.py "object-detection" --type models --limit 20
python scripts/search_huggingface.py "grounding-dino" --type models
python scripts/search_huggingface.py "yolo-world" --type models
```

## Resources

### scripts/
- `search_github.py` - GitHub repository search wrapper
- `search_huggingface.py` - Hugging Face Hub search wrapper

### references/
- `github_api.md` - GitHub CLI detailed reference
- `huggingface_api.md` - Hugging Face Hub API and CLI reference

## Tips

1. **Start broad, then narrow**: Begin with general keywords, then add filters
2. **Check stars/likes**: Higher counts often indicate quality
3. **Review recent activity**: Recently updated repos are better maintained
4. **Use --help first**: Scripts have detailed usage information
5. **Download selectively**: Use `uvx hf download --include` to download only needed files
6. **Always cite sources**: Include repository URLs, Space links, or model IDs you referenced
