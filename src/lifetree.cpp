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

std::string jsonEscape(const std::string &value) {
  std::ostringstream stream;
  for (const char ch : value) {
    switch (ch) {
      case '\"':
        stream << "\\\"";
        break;
      case '\\':
        stream << "\\\\";
        break;
      case '\b':
        stream << "\\b";
        break;
      case '\f':
        stream << "\\f";
        break;
      case '\n':
        stream << "\\n";
        break;
      case '\r':
        stream << "\\r";
        break;
      case '\t':
        stream << "\\t";
        break;
      default:
        stream << ch;
        break;
    }
  }
  return stream.str();
}

} // namespace

bool LifeTree::addModule(const std::string &name, std::string *error) {
  if (name.empty()) {
    setError(error, "module name cannot be empty");
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  if (name_to_id_.find(name) != name_to_id_.end()) {
    setError(error, "module already exists: " + name);
    return false;
  }

  const ModuleId id = next_module_id_++;
  nodes_.emplace(id, Node{id, name, true, {}, {}});
  name_to_id_.emplace(name, id);
  return true;
}

bool LifeTree::lookupModuleId(const std::string &name, ModuleId *id, std::string *error) const {
  if (id == nullptr) {
    setError(error, "module id output cannot be null");
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  return resolveModuleIdUnlocked(name, id, error);
}

bool LifeTree::getModuleById(ModuleId id, Node *node, std::string *error) const {
  if (node == nullptr) {
    setError(error, "node output cannot be null");
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  const auto nodeIt = nodes_.find(id);
  if (nodeIt == nodes_.end()) {
    setError(error, "module id does not exist");
    return false;
  }

  *node = nodeIt->second;
  return true;
}

bool LifeTree::isModuleRegistered(ModuleId id, bool *isRegistered, std::string *error) const {
  if (isRegistered == nullptr) {
    setError(error, "registration output cannot be null");
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  const auto nodeIt = nodes_.find(id);
  if (nodeIt == nodes_.end()) {
    setError(error, "module id does not exist");
    return false;
  }

  *isRegistered = nodeIt->second.IsRegistered;
  return true;
}

bool LifeTree::unregisterModule(const std::string &name,
                                ModuleId *unregisteredId,
                                std::string *error) {
  std::lock_guard<std::mutex> lock(mutex_);

  ModuleId id = 0;
  if (!resolveModuleIdUnlocked(name, &id, error)) {
    setError(error, "module does not exist: " + name);
    return false;
  }

  auto nodeIt = nodes_.find(id);
  if (nodeIt == nodes_.end()) {
    setError(error, "module does not exist: " + name);
    return false;
  }
  if (!nodeIt->second.IsRegistered) {
    setError(error, "module is already unregistered: " + name);
    return false;
  }

  nodeIt->second.IsRegistered = false;
  name_to_id_.erase(name);
  if (unregisteredId != nullptr) {
    *unregisteredId = id;
  }
  return true;
}

bool LifeTree::destroyModule(ModuleId id, std::string *error) {
  std::lock_guard<std::mutex> lock(mutex_);
  return destroyModuleUnlocked(id, error);
}

bool LifeTree::addDependency(const std::string &from, const std::string &to, std::string *error) {
  if (from.empty() || to.empty()) {
    setError(error, "dependency endpoints cannot be empty");
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);

  ModuleId fromId = 0;
  if (!resolveModuleIdUnlocked(from, &fromId, error)) {
    setError(error, "source module does not exist: " + from);
    return false;
  }

  ModuleId toId = 0;
  if (!resolveModuleIdUnlocked(to, &toId, error)) {
    setError(error, "target module does not exist: " + to);
    return false;
  }

  if (fromId == toId) {
    setError(error, "self-dependency is not allowed: " + from);
    return false;
  }

  auto &fromNode = nodes_.at(fromId);
  if (fromNode.Dependencies.find(toId) != fromNode.Dependencies.end()) {
    setError(error, "dependency already exists: " + from + " -> " + to);
    return false;
  }

  if (wouldCreateCycleUnlocked(fromId, toId)) {
    setError(error, "adding dependency would create a cycle: " + from + " -> " + to);
    return false;
  }

  fromNode.Dependencies.insert(toId);
  nodes_.at(toId).Dependents.insert(fromId);
  return true;
}

bool LifeTree::removeDependency(const std::string &from, const std::string &to, std::string *error) {
  std::lock_guard<std::mutex> lock(mutex_);

  ModuleId fromId = 0;
  ModuleId toId = 0;
  if (!resolveModuleIdUnlocked(from, &fromId, nullptr) ||
      !resolveModuleIdUnlocked(to, &toId, nullptr)) {
    setError(error, "cannot remove dependency between unknown modules");
    return false;
  }

  auto &fromNode = nodes_.at(fromId);
  auto dependencyIt = fromNode.Dependencies.find(toId);
  if (dependencyIt == fromNode.Dependencies.end()) {
    setError(error, "dependency does not exist: " + from + " -> " + to);
    return false;
  }

  fromNode.Dependencies.erase(dependencyIt);
  nodes_.at(toId).Dependents.erase(fromId);
  return true;
}

bool LifeTree::canSafelyDelete(const std::string &name,
                               std::vector<std::string> *blockers,
                               std::string *error) const {
  std::lock_guard<std::mutex> lock(mutex_);

  ModuleId id = 0;
  if (!resolveModuleIdUnlocked(name, &id, error)) {
    setError(error, "module does not exist: " + name);
    return false;
  }

  const auto &node = nodes_.at(id);
  if (blockers != nullptr) {
    *blockers = idsToSortedNamesUnlocked(node.Dependents);
  }
  return node.Dependents.empty();
}

bool LifeTree::deleteModule(const std::string &name, std::string *error) {
  std::lock_guard<std::mutex> lock(mutex_);

  ModuleId id = 0;
  if (!resolveModuleIdUnlocked(name, &id, error)) {
    setError(error, "module does not exist: " + name);
    return false;
  }

  const auto nodeIt = nodes_.find(id);
  if (nodeIt == nodes_.end()) {
    setError(error, "module does not exist: " + name);
    return false;
  }
  if (!nodeIt->second.IsRegistered) {
    setError(error, "module is unregistered: " + name);
    return false;
  }

  if (!nodeIt->second.Dependents.empty()) {
    const auto blockers = idsToSortedNamesUnlocked(nodeIt->second.Dependents);
    std::ostringstream stream;
    stream << "cannot delete module " << name << "; active dependents: ";
    for (std::size_t index = 0; index < blockers.size(); ++index) {
      if (index != 0) {
        stream << ", ";
      }
      stream << blockers[index];
    }
    setError(error, stream.str());
    return false;
  }

  name_to_id_.erase(name);
  nodeIt->second.IsRegistered = false;
  return destroyModuleUnlocked(id, error);
}

bool LifeTree::forceDeleteWithCascade(const std::string &name,
                                      std::vector<std::string> *deleted,
                                      std::string *error) {
  std::lock_guard<std::mutex> lock(mutex_);

  ModuleId startId = 0;
  if (!resolveModuleIdUnlocked(name, &startId, error)) {
    setError(error, "module does not exist: " + name);
    return false;
  }

  std::vector<ModuleId> order;
  if (!computeCascadeDeletionOrderUnlocked(startId, &order, error)) {
    return false;
  }

  std::vector<std::string> deletedNames;
  deletedNames.reserve(order.size());

  for (const auto nodeId : order) {
    auto nodeIt = nodes_.find(nodeId);
    if (nodeIt == nodes_.end()) {
      continue;
    }

    deletedNames.push_back(nodeIt->second.Name);
    if (nodeIt->second.IsRegistered) {
      name_to_id_.erase(nodeIt->second.Name);
      nodeIt->second.IsRegistered = false;
    }

    for (const auto dependencyId : nodeIt->second.Dependencies) {
      auto dependencyIt = nodes_.find(dependencyId);
      if (dependencyIt != nodes_.end()) {
        dependencyIt->second.Dependents.erase(nodeId);
      }
    }

    nodes_.erase(nodeIt);
  }

  if (deleted != nullptr) {
    *deleted = std::move(deletedNames);
  }
  return true;
}

std::vector<std::string> LifeTree::topologicalOrder(std::string *error) const {
  std::lock_guard<std::mutex> lock(mutex_);

  std::unordered_map<ModuleId, int> indegrees;
  for (const auto &[id, node] : nodes_) {
    indegrees.emplace(id, static_cast<int>(node.Dependencies.size()));
  }

  std::queue<ModuleId> ready;
  for (const auto &[id, indegree] : indegrees) {
    if (indegree == 0) {
      ready.push(id);
    }
  }

  std::vector<std::string> order;
  order.reserve(nodes_.size());

  while (!ready.empty()) {
    const auto currentId = ready.front();
    ready.pop();

    const auto nodeIt = nodes_.find(currentId);
    if (nodeIt == nodes_.end()) {
      continue;
    }
    order.push_back(nodeIt->second.Name);

    for (const auto dependentId : nodeIt->second.Dependents) {
      auto indegreeIt = indegrees.find(dependentId);
      if (indegreeIt == indegrees.end()) {
        continue;
      }
      --indegreeIt->second;
      if (indegreeIt->second == 0) {
        ready.push(dependentId);
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

  ModuleId id = 0;
  if (!resolveModuleIdUnlocked(name, &id, error)) {
    setError(error, "module does not exist: " + name);
    return {};
  }

  return idsToSortedNamesUnlocked(nodes_.at(id).Dependencies);
}

std::vector<std::string> LifeTree::getDependents(const std::string &name, std::string *error) const {
  std::lock_guard<std::mutex> lock(mutex_);

  ModuleId id = 0;
  if (!resolveModuleIdUnlocked(name, &id, error)) {
    setError(error, "module does not exist: " + name);
    return {};
  }

  return idsToSortedNamesUnlocked(nodes_.at(id).Dependents);
}

std::vector<std::string> LifeTree::transitiveDependencies(const std::string &name, std::string *error) const {
  std::lock_guard<std::mutex> lock(mutex_);

  ModuleId id = 0;
  if (!resolveModuleIdUnlocked(name, &id, error)) {
    setError(error, "module does not exist: " + name);
    return {};
  }

  return traverseUnlocked(id, true);
}

std::vector<std::string> LifeTree::transitiveDependents(const std::string &name, std::string *error) const {
  std::lock_guard<std::mutex> lock(mutex_);

  ModuleId id = 0;
  if (!resolveModuleIdUnlocked(name, &id, error)) {
    setError(error, "module does not exist: " + name);
    return {};
  }

  return traverseUnlocked(id, false);
}

bool LifeTree::analyzeDelete(const std::string &name, DeleteAnalysis *analysis, std::string *error) const {
  if (analysis == nullptr) {
    setError(error, "analysis output cannot be null");
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);

  ModuleId id = 0;
  if (!resolveModuleIdUnlocked(name, &id, error)) {
    setError(error, "module does not exist: " + name);
    return false;
  }

  const auto &node = nodes_.at(id);
  analysis->DirectDependents = idsToSortedNamesUnlocked(node.Dependents);
  analysis->TransitiveDependents = traverseUnlocked(id, false);
  analysis->CanSafelyDelete = analysis->DirectDependents.empty();

  std::vector<ModuleId> orderIds;
  std::string localError;
  if (!computeCascadeDeletionOrderUnlocked(id, &orderIds, &localError)) {
    setError(error, localError);
    return false;
  }

  analysis->SuggestedCascadeOrder.clear();
  analysis->SuggestedCascadeOrder.reserve(orderIds.size());
  for (const auto nodeId : orderIds) {
    auto nodeIt = nodes_.find(nodeId);
    if (nodeIt != nodes_.end()) {
      analysis->SuggestedCascadeOrder.push_back(nodeIt->second.Name);
    }
  }

  return true;
}

bool LifeTree::validateInvariants(std::string *error) const {
  std::lock_guard<std::mutex> lock(mutex_);

  for (const auto &[id, node] : nodes_) {
    if (node.Id != id) {
      setError(error, "node key/id mismatch for module: " + node.Name);
      return false;
    }

    auto nameIt = name_to_id_.find(node.Name);
    if (node.IsRegistered) {
      if (nameIt == name_to_id_.end() || nameIt->second != id) {
        setError(error, "name-to-id mapping mismatch for registered module: " + node.Name);
        return false;
      }
    } else {
      if (nameIt != name_to_id_.end() && nameIt->second == id) {
        setError(error, "unregistered module is still name-visible under same id: " + node.Name);
        return false;
      }
    }

    for (const auto dependencyId : node.Dependencies) {
      auto dependencyIt = nodes_.find(dependencyId);
      if (dependencyIt == nodes_.end()) {
        setError(error, "missing dependency node for module: " + node.Name);
        return false;
      }
      if (dependencyIt->second.Dependents.find(id) == dependencyIt->second.Dependents.end()) {
        setError(error, "dependency back-link missing for edge " + node.Name + " -> " + dependencyIt->second.Name);
        return false;
      }
    }

    for (const auto dependentId : node.Dependents) {
      auto dependentIt = nodes_.find(dependentId);
      if (dependentIt == nodes_.end()) {
        setError(error, "missing dependent node for module: " + node.Name);
        return false;
      }
      if (dependentIt->second.Dependencies.find(id) == dependentIt->second.Dependencies.end()) {
        setError(error, "dependent forward-link missing for edge " + dependentIt->second.Name + " -> " + node.Name);
        return false;
      }
    }
  }

  for (const auto &[name, id] : name_to_id_) {
    auto nodeIt = nodes_.find(id);
    if (nodeIt == nodes_.end() || nodeIt->second.Name != name || !nodeIt->second.IsRegistered) {
      setError(error, "id-to-name mapping mismatch for module: " + name);
      return false;
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

  std::unordered_set<ModuleId> rootIds;
  for (const auto &[id, node] : nodes_) {
    if (node.Dependencies.empty()) {
      rootIds.insert(id);
    }
  }

  return idsToSortedNamesUnlocked(rootIds);
}

std::vector<std::string> LifeTree::leaves() const {
  std::lock_guard<std::mutex> lock(mutex_);

  std::unordered_set<ModuleId> leafIds;
  for (const auto &[id, node] : nodes_) {
    if (node.Dependents.empty()) {
      leafIds.insert(id);
    }
  }

  return idsToSortedNamesUnlocked(leafIds);
}

std::vector<std::string> LifeTree::isolatedModules() const {
  std::lock_guard<std::mutex> lock(mutex_);

  std::unordered_set<ModuleId> isolatedIds;
  for (const auto &[id, node] : nodes_) {
    if (node.Dependencies.empty() && node.Dependents.empty()) {
      isolatedIds.insert(id);
    }
  }

  return idsToSortedNamesUnlocked(isolatedIds);
}

std::string LifeTree::toDot() const {
  std::lock_guard<std::mutex> lock(mutex_);

  std::ostringstream stream;
  stream << "digraph LifeTree {\n";
  stream << "  rankdir=LR;\n";
  stream << "  node [shape=box, style=rounded];\n";

  const auto sortedIds = sortedNodeIdsByNameUnlocked();

  for (const auto id : sortedIds) {
    const auto &node = nodes_.at(id);
    stream << "  \"id_" << id << "\" [label=\"" << node.Name << " (#" << id << ")\"];\n";
  }

  for (const auto id : sortedIds) {
    const auto &node = nodes_.at(id);
    std::vector<ModuleId> dependencyIds(node.Dependencies.begin(), node.Dependencies.end());
    std::sort(dependencyIds.begin(), dependencyIds.end());
    for (const auto dependencyId : dependencyIds) {
      stream << "  \"id_" << id << "\" -> \"id_" << dependencyId << "\";\n";
    }
  }

  stream << "}\n";
  return stream.str();
}

std::string LifeTree::toJson() const {
  std::lock_guard<std::mutex> lock(mutex_);

  std::size_t edgeCount = 0;
  for (const auto &[_, node] : nodes_) {
    edgeCount += node.Dependencies.size();
  }

  std::ostringstream stream;
  stream << "{\n";
  stream << "  \"graph\": \"LifeTree\",\n";
  stream << "  \"modules\": [\n";

  const auto sortedIds = sortedNodeIdsByNameUnlocked();
  for (std::size_t index = 0; index < sortedIds.size(); ++index) {
    const ModuleId id = sortedIds[index];
    const auto &node = nodes_.at(id);

    stream << "    {\n";
    stream << "      \"id\": " << id << ",\n";
    stream << "      \"name\": \"" << jsonEscape(node.Name) << "\",\n";
    stream << "      \"is_registered\": " << (node.IsRegistered ? "true" : "false") << ",\n";

    const auto dependencies = idsToSortedNamesUnlocked(node.Dependencies);
    std::vector<ModuleId> dependencyIds(node.Dependencies.begin(), node.Dependencies.end());
    std::sort(dependencyIds.begin(), dependencyIds.end());
    stream << "      \"dependencies\": [";
    for (std::size_t depIndex = 0; depIndex < dependencies.size(); ++depIndex) {
      if (depIndex != 0) {
        stream << ", ";
      }
      stream << "\"" << jsonEscape(dependencies[depIndex]) << "\"";
    }
    stream << "],\n";
    stream << "      \"dependency_ids\": [";
    for (std::size_t depIndex = 0; depIndex < dependencyIds.size(); ++depIndex) {
      if (depIndex != 0) {
        stream << ", ";
      }
      stream << dependencyIds[depIndex];
    }
    stream << "],\n";

    const auto dependents = idsToSortedNamesUnlocked(node.Dependents);
    std::vector<ModuleId> dependentIds(node.Dependents.begin(), node.Dependents.end());
    std::sort(dependentIds.begin(), dependentIds.end());
    stream << "      \"dependents\": [";
    for (std::size_t depIndex = 0; depIndex < dependents.size(); ++depIndex) {
      if (depIndex != 0) {
        stream << ", ";
      }
      stream << "\"" << jsonEscape(dependents[depIndex]) << "\"";
    }
    stream << "],\n";
    stream << "      \"dependent_ids\": [";
    for (std::size_t depIndex = 0; depIndex < dependentIds.size(); ++depIndex) {
      if (depIndex != 0) {
        stream << ", ";
      }
      stream << dependentIds[depIndex];
    }
    stream << "]\n";

    stream << "    }";
    if (index + 1 != sortedIds.size()) {
      stream << ",";
    }
    stream << "\n";
  }

  stream << "  ],\n";
  stream << "  \"stats\": {\n";
  stream << "    \"modules\": " << nodes_.size() << ",\n";
  stream << "    \"registered_modules\": " << name_to_id_.size() << ",\n";
  stream << "    \"dependency_edges\": " << edgeCount << "\n";
  stream << "  }\n";
  stream << "}\n";
  return stream.str();
}

bool LifeTree::hasModule(const std::string &name) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return moduleExistsUnlocked(name);
}

std::size_t LifeTree::registeredModuleCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return name_to_id_.size();
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

bool LifeTree::destroyModuleUnlocked(ModuleId id, std::string *error) {
  auto nodeIt = nodes_.find(id);
  if (nodeIt == nodes_.end()) {
    setError(error, "module does not exist");
    return false;
  }
  if (nodeIt->second.IsRegistered) {
    setError(error, "module is still registered: " + nodeIt->second.Name);
    return false;
  }
  if (!nodeIt->second.Dependents.empty()) {
    const auto blockers = idsToSortedNamesUnlocked(nodeIt->second.Dependents);
    std::ostringstream stream;
    stream << "cannot destroy module " << nodeIt->second.Name << "; active dependents: ";
    for (std::size_t index = 0; index < blockers.size(); ++index) {
      if (index != 0) {
        stream << ", ";
      }
      stream << blockers[index];
    }
    setError(error, stream.str());
    return false;
  }

  for (const auto dependencyId : nodeIt->second.Dependencies) {
    auto dependencyIt = nodes_.find(dependencyId);
    if (dependencyIt != nodes_.end()) {
      dependencyIt->second.Dependents.erase(id);
    }
  }

  nodes_.erase(nodeIt);
  return true;
}

bool LifeTree::resolveModuleIdUnlocked(const std::string &name, ModuleId *id, std::string *error) const {
  auto it = name_to_id_.find(name);
  if (it == name_to_id_.end()) {
    setError(error, "module does not exist: " + name);
    return false;
  }

  if (id != nullptr) {
    *id = it->second;
  }
  return true;
}

bool LifeTree::moduleExistsUnlocked(const std::string &name) const {
  return name_to_id_.find(name) != name_to_id_.end();
}

std::vector<ModuleId> LifeTree::sortedNodeIdsByNameUnlocked() const {
  std::vector<ModuleId> ids;
  ids.reserve(nodes_.size());
  for (const auto &[id, _] : nodes_) {
    ids.push_back(id);
  }

  std::sort(ids.begin(), ids.end(), [this](ModuleId left, ModuleId right) {
    const auto &leftNode = nodes_.at(left);
    const auto &rightNode = nodes_.at(right);
    if (leftNode.Name != rightNode.Name) {
      return leftNode.Name < rightNode.Name;
    }
    return left < right;
  });
  return ids;
}

std::vector<std::string> LifeTree::idsToSortedNamesUnlocked(const std::unordered_set<ModuleId> &ids) const {
  std::vector<std::string> names;
  names.reserve(ids.size());

  for (const auto id : ids) {
    auto nodeIt = nodes_.find(id);
    if (nodeIt != nodes_.end()) {
      names.push_back(nodeIt->second.Name);
    }
  }

  std::sort(names.begin(), names.end());
  return names;
}

std::vector<std::string> LifeTree::traverseUnlocked(ModuleId start, bool followDependencies) const {
  std::unordered_set<ModuleId> visited;
  std::queue<ModuleId> pending;
  pending.push(start);
  visited.insert(start);

  std::unordered_set<ModuleId> result;
  while (!pending.empty()) {
    const auto current = pending.front();
    pending.pop();

    auto nodeIt = nodes_.find(current);
    if (nodeIt == nodes_.end()) {
      continue;
    }

    const auto &next = followDependencies ? nodeIt->second.Dependencies : nodeIt->second.Dependents;
    for (const auto neighborId : next) {
      if (visited.find(neighborId) != visited.end()) {
        continue;
      }
      visited.insert(neighborId);
      result.insert(neighborId);
      pending.push(neighborId);
    }
  }

  return idsToSortedNamesUnlocked(result);
}

bool LifeTree::computeCascadeDeletionOrderUnlocked(ModuleId start,
                                                   std::vector<ModuleId> *order,
                                                   std::string *error) const {
  if (nodes_.find(start) == nodes_.end()) {
    setError(error, "module does not exist");
    return false;
  }
  if (order == nullptr) {
    setError(error, "order output cannot be null");
    return false;
  }

  std::unordered_set<ModuleId> closure;
  std::queue<ModuleId> pending;
  pending.push(start);
  closure.insert(start);

  while (!pending.empty()) {
    const auto current = pending.front();
    pending.pop();

    auto nodeIt = nodes_.find(current);
    if (nodeIt == nodes_.end()) {
      continue;
    }

    for (const auto dependentId : nodeIt->second.Dependents) {
      if (closure.insert(dependentId).second) {
        pending.push(dependentId);
      }
    }
  }

  std::unordered_map<ModuleId, int> activeDependents;
  for (const auto id : closure) {
    activeDependents.emplace(id, 0);
  }

  for (const auto id : closure) {
    auto nodeIt = nodes_.find(id);
    if (nodeIt == nodes_.end()) {
      continue;
    }

    int count = 0;
    for (const auto dependentId : nodeIt->second.Dependents) {
      if (closure.find(dependentId) != closure.end()) {
        ++count;
      }
    }
    activeDependents[id] = count;
  }

  std::set<std::pair<std::string, ModuleId>> ready;
  for (const auto &[id, count] : activeDependents) {
    if (count == 0) {
      ready.emplace(nodes_.at(id).Name, id);
    }
  }

  std::vector<ModuleId> deletionOrder;
  deletionOrder.reserve(closure.size());

  while (!ready.empty()) {
    auto currentIt = ready.begin();
    const auto currentId = currentIt->second;
    ready.erase(currentIt);

    deletionOrder.push_back(currentId);

    auto nodeIt = nodes_.find(currentId);
    if (nodeIt == nodes_.end()) {
      continue;
    }

    for (const auto dependencyId : nodeIt->second.Dependencies) {
      if (closure.find(dependencyId) == closure.end()) {
        continue;
      }

      auto countIt = activeDependents.find(dependencyId);
      if (countIt == activeDependents.end()) {
        continue;
      }

      --countIt->second;
      if (countIt->second == 0) {
        ready.emplace(nodes_.at(dependencyId).Name, dependencyId);
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

bool LifeTree::wouldCreateCycleUnlocked(ModuleId from, ModuleId to) const {
  std::unordered_set<ModuleId> visited;
  std::stack<ModuleId> pending;
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

    for (const auto dependencyId : nodeIt->second.Dependencies) {
      if (visited.find(dependencyId) == visited.end()) {
        pending.push(dependencyId);
      }
    }
  }

  return false;
}

} // namespace lifetree
