#include "lifetree.h"

#include <algorithm>
#include <queue>
#include <set>
#include <sstream>
#include <stack>

namespace lifetree {

namespace {

void setError(std::string *error, const std::string &message) {
  if (error != nullptr) {
    *error = message;
  }
}

std::string join(const std::vector<std::string> &items) {
  std::ostringstream stream;
  for (std::size_t index = 0; index < items.size(); ++index) {
    if (index != 0) {
      stream << ", ";
    }
    stream << items[index];
  }
  return stream.str();
}

template <typename T>
std::vector<T> toSortedVector(const std::unordered_set<T> &input) {
  std::vector<T> values(input.begin(), input.end());
  std::sort(values.begin(), values.end());
  return values;
}

template <typename T>
std::vector<T> toSortedVector(const std::set<T> &input) {
  return {input.begin(), input.end()};
}

} // namespace

bool LifeTree::addModule(const std::string &name, std::string *error) {
  if (name.empty()) {
    setError(error, "module name cannot be empty");
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  if (nodes_.find(name) != nodes_.end()) {
    setError(error, "module already exists: " + name);
    return false;
  }

  nodes_.emplace(name, Node{name, {}, {}});
  return true;
}

bool LifeTree::addDependency(const std::string &from, const std::string &to, std::string *error) {
  if (from.empty() || to.empty()) {
    setError(error, "dependency endpoints cannot be empty");
    return false;
  }
  if (from == to) {
    setError(error, "self-dependency is not allowed: " + from);
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  auto fromIt = nodes_.find(from);
  if (fromIt == nodes_.end()) {
    setError(error, "source module does not exist: " + from);
    return false;
  }

  auto toIt = nodes_.find(to);
  if (toIt == nodes_.end()) {
    setError(error, "target module does not exist: " + to);
    return false;
  }

  if (fromIt->second.Dependencies.find(to) != fromIt->second.Dependencies.end()) {
    setError(error, "dependency already exists: " + from + " -> " + to);
    return false;
  }

  if (wouldCreateCycleUnlocked(from, to)) {
    setError(error, "adding dependency would create a cycle: " + from + " -> " + to);
    return false;
  }

  fromIt->second.Dependencies.insert(to);
  toIt->second.Dependents.insert(from);
  return true;
}

bool LifeTree::removeDependency(const std::string &from, const std::string &to, std::string *error) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto fromIt = nodes_.find(from);
  auto toIt = nodes_.find(to);
  if (fromIt == nodes_.end() || toIt == nodes_.end()) {
    setError(error, "cannot remove dependency between unknown modules");
    return false;
  }

  auto dependencyIt = fromIt->second.Dependencies.find(to);
  if (dependencyIt == fromIt->second.Dependencies.end()) {
    setError(error, "dependency does not exist: " + from + " -> " + to);
    return false;
  }

  fromIt->second.Dependencies.erase(dependencyIt);
  toIt->second.Dependents.erase(from);
  return true;
}

bool LifeTree::canSafelyDelete(const std::string &name,
                               std::vector<std::string> *blockers,
                               std::string *error) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto nodeIt = nodes_.find(name);
  if (nodeIt == nodes_.end()) {
    setError(error, "module does not exist: " + name);
    return false;
  }

  if (blockers != nullptr) {
    *blockers = toSortedVector(nodeIt->second.Dependents);
  }
  return nodeIt->second.Dependents.empty();
}

bool LifeTree::deleteModule(const std::string &name, std::string *error) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto nodeIt = nodes_.find(name);
  if (nodeIt == nodes_.end()) {
    setError(error, "module does not exist: " + name);
    return false;
  }

  if (!nodeIt->second.Dependents.empty()) {
    const auto blockers = toSortedVector(nodeIt->second.Dependents);
    setError(error, "cannot delete module " + name + "; active dependents: " + join(blockers));
    return false;
  }

