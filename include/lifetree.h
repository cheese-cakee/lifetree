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
  bool IsRegistered = true;
  std::unordered_set<ModuleId> Dependencies;
  std::unordered_set<ModuleId> Dependents;
};

struct DeleteAnalysis {
  bool CanSafelyDelete = false;
  std::vector<ModuleId> DirectDependents;
  std::vector<ModuleId> TransitiveDependents;
  std::vector<ModuleId> SuggestedCascadeOrder;
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
  bool lookupModuleId(const std::string &name, ModuleId *id, std::string *error = nullptr) const;
  bool getModuleById(ModuleId id, Node *node, std::string *error = nullptr) const;
  bool isModuleRegistered(ModuleId id, bool *isRegistered, std::string *error = nullptr) const;
  bool unregisterModule(const std::string &name,
                        ModuleId *unregisteredId = nullptr,
                        std::string *error = nullptr);
  bool destroyModule(ModuleId id, std::string *error = nullptr);
  bool addDependency(const std::string &from, const std::string &to, std::string *error = nullptr);
  bool removeDependency(const std::string &from, const std::string &to, std::string *error = nullptr);

  bool canSafelyDelete(const std::string &name,
                       std::vector<ModuleId> *blockers = nullptr,
                       std::string *error = nullptr) const;
  // Contract:
  // - returns false and does not mutate graph state when active dependents exist
  // - unregisters + destroys atomically when deletion is allowed
  bool deleteModule(const std::string &name, std::string *error = nullptr);
  bool forceDeleteWithCascade(const std::string &name,
                              std::vector<ModuleId> *deleted = nullptr,
                              std::string *error = nullptr);

  std::vector<ModuleId> topologicalOrder(std::string *error = nullptr) const;
  std::vector<ModuleId> getDependencies(const std::string &name, std::string *error = nullptr) const;
  std::vector<ModuleId> getDependents(const std::string &name, std::string *error = nullptr) const;
  std::vector<ModuleId> transitiveDependencies(const std::string &name, std::string *error = nullptr) const;
  std::vector<ModuleId> transitiveDependents(const std::string &name, std::string *error = nullptr) const;

  bool analyzeDelete(const std::string &name, DeleteAnalysis *analysis, std::string *error = nullptr) const;
  bool validateInvariants(std::string *error = nullptr) const;

  GraphStats stats() const;
  std::vector<ModuleId> roots() const;
  std::vector<ModuleId> leaves() const;
  std::vector<ModuleId> isolatedModules() const;
  std::vector<ModuleId> getDeferredModules() const;
  std::size_t garbageCollect(std::vector<ModuleId> *destroyed = nullptr);
  std::string toDot() const;
  std::string toJson() const;

  bool hasModule(const std::string &name) const;
  std::size_t registeredModuleCount() const;
  std::size_t moduleCount() const;
  std::size_t dependencyEdgeCount() const;

private:
  bool resolveModuleIdUnlocked(const std::string &name, ModuleId *id, std::string *error) const;
  bool moduleExistsUnlocked(const std::string &name) const;
  bool destroyModuleUnlocked(ModuleId id, std::string *error);

  std::vector<ModuleId> sortedNodeIdsByNameUnlocked() const;
  std::vector<ModuleId> sortNodeIdsUnlocked(const std::unordered_set<ModuleId> &ids) const;
  std::vector<std::string> idsToSortedNamesUnlocked(const std::unordered_set<ModuleId> &ids) const;

  std::vector<ModuleId> traverseUnlocked(ModuleId start, bool followDependencies) const;
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
