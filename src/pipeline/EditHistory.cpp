#include "pipeline/EditHistory.h"
#include "core/Logger.h"
#include <cassert>

namespace vega {

void EditHistory::push(EditCommand cmd)
{
    if (current_ + 1 < static_cast<int>(stack_.size()))
    {
        int discarded = static_cast<int>(stack_.size()) - (current_ + 1);
        stack_.erase(stack_.begin() + (current_ + 1), stack_.end());
        VEGA_LOG_DEBUG("EditHistory: discarded {} redo entries (new branch)", discarded);
    }

    VEGA_LOG_INFO("EditHistory: push '{}' [{}/{}]",
        cmd.description, current_ + 2, static_cast<int>(stack_.size()) + 1);

    stack_.push_back(std::move(cmd));
    current_ = static_cast<int>(stack_.size()) - 1;

    if (static_cast<int>(stack_.size()) > MAX_HISTORY)
    {
        int excess = static_cast<int>(stack_.size()) - MAX_HISTORY;
        stack_.erase(stack_.begin(), stack_.begin() + excess);
        current_ -= excess;
        VEGA_LOG_DEBUG("EditHistory: trimmed {} oldest entries", excess);
    }
}

bool EditHistory::canUndo() const { return current_ >= 0; }
bool EditHistory::canRedo() const { return current_ + 1 < static_cast<int>(stack_.size()); }

const EditRecipe& EditHistory::undo()
{
    assert(canUndo());
    const EditRecipe& recipe = stack_[current_].before;
    VEGA_LOG_INFO("EditHistory: undo '{}' [{}/{}]",
        stack_[current_].description, current_, static_cast<int>(stack_.size()));
    --current_;
    return recipe;
}

const EditRecipe& EditHistory::redo()
{
    assert(canRedo());
    ++current_;
    VEGA_LOG_INFO("EditHistory: redo '{}' [{}/{}]",
        stack_[current_].description, current_ + 1, static_cast<int>(stack_.size()));
    return stack_[current_].after;
}

void EditHistory::clear()
{
    VEGA_LOG_DEBUG("EditHistory: cleared ({} entries)", stack_.size());
    stack_.clear();
    current_ = -1;
}

} // namespace vega
