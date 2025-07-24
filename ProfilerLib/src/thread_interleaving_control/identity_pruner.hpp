#pragma once

#include "trace.hpp"

#include "../utils/tree_graph.hpp"

#include <iostream>

namespace tree_pruners
{
    struct identity
    {
        static void prune_tree(pmr::tree_graph<int, past_trace_item>& graph, std::wofstream& extra_log)
        {
            // do nothing
            extra_log << L"[TreePruner] No modifications to the tree" << std::endl;
        }
    };
}
