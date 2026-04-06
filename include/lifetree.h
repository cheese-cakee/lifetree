#ifndef LIFETREE_LIFETREE_H
#define LIFETREE_LIFETREE_H

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace lifetree {

using ModuleId = std::uint64_t;

struct Node {
  ModuleId Id = 0;
  std::string Name;
  std::unordered_set<ModuleId> Dependencies;
  std::unordered_set<ModuleId> Dependents;
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
  std::size_t Roots = 0;
  std::size_t Leaves = 0;
};

class LifeTree {
public:
  bool addModule(const std::string &name, std::string *error = nullptr);
  bool addDependency(const std::string &from, const std::string &to, std::string *error = nullptr);
  bool removeDependency(const std::string &from, const std::string &to, std::string *error = nullptr);

  bool canSafelyDelete(const std::string &name,
                       std::vector<std::string> *blockers = nullptr,
                       std::string *error = nullptr) const;
  bool deleteModule(const std::string &name, std::string *error = nullptr);
  bool forceDeleteWithCascade(const std::string &name,
                              std::vector<std::string> *deleted = nullptr,
                              std::string *error = nullptr);

  std::vector<std::string> topologicalOrder(std::string *error = nullptr) const;
  std::vector<std::string> getDependencies(const std::string &name, std::string *error = nullptr) const;
  std::vector<std::string> getDependents(const std::string &name, std::string *error = nullptr) const;
  std::vector<std::string> transitiveDependencies(const std::string &name, std::string *error = nullptr) const;
  std::vector<std::string> transitiveDependents(const std::string &name, std::string *error = nullptr) const;

  bool analyzeDelete(const std::string &name, DeleteAnalysis *analysis, std::string *error = nullptr) const;
  bool validateInvariants(std::string *error = nullptr) const;

  GraphStats stats() const;
  std::vector<std::string> roots() const;
  std::vector<std::string> leaves() const;
  std::vector<std::string> isolatedModules() const;
  std::string toDot() const;

  bool hasModule(const std::string &name) const;
  std::size_t moduleCount() const;
  std::size_t dependencyEdgeCount() const;

private:
  bool resolveModuleIdUnlocked(const std::string &name, ModuleId *id, std::string *error) const;
  bool moduleExistsUnlocked(const std::string &name) const;

  std::vector<ModuleId> sortedNodeIdsByNameUnlocked() const;
  std::vector<std::string> idsToSortedNamesUnlocked(const std::unordered_set<ModuleId> &ids) const;

  std::vector<std::string> traverseUnlocked(ModuleId start, bool followDependencies) const;
  bool computeCascadeDeletionOrderUnlocked(ModuleId start,
                                           std::vector<ModuleId> *order,
                                           std::string *error) const;
  bool wouldCreateCycleUnlocked(ModuleId from, ModuleId to) const;

  std::unordered_map<ModuleId, Node> nodes_;
  std::unordered_map<std::string, ModuleId> name_to_id_;
  ModuleId next_module_id_ = 1;
  mutable std::mutex mutex_;
};

} // namespace lifetree

#endif // LIFETREE_LIFETREE_H