  for (const auto &dependency : nodeIt->second.Dependencies) {
    auto dependencyIt = nodes_.find(dependency);
    if (dependencyIt != nodes_.end()) {
      dependencyIt->second.Dependents.erase(name);
    }
  }

  nodes_.erase(nodeIt);
  return true;
}

bool LifeTree::forceDeleteWithCascade(const std::string &name,
                                      std::vector<std::string> *deleted,
                                      std::string *error) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<std::string> order;
  if (!computeCascadeDeletionOrderUnlocked(name, &order, error)) {
    return false;
  }

  for (const auto &nodeName : order) {
    auto nodeIt = nodes_.find(nodeName);
    if (nodeIt == nodes_.end()) {
      continue;
    }

    for (const auto &dependency : nodeIt->second.Dependencies) {
      auto dependencyIt = nodes_.find(dependency);
      if (dependencyIt != nodes_.end()) {
        dependencyIt->second.Dependents.erase(nodeName);
      }
    }
    nodes_.erase(nodeIt);
  }

  if (deleted != nullptr) {
    *deleted = order;
  }
  return true;
}

std::vector<std::string> LifeTree::topologicalOrder(std::string *error) const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::unordered_map<std::string, int> indegrees;
  for (const auto &[name, node] : nodes_) {
    indegrees.emplace(name, static_cast<int>(node.Dependencies.size()));
  }

  std::queue<std::string> ready;
  for (const auto &[name, indegree] : indegrees) {
    if (indegree == 0) {
      ready.push(name);
    }
  }

  std::vector<std::string> order;
  order.reserve(nodes_.size());
  while (!ready.empty()) {
    auto current = ready.front();
    ready.pop();
    order.push_back(current);

    auto nodeIt = nodes_.find(current);
    if (nodeIt == nodes_.end()) {
      continue;
    }

    for (const auto &dependent : nodeIt->second.Dependents) {
      auto indegreeIt = indegrees.find(dependent);
      if (indegreeIt == indegrees.end()) {
        continue;
      }
      --indegreeIt->second;
      if (indegreeIt->second == 0) {
        ready.push(dependent);
      }
    }
  }

  if (order.size() != nodes_.size()) {
    setError(error, "topological order failed: graph contains a cycle");
    return {};
  }

  return order;
}

std::vector<std::string> LifeTree::getDependencies(const std::string &name, std::string *error) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto nodeIt = nodes_.find(name);
  if (nodeIt == nodes_.end()) {
    setError(error, "module does not exist: " + name);
    return {};
  }
  return toSortedVector(nodeIt->second.Dependencies);
}

std::vector<std::string> LifeTree::getDependents(const std::string &name, std::string *error) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto nodeIt = nodes_.find(name);
  if (nodeIt == nodes_.end()) {
    setError(error, "module does not exist: " + name);
    return {};
  }
  return toSortedVector(nodeIt->second.Dependents);
}

std::vector<std::string> LifeTree::transitiveDependencies(const std::string &name, std::string *error) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!moduleExistsUnlocked(name)) {
    setError(error, "module does not exist: " + name);
    return {};
  }
  return traverseUnlocked(name, true);
}

std::vector<std::string> LifeTree::transitiveDependents(const std::string &name, std::string *error) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!moduleExistsUnlocked(name)) {
    setError(error, "module does not exist: " + name);
    return {};
  }
  return traverseUnlocked(name, false);
}

