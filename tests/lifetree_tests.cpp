#include "lifetree.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

int failures = 0;

void fail(const std::string &message) {
  ++failures;
  std::cerr << "[FAIL] " << message << "\n";
}

void expectTrue(bool condition, const std::string &message) {
  if (!condition) {
    fail(message);
  }
}

void expectFalse(bool condition, const std::string &message) {
  if (condition) {
    fail(message);
  }
}

void expectEqual(std::size_t actual, std::size_t expected, const std::string &message) {
  if (actual != expected) {
    fail(message + " (got=" + std::to_string(actual) + ", want=" + std::to_string(expected) + ")");
  }
}

void expectContains(const std::vector<std::string> &values,
                    const std::string &needle,
                    const std::string &message) {
  if (std::find(values.begin(), values.end(), needle) == values.end()) {
    fail(message + " (missing " + needle + ")");
  }
}

std::unordered_map<std::string, std::size_t> indexMap(const std::vector<std::string> &order) {
  std::unordered_map<std::string, std::size_t> indices;
  for (std::size_t index = 0; index < order.size(); ++index) {
    indices.emplace(order[index], index);
  }
  return indices;
}

void testBasicModuleOps() {
  lifetree::LifeTree tree;
  std::string error;

  expectTrue(tree.addModule("A", &error), "addModule(A) should succeed");
  expectTrue(tree.hasModule("A"), "A should exist");
  expectEqual(tree.moduleCount(), 1U, "module count should be 1");

  expectFalse(tree.addModule("A", &error), "duplicate addModule(A) should fail");
  expectTrue(!error.empty(), "duplicate add should set error");
  expectFalse(tree.addModule("", &error), "empty module should fail");
}

void testLinearDependencyAndDelete() {
  lifetree::LifeTree tree;
  std::string error;

  expectTrue(tree.addModule("A", &error), "add A");
  expectTrue(tree.addModule("B", &error), "add B");
  expectTrue(tree.addModule("C", &error), "add C");
  expectTrue(tree.addDependency("B", "A", &error), "B->A");
  expectTrue(tree.addDependency("C", "B", &error), "C->B");
  expectEqual(tree.dependencyEdgeCount(), 2U, "edge count should be 2");

  std::vector<std::string> blockers;
  expectFalse(tree.canSafelyDelete("A", &blockers, &error), "A should not be deletable");
  expectContains(blockers, "B", "A blockers should contain B");

  expectFalse(tree.deleteModule("A", &error), "delete A should fail while B depends on it");
  expectTrue(tree.deleteModule("C", &error), "delete C should succeed");
  expectTrue(tree.deleteModule("B", &error), "delete B should succeed");
  expectTrue(tree.canSafelyDelete("A", &blockers, &error), "A should be deletable now");
}

void testDiamondDependencies() {
  lifetree::LifeTree tree;
  std::string error;

  for (const auto &name : {"A", "B", "C", "D"}) {
    expectTrue(tree.addModule(name, &error), std::string("add module ") + name);
  }

  expectTrue(tree.addDependency("B", "A", &error), "B->A");
  expectTrue(tree.addDependency("C", "A", &error), "C->A");
  expectTrue(tree.addDependency("D", "B", &error), "D->B");
  expectTrue(tree.addDependency("D", "C", &error), "D->C");

  std::vector<std::string> blockers;
  expectFalse(tree.canSafelyDelete("A", &blockers, &error), "A should not be deletable in diamond");
  expectContains(blockers, "B", "A blockers include B");
  expectContains(blockers, "C", "A blockers include C");

  expectTrue(tree.deleteModule("D", &error), "delete D");
  expectTrue(tree.deleteModule("B", &error), "delete B");
  expectTrue(tree.deleteModule("C", &error), "delete C");
  expectTrue(tree.deleteModule("A", &error), "delete A");
}

void testCycleRejection() {
  lifetree::LifeTree tree;
  std::string error;

  for (const auto &name : {"A", "B", "C"}) {
    expectTrue(tree.addModule(name, &error), std::string("add module ") + name);
  }

  expectTrue(tree.addDependency("B", "A", &error), "B->A");
  expectTrue(tree.addDependency("C", "B", &error), "C->B");
  expectFalse(tree.addDependency("A", "C", &error), "A->C should be rejected as cycle");
  expectTrue(!error.empty(), "cycle rejection should set error");
}

