#pragma once
#include "pipeline/EditRecipe.h"
#include "pipeline/IProcessNode.h"
#include <vector>
#include <string>

namespace vega {

struct EditCommand {
    std::string description;  // e.g., "Exposure +1.3", "White Balance"
    EditRecipe before;
    EditRecipe after;
    PipelineStage affected_stage;
};

class EditHistory {
public:
    void push(EditCommand cmd);

    bool canUndo() const;
    bool canRedo() const;

    // Returns the recipe to apply after undo/redo
    const EditRecipe& undo();
    const EditRecipe& redo();

    // Get current position info for UI display
    int currentIndex() const { return current_; }
    int totalEntries() const { return static_cast<int>(stack_.size()); }

    // Clear all history
    void clear();

private:
    std::vector<EditCommand> stack_;
    int current_ = -1;
    static constexpr int MAX_HISTORY = 200;
};

} // namespace vega