bool LifeTree::analyzeDelete(const std::string &name, DeleteAnalysis *analysis, std::string *error) const {
  if (analysis == nullptr) {
    setError(error, "analysis output cannot be null");
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  auto nodeIt = nodes_.find(name);
  if (nodeIt == nodes_.end()) {
    setError(error, "module does not exist: " + name);
    return false;
  }

  analysis->DirectDependents = toSortedVector(nodeIt->second.Dependents);
  analysis->TransitiveDependents = traverseUnlocked(name, false);
  analysis->CanSafelyDelete = analysis->DirectDependents.empty();

  std::vector<std::string> order;
  std::string localError;
  if (!computeCascadeDeletionOrderUnlocked(name, &order, &localError)) {
    setError(error, localError);
    return false;
  }
  analysis->SuggestedCascadeOrder = std::move(order);
  return true;
}

bool LifeTree::validateInvariants(std::string *error) const {
  std::lock_guard<std::mutex> lock(mutex_);

  for (const auto &[name, node] : nodes_) {
    if (node.Name != name) {
      setError(error, "node key/name mismatch: " + name + " vs " + node.Name);
      return false;
    }

    for (const auto &dependency : node.Dependencies) {
      auto dependencyIt = nodes_.find(dependency);
      if (dependencyIt == nodes_.end()) {
        setError(error, "missing dependency node: " + dependency + " referenced by " + name);
        return false;
      }
      if (dependencyIt->second.Dependents.find(name) == dependencyIt->second.Dependents.end()) {
        setError(error, "dependency back-link missing for edge " + name + " -> " + dependency);
        return false;
      }
    }

    for (const auto &dependent : node.Dependents) {
      auto dependentIt = nodes_.find(dependent);
      if (dependentIt == nodes_.end()) {
        setError(error, "missing dependent node: " + dependent + " for " + name);
        return false;
      }
      if (dependentIt->second.Dependencies.find(name) == dependentIt->second.Dependencies.end()) {
        setError(error, "dependent forward-link missing for edge " + dependent + " -> " + name);
        return false;
      }
    }
  }

  return true;
}

GraphStats LifeTree::stats() const {
  std::lock_guard<std::mutex> lock(mutex_);
  GraphStats stats;
  stats.Modules = nodes_.size();
  for (const auto &[_, node] : nodes_) {
    stats.DependencyEdges += node.Dependencies.size();
    if (node.Dependencies.empty()) {
      ++stats.Roots;
    }
    if (node.Dependents.empty()) {
      ++stats.Leaves;
    }
  }
  return stats;
}

std::vector<std::string> LifeTree::roots() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::set<std::string> roots;
  for (const auto &[name, node] : nodes_) {
    if (node.Dependencies.empty()) {
      roots.insert(name);
    }
  }
  return toSortedVector(roots);
}

std::vector<std::string> LifeTree::leaves() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::set<std::string> leaves;
  for (const auto &[name, node] : nodes_) {
    if (node.Dependents.empty()) {
      leaves.insert(name);
    }
  }
  return toSortedVector(leaves);
}

std::vector<std::string> LifeTree::isolatedModules() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::set<std::string> isolated;
  for (const auto &[name, node] : nodes_) {
    if (node.Dependencies.empty() && node.Dependents.empty()) {
      isolated.insert(name);
    }
  }
  return toSortedVector(isolated);
}

std::string LifeTree::toDot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::ostringstream stream;
  stream << "digraph LifeTree {\n";
  stream << "  rankdir=LR;\n";
  stream << "  node [shape=box, style=rounded];\n";

  std::set<std::string> names;
  for (const auto &[name, _] : nodes_) {
    names.insert(name);
  }
  for (const auto &name : names) {
    stream << "  \"" << name << "\";\n";
  }
  for (const auto &[name, node] : nodes_) {
    const auto dependencies = toSortedVector(node.Dependencies);
    for (const auto &dependency : dependencies) {
      stream << "  \"" << name << "\" -> \"" << dependency << "\";\n";
    }
  }

  stream << "}\n";
  return stream.str();
}

bool LifeTree::hasModule(const std::string &name) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return nodes_.find(name) != nodes_.end();
}

std::size_t LifeTree::moduleCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return nodes_.size();
}

std::size_t LifeTree::dependencyEdgeCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::size_t count = 0;
  for (const auto &[_, node] : nodes_) {
    count += node.Dependencies.size();
  }
  return count;
}

bool LifeTree::moduleExistsUnlocked(const std::string &name) const {
  return nodes_.find(name) != nodes_.end();
}