void testTopologicalOrder() {
  lifetree::LifeTree tree;
  std::string error;

  for (const auto &name : {"A", "B", "C", "E"}) {
    expectTrue(tree.addModule(name, &error), std::string("add module ") + name);
  }

  expectTrue(tree.addDependency("B", "A", &error), "B->A");
  expectTrue(tree.addDependency("C", "B", &error), "C->B");
  expectTrue(tree.addDependency("E", "A", &error), "E->A");

  const auto order = tree.topologicalOrder(&error);
  expectEqual(order.size(), 4U, "topological order should contain all modules");

  const auto indices = indexMap(order);
  expectTrue(indices.at("A") < indices.at("B"), "A before B");
  expectTrue(indices.at("B") < indices.at("C"), "B before C");
  expectTrue(indices.at("A") < indices.at("E"), "A before E");
}

void testMissingNodeErrors() {
  lifetree::LifeTree tree;
  std::string error;

  expectFalse(tree.addDependency("X", "Y", &error), "dependency between unknown nodes should fail");
  expectTrue(!error.empty(), "unknown dependency should set error");

  std::vector<std::string> blockers;
  error.clear();
  expectFalse(tree.canSafelyDelete("X", &blockers, &error), "canSafelyDelete on unknown node should fail");
  expectTrue(!error.empty(), "unknown delete check should set error");
}

void testRemoveDependency() {
  lifetree::LifeTree tree;
  std::string error;

  expectTrue(tree.addModule("A", &error), "add A");
  expectTrue(tree.addModule("B", &error), "add B");
  expectTrue(tree.addDependency("B", "A", &error), "B->A");
  expectTrue(tree.removeDependency("B", "A", &error), "remove B->A");
  expectEqual(tree.dependencyEdgeCount(), 0U, "edge count should be 0 after remove");

  std::vector<std::string> blockers;
  expectTrue(tree.canSafelyDelete("A", &blockers, &error), "A should be deletable after removing dependency");
}

void testTransitiveQueriesAndAnalysis() {
  lifetree::LifeTree tree;
  std::string error;

  for (const auto &name : {"A", "B", "C", "D", "E"}) {
    expectTrue(tree.addModule(name, &error), std::string("add module ") + name);
  }

  expectTrue(tree.addDependency("B", "A", &error), "B->A");
  expectTrue(tree.addDependency("C", "B", &error), "C->B");
  expectTrue(tree.addDependency("D", "B", &error), "D->B");
  expectTrue(tree.addDependency("E", "D", &error), "E->D");

  const auto transitiveDependencies = tree.transitiveDependencies("E", &error);
  expectContains(transitiveDependencies, "D", "E transitive deps contain D");
  expectContains(transitiveDependencies, "B", "E transitive deps contain B");
  expectContains(transitiveDependencies, "A", "E transitive deps contain A");

  const auto transitiveDependents = tree.transitiveDependents("A", &error);
  expectContains(transitiveDependents, "B", "A transitive dependents contain B");
  expectContains(transitiveDependents, "C", "A transitive dependents contain C");
  expectContains(transitiveDependents, "D", "A transitive dependents contain D");
  expectContains(transitiveDependents, "E", "A transitive dependents contain E");

  lifetree::DeleteAnalysis analysis;
  expectTrue(tree.analyzeDelete("A", &analysis, &error), "analyzeDelete(A) should succeed");
  expectFalse(analysis.CanSafelyDelete, "A should not be safely deletable");
  expectContains(analysis.DirectDependents, "B", "A direct dependents include B");
  expectContains(analysis.TransitiveDependents, "E", "A transitive dependents include E");
  expectTrue(!analysis.SuggestedCascadeOrder.empty(), "cascade order should not be empty");
}

void testCascadeDelete() {
  lifetree::LifeTree tree;
  std::string error;

  for (const auto &name : {"A", "B", "C", "D"}) {
    expectTrue(tree.addModule(name, &error), std::string("add module ") + name);
  }

  expectTrue(tree.addDependency("B", "A", &error), "B->A");
  expectTrue(tree.addDependency("C", "B", &error), "C->B");
  expectTrue(tree.addDependency("D", "A", &error), "D->A");

  std::vector<std::string> deleted;
  expectTrue(tree.forceDeleteWithCascade("A", &deleted, &error), "cascade delete A should succeed");
  expectEqual(tree.moduleCount(), 0U, "all modules should be removed by cascade delete");
  expectEqual(deleted.size(), 4U, "deleted list size should be 4");
}

