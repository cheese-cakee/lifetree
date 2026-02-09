#include "dependency_tree.h"

#include <algorithm>
#include <functional>
#include <queue>
#include <set>
#include <sstream>
#include <stack>

namespace depgraph {

namespace {

inline void setErr(std::string *err, const std::string &msg) {
  if (err != nullptr) {
    *err = msg;
  }
}

template <typename T>
std::vector<T> sortedVectorFromSet(const std::unordered_set<T> &input) {
  std::vector<T> out(input.begin(), input.end());
  std::sort(out.begin(), out.end());
  return out;
}

template <typename T>
std::vector<T> sortedVectorFromSet(const std::set<T> &input) {
  std::vector<T> out(input.begin(), input.end());
  return out;
}

} // namespace

bool DependencyTree::addModule(const std::string &name, std::string *err) {
  if (name.empty()) {
    setErr(err, "module name cannot be empty");
    return false;
  }

  std::lock_guard<std::mutex> lock(Mu);
  if (Nodes.find(name) != Nodes.end()) {
    setErr(err, "module already exists: " + name);
    return false;
  }

  ModuleNode node;
  node.Name = name;
  Nodes.emplace(name, std::move(node));
  return true;
}

bool DependencyTree::addDependency(const std::string &from, const std::string &to, std::string *err) {
  if (from.empty() || to.empty()) {
    setErr(err, "dependency endpoints cannot be empty");
    return false;
  }
  if (from == to) {
    setErr(err, "self-dependency is not allowed: " + from);
    return false;
  }

  std::lock_guard<std::mutex> lock(Mu);
  if (!moduleExistsUnlocked(from)) {
    setErr(err, "source module does not exist: " + from);
    return false;
  }
  if (!moduleExistsUnlocked(to)) {
    setErr(err, "target module does not exist: " + to);
    return false;
  }

  auto &fromNode = Nodes[from];
  if (fromNode.Dependencies.find(to) != fromNode.Dependencies.end()) {
    setErr(err, "dependency already exists: " + from + " -> " + to);
    return false;
  }

  if (wouldCreateCycleUnlocked(from, to)) {
    setErr(err, "adding dependency would create a cycle: " + from + " -> " + to);
    return false;
  }

  fromNode.Dependencies.insert(to);
  Nodes[to].Dependents.insert(from);
  return true;
}

bool DependencyTree::removeDependency(const std::string &from, const std::string &to, std::string *err) {
  std::lock_guard<std::mutex> lock(Mu);
  if (!moduleExistsUnlocked(from) || !moduleExistsUnlocked(to)) {
    setErr(err, "cannot remove dependency between unknown modules");
    return false;
  }

  auto &fromNode = Nodes[from];
  auto depIt = fromNode.Dependencies.find(to);
  if (depIt == fromNode.Dependencies.end()) {
    setErr(err, "dependency does not exist: " + from + " -> " + to);
    return false;
  }

  fromNode.Dependencies.erase(depIt);
  Nodes[to].Dependents.erase(from);
  return true;
}

bool DependencyTree::canSafelyDelete(const std::string &name,
                                     std::vector<std::string> *blockers,
                                     std::string *err) const {
  std::lock_guard<std::mutex> lock(Mu);
  auto it = Nodes.find(name);
  if (it == Nodes.end()) {
    setErr(err, "module does not exist: " + name);
    return false;
  }

  if (blockers != nullptr) {
    *blockers = sortedVectorFromSet(it->second.Dependents);
  }
  return it->second.Dependents.empty();
}

bool DependencyTree::deleteModule(const std::string &name, std::string *err) {
  std::lock_guard<std::mutex> lock(Mu);
  auto nodeIt = Nodes.find(name);
  if (nodeIt == Nodes.end()) {
    setErr(err, "module does not exist: " + name);
    return false;
  }
  if (!nodeIt->second.Dependents.empty()) {
    auto blockers = sortedVectorFromSet(nodeIt->second.Dependents);
    std::string msg = "cannot delete module " + name + "; active dependents: ";
    for (std::size_t i = 0; i < blockers.size(); ++i) {
      msg += blockers[i];
      if (i + 1 < blockers.size()) {
        msg += ", ";
      }
    }
    setErr(err, msg);
    return false;
  }

  for (const auto &dep : nodeIt->second.Dependencies) {
    auto depIt = Nodes.find(dep);
    if (depIt != Nodes.end()) {
      depIt->second.Dependents.erase(name);
    }
  }

  Nodes.erase(nodeIt);
  return true;
}

bool DependencyTree::forceDeleteWithCascade(const std::string &name,
                                            std::vector<std::string> *deleted,
                                            std::string *err) {
  std::lock_guard<std::mutex> lock(Mu);
  std::vector<std::string> order;
  if (!computeCascadeDeletionOrderUnlocked(name, &order, err)) {
    return false;
  }

  for (const auto &nodeName : order) {
    auto nodeIt = Nodes.find(nodeName);
    if (nodeIt == Nodes.end()) {
      continue;
    }

    for (const auto &dep : nodeIt->second.Dependencies) {
      auto depIt = Nodes.find(dep);
      if (depIt != Nodes.end()) {
        depIt->second.Dependents.erase(nodeName);
      }
    }
    Nodes.erase(nodeIt);
  }

  if (deleted != nullptr) {
    *deleted = order;
  }
  return true;
}

std::vector<std::string> DependencyTree::topologicalOrder(std::string *err) const {
  std::lock_guard<std::mutex> lock(Mu);
  std::unordered_map<std::string, int> indegree;
  for (const auto &entry : Nodes) {
    indegree[entry.first] = static_cast<int>(entry.second.Dependencies.size());
  }

  std::queue<std::string> q;
  for (const auto &entry : indegree) {
    if (entry.second == 0) {
      q.push(entry.first);
    }
  }

  std::vector<std::string> order;
  order.reserve(Nodes.size());
  while (!q.empty()) {
    const std::string cur = q.front();
    q.pop();
    order.push_back(cur);

    auto it = Nodes.find(cur);
    if (it == Nodes.end()) {
      continue;
    }
    for (const auto &dependent : it->second.Dependents) {
      auto indegreeIt = indegree.find(dependent);
      if (indegreeIt == indegree.end()) {
        continue;
      }
      --(indegreeIt->second);
      if (indegreeIt->second == 0) {
        q.push(dependent);
      }
    }
  }

  if (order.size() != Nodes.size()) {
    setErr(err, "topological order failed: graph contains a cycle");
    return {};
  }

  return order;
}

std::vector<std::string> DependencyTree::getDependencies(const std::string &name, std::string *err) const {
  std::lock_guard<std::mutex> lock(Mu);
  auto it = Nodes.find(name);
  if (it == Nodes.end()) {
    setErr(err, "module does not exist: " + name);
    return {};
  }
  return sortedVectorFromSet(it->second.Dependencies);
}

std::vector<std::string> DependencyTree::getDependents(const std::string &name, std::string *err) const {
  std::lock_guard<std::mutex> lock(Mu);
  auto it = Nodes.find(name);
  if (it == Nodes.end()) {
    setErr(err, "module does not exist: " + name);
    return {};
  }
  return sortedVectorFromSet(it->second.Dependents);
}

std::vector<std::string> DependencyTree::transitiveDependencies(const std::string &name, std::string *err) const {
  std::lock_guard<std::mutex> lock(Mu);
  if (!moduleExistsUnlocked(name)) {
    setErr(err, "module does not exist: " + name);
    return {};
  }
  return bfsUnlocked(name, true);
}

std::vector<std::string> DependencyTree::transitiveDependents(const std::string &name, std::string *err) const {
  std::lock_guard<std::mutex> lock(Mu);
  if (!moduleExistsUnlocked(name)) {
    setErr(err, "module does not exist: " + name);
    return {};
  }
  return bfsUnlocked(name, false);
}

bool DependencyTree::analyzeDelete(const std::string &name, DeleteAnalysis *analysis, std::string *err) const {
  if (analysis == nullptr) {
    setErr(err, "analysis output cannot be null");
    return false;
  }

  std::lock_guard<std::mutex> lock(Mu);
  auto it = Nodes.find(name);
  if (it == Nodes.end()) {
    setErr(err, "module does not exist: " + name);
    return false;
  }

  analysis->DirectDependents = sortedVectorFromSet(it->second.Dependents);
  analysis->TransitiveDependents = bfsUnlocked(name, false);
  analysis->CanSafelyDelete = analysis->DirectDependents.empty();

  std::vector<std::string> order;
  std::string localErr;
  if (!computeCascadeDeletionOrderUnlocked(name, &order, &localErr)) {
    setErr(err, localErr);
    return false;
  }
  analysis->SuggestedCascadeOrder = order;
  return true;
}

bool DependencyTree::validateInvariants(std::string *err) const {
  std::lock_guard<std::mutex> lock(Mu);

  for (const auto &[name, node] : Nodes) {
    if (node.Name != name) {
      setErr(err, "node key/name mismatch: " + name + " vs " + node.Name);
      return false;
    }

    for (const auto &dep : node.Dependencies) {
      auto depIt = Nodes.find(dep);
      if (depIt == Nodes.end()) {
        setErr(err, "missing dependency node: " + dep + " referenced by " + name);
        return false;
      }
      if (depIt->second.Dependents.find(name) == depIt->second.Dependents.end()) {
        setErr(err, "dependency back-link missing for edge " + name + " -> " + dep);
        return false;
      }
    }

    for (const auto &depd : node.Dependents) {
      auto depdIt = Nodes.find(depd);
      if (depdIt == Nodes.end()) {
        setErr(err, "missing dependent node: " + depd + " for " + name);
        return false;
      }
      if (depdIt->second.Dependencies.find(name) == depdIt->second.Dependencies.end()) {
        setErr(err, "dependent forward-link missing for edge " + depd + " -> " + name);
        return false;
      }
    }
  }

  return true;
}

GraphStats DependencyTree::stats() const {
  std::lock_guard<std::mutex> lock(Mu);
  GraphStats out;
  out.Modules = Nodes.size();
  for (const auto &[_, node] : Nodes) {
    out.DependencyEdges += node.Dependencies.size();
    if (node.Dependencies.empty()) {
      ++out.Roots;
    }
    if (node.Dependents.empty()) {
      ++out.Leaves;
    }
  }
  return out;
}

std::vector<std::string> DependencyTree::roots() const {
  std::lock_guard<std::mutex> lock(Mu);
  std::set<std::string> out;
  for (const auto &[name, node] : Nodes) {
    if (node.Dependencies.empty()) {
      out.insert(name);
    }
  }
  return sortedVectorFromSet(out);
}

std::vector<std::string> DependencyTree::leaves() const {
  std::lock_guard<std::mutex> lock(Mu);
  std::set<std::string> out;
  for (const auto &[name, node] : Nodes) {
    if (node.Dependents.empty()) {
      out.insert(name);
    }
  }
  return sortedVectorFromSet(out);
}

std::vector<std::string> DependencyTree::isolatedModules() const {
  std::lock_guard<std::mutex> lock(Mu);
  std::set<std::string> out;
  for (const auto &[name, node] : Nodes) {
    if (node.Dependencies.empty() && node.Dependents.empty()) {
      out.insert(name);
    }
  }
  return sortedVectorFromSet(out);
}

std::string DependencyTree::toDot() const {
  std::lock_guard<std::mutex> lock(Mu);
  std::ostringstream oss;
  oss << "digraph ModuleDependencies {\n";
  oss << "  rankdir=LR;\n";
  oss << "  node [shape=box, style=rounded];\n";

  std::set<std::string> names;
  for (const auto &[name, _] : Nodes) {
    names.insert(name);
  }
  for (const auto &name : names) {
    oss << "  \"" << name << "\";\n";
  }
  for (const auto &[name, node] : Nodes) {
    std::vector<std::string> deps(node.Dependencies.begin(), node.Dependencies.end());
    std::sort(deps.begin(), deps.end());
    for (const auto &dep : deps) {
      oss << "  \"" << name << "\" -> \"" << dep << "\";\n";
    }
  }
  oss << "}\n";
  return oss.str();
}

bool DependencyTree::hasModule(const std::string &name) const {
  std::lock_guard<std::mutex> lock(Mu);
  return Nodes.find(name) != Nodes.end();
}

std::size_t DependencyTree::moduleCount() const {
  std::lock_guard<std::mutex> lock(Mu);
  return Nodes.size();
}

std::size_t DependencyTree::dependencyEdgeCount() const {
  std::lock_guard<std::mutex> lock(Mu);
  std::size_t count = 0;
  for (const auto &entry : Nodes) {
    count += entry.second.Dependencies.size();
  }
  return count;
}

bool DependencyTree::moduleExistsUnlocked(const std::string &name) const {
  return Nodes.find(name) != Nodes.end();
}

std::vector<std::string> DependencyTree::bfsUnlocked(const std::string &start,
                                                     bool followDependencies) const {
  std::set<std::string> visited;
  std::queue<std::string> q;
  q.push(start);
  visited.insert(start);

  std::set<std::string> result;
  while (!q.empty()) {
    std::string cur = q.front();
    q.pop();
    auto it = Nodes.find(cur);
    if (it == Nodes.end()) {
      continue;
    }

    const auto &next = followDependencies ? it->second.Dependencies : it->second.Dependents;
    for (const auto &n : next) {
      if (visited.find(n) != visited.end()) {
        continue;
      }
      visited.insert(n);
      result.insert(n);
      q.push(n);
    }
  }
  return sortedVectorFromSet(result);
}

bool DependencyTree::computeCascadeDeletionOrderUnlocked(const std::string &name,
                                                         std::vector<std::string> *order,
                                                         std::string *err) const {
  if (!moduleExistsUnlocked(name)) {
    setErr(err, "module does not exist: " + name);
    return false;
  }
  if (order == nullptr) {
    setErr(err, "order output cannot be null");
    return false;
  }

  // Closure contains the target and all dependents that block its deletion.
  std::set<std::string> closure;
  std::queue<std::string> q;
  q.push(name);
  closure.insert(name);

  while (!q.empty()) {
    auto cur = q.front();
    q.pop();
    auto it = Nodes.find(cur);
    if (it == Nodes.end()) {
      continue;
    }
    for (const auto &depd : it->second.Dependents) {
      if (closure.insert(depd).second) {
        q.push(depd);
      }
    }
  }

  // Safe deletion order means modules with no dependents in closure go first.
  std::unordered_map<std::string, int> activeDependents;
  for (const auto &mod : closure) {
    activeDependents[mod] = 0;
  }
  for (const auto &mod : closure) {
    auto it = Nodes.find(mod);
    if (it == Nodes.end()) {
      continue;
    }
    int count = 0;
    for (const auto &depd : it->second.Dependents) {
      if (closure.find(depd) != closure.end()) {
        ++count;
      }
    }
    activeDependents[mod] = count;
  }

  std::priority_queue<std::string, std::vector<std::string>, std::greater<>> ready;
  for (const auto &entry : activeDependents) {
    if (entry.second == 0) {
      ready.push(entry.first);
    }
  }

  std::vector<std::string> out;
  out.reserve(closure.size());
  while (!ready.empty()) {
    auto cur = ready.top();
    ready.pop();
    out.push_back(cur);

    auto curIt = Nodes.find(cur);
    if (curIt == Nodes.end()) {
      continue;
    }

    for (const auto &dep : curIt->second.Dependencies) {
      if (closure.find(dep) == closure.end()) {
        continue;
      }
      auto indepIt = activeDependents.find(dep);
      if (indepIt == activeDependents.end()) {
        continue;
      }
      --(indepIt->second);
      if (indepIt->second == 0) {
        ready.push(dep);
      }
    }
  }

  if (out.size() != closure.size()) {
    setErr(err, "failed to compute cascade order; possible invariant violation");
    return false;
  }

  *order = std::move(out);
  return true;
}

bool DependencyTree::wouldCreateCycleUnlocked(const std::string &from, const std::string &to) const {
  // Adding from -> to creates a cycle if to can already reach from through dependencies.
  std::unordered_set<std::string> visited;
  std::stack<std::string> st;
  st.push(to);

  while (!st.empty()) {
    const std::string cur = st.top();
    st.pop();

    if (cur == from) {
      return true;
    }
    if (visited.find(cur) != visited.end()) {
      continue;
    }
    visited.insert(cur);

    auto it = Nodes.find(cur);
    if (it == Nodes.end()) {
      continue;
    }
    for (const auto &dep : it->second.Dependencies) {
      if (visited.find(dep) == visited.end()) {
        st.push(dep);
      }
    }
  }
  return false;
}

} // namespace depgraph