std::vector<std::string> LifeTree::traverseUnlocked(const std::string &start, bool followDependencies) const {
  std::set<std::string> visited;
  std::queue<std::string> pending;
  pending.push(start);
  visited.insert(start);

  std::set<std::string> result;
  while (!pending.empty()) {
    auto current = pending.front();
    pending.pop();

    auto nodeIt = nodes_.find(current);
    if (nodeIt == nodes_.end()) {
      continue;
    }

    const auto &next = followDependencies ? nodeIt->second.Dependencies : nodeIt->second.Dependents;
    for (const auto &name : next) {
      if (visited.find(name) != visited.end()) {
        continue;
      }
      visited.insert(name);
      result.insert(name);
      pending.push(name);
    }
  }
  return toSortedVector(result);
}

bool LifeTree::computeCascadeDeletionOrderUnlocked(const std::string &name,
                                                   std::vector<std::string> *order,
                                                   std::string *error) const {
  if (!moduleExistsUnlocked(name)) {
    setError(error, "module does not exist: " + name);
    return false;
  }
  if (order == nullptr) {
    setError(error, "order output cannot be null");
    return false;
  }

  std::set<std::string> closure;
  std::queue<std::string> pending;
  pending.push(name);
  closure.insert(name);

  while (!pending.empty()) {
    const auto current = pending.front();
    pending.pop();

    auto nodeIt = nodes_.find(current);
    if (nodeIt == nodes_.end()) {
      continue;
    }
    for (const auto &dependent : nodeIt->second.Dependents) {
      if (closure.insert(dependent).second) {
        pending.push(dependent);
      }
    }
  }

  std::unordered_map<std::string, int> activeDependents;
  for (const auto &module : closure) {
    activeDependents.emplace(module, 0);
  }
  for (const auto &module : closure) {
    auto nodeIt = nodes_.find(module);
    if (nodeIt == nodes_.end()) {
      continue;
    }

    int count = 0;
    for (const auto &dependent : nodeIt->second.Dependents) {
      if (closure.find(dependent) != closure.end()) {
        ++count;
      }
    }
    activeDependents[module] = count;
  }

  std::priority_queue<std::string, std::vector<std::string>, std::greater<>> ready;
  for (const auto &[module, count] : activeDependents) {
    if (count == 0) {
      ready.push(module);
    }
  }

  std::vector<std::string> deletionOrder;
  deletionOrder.reserve(closure.size());
  while (!ready.empty()) {
    auto current = ready.top();
    ready.pop();
    deletionOrder.push_back(current);

    auto nodeIt = nodes_.find(current);
    if (nodeIt == nodes_.end()) {
      continue;
    }

    for (const auto &dependency : nodeIt->second.Dependencies) {
      if (closure.find(dependency) == closure.end()) {
        continue;
      }

      auto countIt = activeDependents.find(dependency);
      if (countIt == activeDependents.end()) {
        continue;
      }
      --countIt->second;
      if (countIt->second == 0) {
        ready.push(dependency);
      }
    }
  }

  if (deletionOrder.size() != closure.size()) {
    setError(error, "failed to compute cascade order; possible invariant violation");
    return false;
  }

  *order = std::move(deletionOrder);
  return true;
}

bool LifeTree::wouldCreateCycleUnlocked(const std::string &from, const std::string &to) const {
  std::unordered_set<std::string> visited;
  std::stack<std::string> pending;
  pending.push(to);

  while (!pending.empty()) {
    const auto current = pending.top();
    pending.pop();

    if (current == from) {
      return true;
    }
    if (!visited.insert(current).second) {
      continue;
    }

    auto nodeIt = nodes_.find(current);
    if (nodeIt == nodes_.end()) {
      continue;
    }
    for (const auto &dependency : nodeIt->second.Dependencies) {
      if (visited.find(dependency) == visited.end()) {
        pending.push(dependency);
      }
    }
  }

  return false;
}

} // namespace lifetree