void testStatsAndTopologyHelpers() {
  lifetree::LifeTree tree;
  std::string error;

  for (const auto &name : {"A", "B", "C", "X"}) {
    expectTrue(tree.addModule(name, &error), std::string("add module ") + name);
  }

  expectTrue(tree.addDependency("B", "A", &error), "B->A");
  expectTrue(tree.addDependency("C", "B", &error), "C->B");

  const auto stats = tree.stats();
  expectEqual(stats.Modules, 4U, "stats modules");
  expectEqual(stats.DependencyEdges, 2U, "stats edges");

  const auto roots = tree.roots();
  expectContains(roots, "A", "roots include A");
  expectContains(roots, "X", "roots include X");

  const auto leaves = tree.leaves();
  expectContains(leaves, "C", "leaves include C");
  expectContains(leaves, "X", "leaves include X");

  const auto isolated = tree.isolatedModules();
  expectContains(isolated, "X", "isolated includes X");

  const auto dot = tree.toDot();
  expectTrue(dot.find("digraph LifeTree") != std::string::npos, "dot header exists");
  lifetree::ModuleId aId = 0;
  lifetree::ModuleId bId = 0;
  expectTrue(tree.lookupModuleId("A", &aId, &error), "lookup A id for dot check");
  expectTrue(tree.lookupModuleId("B", &bId, &error), "lookup B id for dot check");
  const std::string edge = "\"id_" + std::to_string(bId) + "\" -> \"id_" + std::to_string(aId) + "\"";
  expectTrue(dot.find(edge) != std::string::npos, "dot contains B->A edge by id");
}

void testInvariantValidation() {
  lifetree::LifeTree tree;
  std::string error;

  expectTrue(tree.addModule("A", &error), "add A");
  expectTrue(tree.addModule("B", &error), "add B");
  expectTrue(tree.addDependency("B", "A", &error), "B->A");
  expectTrue(tree.validateInvariants(&error), "invariants should hold");
}

void testJsonExport() {
  lifetree::LifeTree tree;
  std::string error;

  expectTrue(tree.addModule("A", &error), "add A");
  expectTrue(tree.addModule("B", &error), "add B");
  expectTrue(tree.addDependency("B", "A", &error), "B->A");

  lifetree::ModuleId aId = 0;
  lifetree::ModuleId bId = 0;
  expectTrue(tree.lookupModuleId("A", &aId, &error), "lookup A id");
  expectTrue(tree.lookupModuleId("B", &bId, &error), "lookup B id");
  expectTrue(tree.unregisterModule("A", nullptr, &error), "unregister A");

  const std::string json = tree.toJson();
  expectTrue(json.find("\"graph\": \"LifeTree\"") != std::string::npos, "json graph tag exists");
  expectTrue(json.find("\"name\": \"A\"") != std::string::npos, "json contains A module");
  expectTrue(json.find("\"name\": \"B\"") != std::string::npos, "json contains B module");
  expectTrue(json.find("\"is_registered\": false") != std::string::npos, "json reflects unregistered module");
  expectTrue(json.find("\"dependencies\": [\"A\"]") != std::string::npos, "json includes dependency by name");
  const std::string dependencyIdField = "\"dependency_ids\": [" + std::to_string(aId) + "]";
  expectTrue(json.find(dependencyIdField) != std::string::npos, "json includes dependency id");
  const std::string dependentIdField = "\"dependent_ids\": [" + std::to_string(bId) + "]";
  expectTrue(json.find(dependentIdField) != std::string::npos, "json includes dependent id");
  expectTrue(json.find("\"registered_modules\": 1") != std::string::npos, "json stats include registered count");
  expectTrue(json.find("\"dependency_edges\": 1") != std::string::npos, "json stats include edge count");

  const std::string jsonAgain = tree.toJson();
  expectTrue(json == jsonAgain, "json export should be deterministic");

  expectTrue(tree.deleteModule("B", &error), "delete B should succeed before destroying deferred A");
  expectTrue(tree.destroyModule(aId, &error), "destroy deferred A should still work after json export");
}

void testUnregisterAndDestroyLifecycle() {
  lifetree::LifeTree tree;
  std::string error;

  expectTrue(tree.addModule("A", &error), "add A");
  expectTrue(tree.addModule("B", &error), "add B");
  expectTrue(tree.addDependency("B", "A", &error), "B->A");

  lifetree::ModuleId oldA = 0;
  expectTrue(tree.unregisterModule("A", &oldA, &error), "unregister A should succeed");
  expectTrue(oldA != 0, "unregister should return non-zero module id");
  expectFalse(tree.hasModule("A"), "A should be name-invisible after unregister");

  expectTrue(tree.addModule("A", &error), "re-add A should succeed after unregister");
  expectTrue(tree.hasModule("A"), "new A should be visible");

  expectFalse(tree.destroyModule(oldA, &error), "destroy old A should fail while B depends on it");
  expectTrue(!error.empty(), "blocked destroy should set error");

  error.clear();
  expectTrue(tree.deleteModule("B", &error), "delete B should succeed");
  expectTrue(tree.destroyModule(oldA, &error), "destroy old A should succeed after deleting B");

  expectFalse(tree.destroyModule(oldA, &error), "destroying old A twice should fail");

  expectFalse(tree.destroyModule(0, &error), "destroying unknown id should fail");
  expectTrue(!error.empty(), "destroying unknown id should set error");

  error.clear();
  expectFalse(tree.destroyModule(1, &error), "destroy registered new A should fail");
  expectTrue(!error.empty(), "destroying registered module should set error");

  lifetree::ModuleId newA = 0;
  expectTrue(tree.unregisterModule("A", &newA, &error), "unregister new A should succeed");
  expectTrue(tree.destroyModule(newA, &error), "destroy new A should succeed after unregister");
}

