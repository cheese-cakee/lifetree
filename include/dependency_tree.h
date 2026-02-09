#ifndef WASMEDGE_DEPENDENCY_TREE_H
#define WASMEDGE_DEPENDENCY_TREE_H

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace depgraph {

struct ModuleNode {
  std::string Name;
  std::unordered_set<std::string> Dependencies;
  std::unordered_set<std::string> Dependents;
};

struct DeleteAnalysis {
  bool CanSafelyDelete = false;
  std::vector<std::string> DirectDependents;
  std::vector<std::string> TransitiveDependents;
  std::vector<std::string> SuggestedCascadeOrder;
};

struct GraphStats {
  std::size_t Modules = 0;
  std::size_t DependencyEdges = 0;
  std::size_t Roots = 0; // No dependencies.
  std::size_t Leaves = 0; // No dependents.
};

class DependencyTree {
public:
  bool addModule(const std::string &name, std::string *err = nullptr);
  bool addDependency(const std::string &from, const std::string &to, std::string *err = nullptr);
  bool removeDependency(const std::string &from, const std::string &to, std::string *err = nullptr);

  bool canSafelyDelete(const std::string &name,
                       std::vector<std::string> *blockers = nullptr,
                       std::string *err = nullptr) const;
  bool deleteModule(const std::string &name, std::string *err = nullptr);
  bool forceDeleteWithCascade(const std::string &name, std::vector<std::string> *deleted = nullptr, std::string *err = nullptr);

  std::vector<std::string> topologicalOrder(std::string *err = nullptr) const;
  std::vector<std::string> getDependencies(const std::string &name, std::string *err = nullptr) const;
  std::vector<std::string> getDependents(const std::string &name, std::string *err = nullptr) const;
  std::vector<std::string> transitiveDependencies(const std::string &name, std::string *err = nullptr) const;
  std::vector<std::string> transitiveDependents(const std::string &name, std::string *err = nullptr) const;

  bool analyzeDelete(const std::string &name, DeleteAnalysis *analysis, std::string *err = nullptr) const;
  bool validateInvariants(std::string *err = nullptr) const;

  GraphStats stats() const;
  std::vector<std::string> roots() const;
  std::vector<std::string> leaves() const;
  std::vector<std::string> isolatedModules() const;
  std::string toDot() const;

  bool hasModule(const std::string &name) const;
  std::size_t moduleCount() const;
  std::size_t dependencyEdgeCount() const;

private:
  std::vector<std::string> bfsUnlocked(const std::string &start, bool followDependencies) const;
  bool computeCascadeDeletionOrderUnlocked(const std::string &name, std::vector<std::string> *order, std::string *err) const;
  bool moduleExistsUnlocked(const std::string &name) const;
  bool wouldCreateCycleUnlocked(const std::string &from, const std::string &to) const;

  std::unordered_map<std::string, ModuleNode> Nodes;
  mutable std::mutex Mu;
};

} // namespace depgraph

#endif // WASMEDGE_DEPENDENCY_TREE_H
