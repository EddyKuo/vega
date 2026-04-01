#include "pipeline/EditHistory.h"
#include <cassert>

namespace vega {

void EditHistory::push(EditCommand cmd)
{
    // If we're in the middle of the stack (user undid some actions then made
    // a new edit), discard everything after the current position. This creates
    // a new branch of history.
    if (current_ + 1 < static_cast<int>(stack_.size()))
    {
        stack_.erase(stack_.begin() + (current_ + 1), stack_.end());
    }

    stack_.push_back(std::move(cmd));
    current_ = static_cast<int>(stack_.size()) - 1;

    // Trim oldest entries if we exceed the maximum history size.
    if (static_cast<int>(stack_.size()) > MAX_HISTORY)
    {
        int excess = static_cast<int>(stack_.size()) - MAX_HISTORY;
        stack_.erase(stack_.begin(), stack_.begin() + excess);
        current_ -= excess;
    }
}

bool EditHistory::canUndo() const
{
    return current_ >= 0;
}

bool EditHistory::canRedo() const
{
    return current_ + 1 < static_cast<int>(stack_.size());
}

const EditRecipe& EditHistory::undo()
{
    assert(canUndo() && "undo() called with nothing to undo");
    const EditRecipe& recipe = stack_[current_].before;
    --current_;
    return recipe;
}

const EditRecipe& EditHistory::redo()
{
    assert(canRedo() && "redo() called with nothing to redo");
    ++current_;
    return stack_[current_].after;
}

void EditHistory::clear()
{
    stack_.clear();
    current_ = -1;
}

} // namespace vega