void testLifecycleObservabilityById() {
  lifetree::LifeTree tree;
  std::string error;

  expectTrue(tree.addModule("A", &error), "add A");
  expectTrue(tree.addModule("B", &error), "add B");
  expectTrue(tree.addDependency("B", "A", &error), "B->A");
  expectEqual(tree.registeredModuleCount(), 2U, "registered module count should be 2");

  lifetree::ModuleId aId = 0;
  expectTrue(tree.lookupModuleId("A", &aId, &error), "lookup A id should succeed");
  expectTrue(aId != 0, "lookup should return non-zero module id");

  lifetree::Node snapshot;
  expectTrue(tree.getModuleById(aId, &snapshot, &error), "snapshot by id should succeed");
  expectTrue(snapshot.Name == "A", "snapshot should report module name");
  expectTrue(snapshot.IsRegistered, "snapshot should report registered state");

  bool isRegistered = false;
  expectTrue(tree.isModuleRegistered(aId, &isRegistered, &error), "registration query should succeed");
  expectTrue(isRegistered, "A should be registered before unregister");

  lifetree::ModuleId unregisteredId = 0;
  expectTrue(tree.unregisterModule("A", &unregisteredId, &error), "unregister A should succeed");
  expectTrue(unregisteredId == aId, "unregister should return same id as lookup");
  expectEqual(tree.registeredModuleCount(), 1U, "registered module count should drop to 1");

  error.clear();
  lifetree::ModuleId missingId = 0;
  expectFalse(tree.lookupModuleId("A", &missingId, &error), "lookup should fail after unregister");
  expectTrue(!error.empty(), "failed lookup should set error");

  error.clear();
  expectTrue(tree.isModuleRegistered(aId, &isRegistered, &error), "registration query should still work by id");
  expectFalse(isRegistered, "A should be unregistered after unregister");

  error.clear();
  expectTrue(tree.getModuleById(aId, &snapshot, &error), "snapshot should still exist while deferred");
  expectFalse(snapshot.IsRegistered, "snapshot should reflect unregistered lifecycle state");
}

void testDeleteContractNonMutatingWhenBlocked() {
  lifetree::LifeTree tree;
  std::string error;

  expectTrue(tree.addModule("A", &error), "add A");
  expectTrue(tree.addModule("B", &error), "add B");
  expectTrue(tree.addDependency("B", "A", &error), "B->A");

  const std::size_t initialModules = tree.moduleCount();
  const std::size_t initialEdges = tree.dependencyEdgeCount();
  const auto initialDependents = tree.getDependents("A", &error);
  expectContains(initialDependents, "B", "A dependents should include B before delete attempt");

  error.clear();
  expectFalse(tree.deleteModule("A", &error), "delete A should fail when dependents are present");
  expectTrue(!error.empty(), "blocked delete should set error");
  expectTrue(tree.hasModule("A"), "A should remain visible after blocked delete");
  expectTrue(tree.hasModule("B"), "B should remain visible after blocked delete");
  expectEqual(tree.moduleCount(), initialModules, "blocked delete should not change module count");
  expectEqual(tree.dependencyEdgeCount(), initialEdges, "blocked delete should not change edge count");

  error.clear();
  const auto dependentsAfter = tree.getDependents("A", &error);
  expectContains(dependentsAfter, "B", "blocked delete should preserve dependent edge");
}

} // namespace

int main() {
  testBasicModuleOps();
  testLinearDependencyAndDelete();
  testDiamondDependencies();
  testCycleRejection();
  testTopologicalOrder();
  testMissingNodeErrors();
  testRemoveDependency();
  testTransitiveQueriesAndAnalysis();
  testCascadeDelete();
  testStatsAndTopologyHelpers();
  testInvariantValidation();
  testJsonExport();
  testUnregisterAndDestroyLifecycle();
  testLifecycleObservabilityById();
  testDeleteContractNonMutatingWhenBlocked();

  if (failures == 0) {
    std::cout << "[PASS] all LifeTree tests passed\n";
    return 0;
  }

  std::cerr << "[FAIL] total failures: " << failures << "\n";
  return 1;
}
