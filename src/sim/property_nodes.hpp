#pragma once

// **A JSBSim property by name, found once.** JSBSim finds a property by
// walking its path through the tree, allocating as it goes; asked for by name
// every step - the learnt landing reads seventeen, the approach autopilot as
// many again - that walk was most of a flight's time in the sanitized debug
// build. Each node is found the first time it is asked for and kept; the tree
// owns it, for as long as the tree lives.
//
// **A property the tree has not got is found absent once** too: kept with the
// deepest node of its path that is there, and how many children that node
// had. Nothing removes a node from JSBSim's tree, so the property can come
// only as a new child of that node: while it has as many children, the
// property is still not there and its path is not walked again. Every step
// asks after speedbrakes, spoilers, cooling flaps and a supercharger, and most
// models have none of them.

#include <string>
#include <unordered_map>

class SGPropertyNode;

namespace glideslope::sim {

class PropertyNodes {
public:
    // No tree yet: one is given before anything is found.
    PropertyNodes() = default;
    // The tree from `root`, which outlives this.
    explicit PropertyNodes(SGPropertyNode* root) : root_(root) {}

    // The node at `name`, a path from the root; null where there is none.
    SGPropertyNode* find(const std::string& name) const;

private:
    struct Absent {
        const SGPropertyNode* nearest;
        int children;
    };
    SGPropertyNode* root_ = nullptr;
    mutable std::unordered_map<std::string, SGPropertyNode*> found_;
    mutable std::unordered_map<std::string, Absent> absent_;
};

} // namespace glideslope::sim
