#pragma once
#include <memory>
#include <vector>
#include <functional>

namespace detail
{
    template<typename T, typename Alloc, typename... Ts>
    T* construct(Alloc alloc, Ts&&... ts)
    {
        T* ptr = alloc.allocate(1);
        return new (ptr) T(std::forward<Ts>(ts)...); // cannot use std::construct_at as ctor is private
    }

    template<typename T, typename Alloc>
    void destroy(Alloc alloc, T* ptr)
    {
        std::destroy_at(ptr);
        alloc.deallocate(ptr, 1);
    }

    template<typename T, typename Alloc, typename... Ts>
    auto allocate_unique(Alloc alloc, Ts&&... ts)
    {
        return std::unique_ptr<T, std::function<void(T*)>>(construct<T>(alloc, std::forward<Ts>(ts)...), [alloc](T* ptr) { destroy(alloc, ptr); });
    }
}

template<typename V, typename E, typename EdgeEqual = std::equal_to<>, typename AllocE = std::allocator<E>>
class tree_graph
{
    EdgeEqual edge_equal;
    AllocE alloc;

public:
    tree_graph(EdgeEqual edge_equal, AllocE alloc)
        : edge_equal(edge_equal), alloc(alloc)
        , dummy_root(*this, V{})
    {
    }

    explicit tree_graph(EdgeEqual edge_equal)
        : tree_graph(edge_equal, AllocE{})
    {
    }

    explicit tree_graph(AllocE alloc)
        : tree_graph(EdgeEqual{}, alloc)
    {
    }

    tree_graph()
        : tree_graph(EdgeEqual{}, AllocE{})
    {
    }

    class graph_vertex
    {
        template<typename T, typename Alloc, typename... Ts>
        friend T* detail::construct(Alloc, Ts&&...);

        friend tree_graph;
        tree_graph& graph;

        using graph_vertex_ptr = std::unique_ptr<graph_vertex, std::function<void(graph_vertex*)>>;
        using AllocV = typename std::allocator_traits<AllocE>::template rebind_alloc<graph_vertex_ptr>;

        std::vector<graph_vertex_ptr, AllocV> children;
        graph_vertex* father; // ToDo: copying/moving father vertex will break this ptr

    public:
        V value;

    private:
        std::vector<E, AllocE> edges;

        graph_vertex(tree_graph& graph, V vertex, graph_vertex* father = nullptr)
            : graph(graph), children(graph.alloc), father(father), value(vertex), edges(graph.alloc)
        {
        }

        template<typename Vertex>
        graph_vertex_ptr create_edge(Vertex&& new_vertex)
        {
            using AllocGV = typename std::allocator_traits<AllocV>::template rebind_alloc<graph_vertex>;
            return detail::allocate_unique<graph_vertex, AllocGV>(graph.alloc, graph, std::forward<Vertex>(new_vertex), this);
        }

    public:
        template<typename Edge, typename Vertex>
        graph_vertex& add_edge(Edge&& new_edge, Vertex&& new_vertex)
        {
            // Check if the edge is already there
            for (std::size_t i = 0; i < edges.size(); ++i)
            {
                if (graph.edge_equal(edges[i], new_edge))
                {
                    // children[i]->value = std::forward<Vertex>(new_vertex);
                    return *children[i];
                }
            }

            // Add new edge
            edges.push_back(std::forward<Edge>(new_edge));
            children.push_back(create_edge(std::forward<Vertex>(new_vertex)));
            return *children.back();
        }

        [[nodiscard]] graph_vertex* previous_vertex() const
        {
            return father;
        }

        [[nodiscard]] std::size_t edges_size() const
        {
            return edges.size();
        }

        [[nodiscard]] const E& get_edge(std::size_t index) const
        {
            return edges[index];
        }

        [[nodiscard]] const graph_vertex& next_vertex(std::size_t index) const
        {
            return *children[index];
        }
    };

    graph_vertex& root()
    {
        return dummy_root;
    }

private:
    graph_vertex dummy_root;
};

namespace pmr
{
    template<typename V, typename E, typename EdgeEqual = std::equal_to<>>
    using tree_graph = tree_graph<V, E, EdgeEqual, std::pmr::polymorphic_allocator<E>>;
}
