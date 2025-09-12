#pragma once

#include "../trace.hpp"

#include "../../utils/tree_graph.hpp"

#include <iostream>

namespace tree_pruners
{
    struct randomthset
    {
        static void prune_tree(pmr::tree_graph<int, past_trace_item>& graph, std::wofstream& extra_log)
        {
            std::srand(std::time(0));

            randomly_prune_subtree(graph.root(), extra_log);
        }

        static int randomly_prune_subtree(pmr::tree_graph<int, past_trace_item>::graph_vertex& vertex, std::wofstream& extra_log)
        {
            std::size_t reduced_by_total = 0;
           
            if (vertex.edges_size() > 0)
            {
                std::size_t reduced_by_nested = 0;

                for (std::size_t i = 0; i < vertex.edges_size(); ++i)
                {
                    reduced_by_nested += randomly_prune_subtree(vertex.next_vertex(i), extra_log);
                }

                vertex.value -= reduced_by_nested;

                reduced_by_total += reduced_by_nested;
            }

            if (vertex.value > 0)
            {
                // here we need to reduce by considering only the number of remaining unexplored threads (choices) at this scheduling point
                // options_size is the same for all edges

                if (vertex.edges_size() > 0)
                {
                    std::size_t remaining_threads_count = std::max(0, static_cast<int>(vertex.get_edge(0).options_size - vertex.edges_size()));

                    std::size_t reduce_for_current = std::rand() % (remaining_threads_count + 1);

                    vertex.value -= reduce_for_current;

                    reduced_by_total += reduce_for_current;
                }
            }
             
            // we do not have to modify vertex.options_size here

            return reduced_by_total;
        }
    };
}
