#!/bin/bash
# UserPromptSubmit Hook: Auto-inject guidelines on every prompt
# Location: .claude/hooks/inject-guidelines.sh

set -e

# Project directory (provided by Claude Code)
PROJECT_DIR="${CLAUDE_PROJECT_DIR:-$(pwd)}"
GUIDELINES_DIR="$PROJECT_DIR/.claude/guidelines"

# Primary guideline file to always inject
PRIMARY_GUIDELINE="$GUIDELINES_DIR/work-guidelines.md"
RELATIVE_GUIDELINE=".claude/guidelines/work-guidelines.md"

# Output as system-reminder for Claude to process
if [ -f "$PRIMARY_GUIDELINE" ]; then
  echo "<system-reminder>"
  echo "Called the Read tool with the following input: {\"file_path\":\"$RELATIVE_GUIDELINE\"}"
  echo "</system-reminder>"
  echo "<system-reminder>"
  echo "Result of calling the Read tool:"
  cat "$PRIMARY_GUIDELINE"
  echo "</system-reminder>"
fi

# Uncomment below to inject ALL guidelines in the directory
# if [ -d "$GUIDELINES_DIR" ]; then
#   for file in "$GUIDELINES_DIR"/*.md; do
#     if [ -f "$file" ]; then
#       echo "<system-reminder>"
#       echo "### $(basename "$file")"
#       cat "$file"
#       echo "</system-reminder>"
#     fi
#   done
# fi

exit 0
