#include "sim/property_nodes.hpp"

#include <simgear/props/props.hxx>

namespace glideslope::sim {

SGPropertyNode* PropertyNodes::find(const std::string& name) const {
    if (const auto found = found_.find(name); found != found_.end()) {
        return found->second;
    }
    const auto absent = absent_.find(name);
    if (absent != absent_.end() &&
        absent->second.nearest->nChildren() == absent->second.children) {
        return nullptr;
    }
    if (SGPropertyNode* n = root_->getNode(name)) {
        found_.emplace(name, n);
        if (absent != absent_.end()) {
            absent_.erase(absent);
        }
        return n;
    }
    // The deepest node of its path that is there: the root, if none is.
    const SGPropertyNode* nearest = root_;
    for (std::size_t end = name.rfind('/'); end != std::string::npos && end > 0;
         end = name.rfind('/', end - 1)) {
        if (const SGPropertyNode* up = root_->getNode(name.substr(0, end))) {
            nearest = up;
            break;
        }
    }
    absent_.insert_or_assign(name, Absent{nearest, nearest->nChildren()});
    return nullptr;
}

} // namespace glideslope::sim
