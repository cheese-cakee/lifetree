#include "dependency_tree.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

int Failures = 0;

void fail(const std::string &msg) {
  ++Failures;
  std::cerr << "[FAIL] " << msg << "\n";
}

void expectTrue(bool cond, const std::string &msg) {
  if (!cond) {
    fail(msg);
  }
}

void expectFalse(bool cond, const std::string &msg) {
  if (cond) {
    fail(msg);
  }
}

void expectEqSize(std::size_t got, std::size_t want, const std::string &msg) {
  if (got != want) {
    fail(msg + " (got=" + std::to_string(got) + ", want=" + std::to_string(want) + ")");
  }
}

void expectContains(const std::vector<std::string> &v, const std::string &needle, const std::string &msg) {
  if (std::find(v.begin(), v.end(), needle) == v.end()) {
    fail(msg + " (missing " + needle + ")");
  }
}

std::unordered_map<std::string, int> indexMap(const std::vector<std::string> &order) {
  std::unordered_map<std::string, int> idx;
  for (int i = 0; i < static_cast<int>(order.size()); ++i) {
    idx[order[static_cast<std::size_t>(i)]] = i;
  }
  return idx;
}

void testBasicModuleOps() {
  depgraph::DependencyTree tree;
  std::string err;
  expectTrue(tree.addModule("A", &err), "addModule(A) should succeed");
  expectTrue(tree.hasModule("A"), "A should exist");
  expectEqSize(tree.moduleCount(), 1U, "module count should be 1");

  expectFalse(tree.addModule("A", &err), "duplicate addModule(A) should fail");
  expectTrue(!err.empty(), "duplicate add should set err");
  expectFalse(tree.addModule("", &err), "empty module should fail");
}

void testLinearDependencyAndDelete() {
  depgraph::DependencyTree tree;
  std::string err;
  expectTrue(tree.addModule("A", &err), "add A");
  expectTrue(tree.addModule("B", &err), "add B");
  expectTrue(tree.addModule("C", &err), "add C");

  expectTrue(tree.addDependency("B", "A", &err), "B->A");
  expectTrue(tree.addDependency("C", "B", &err), "C->B");
  expectEqSize(tree.dependencyEdgeCount(), 2U, "edge count should be 2");

  std::vector<std::string> blockers;
  expectFalse(tree.canSafelyDelete("A", &blockers, &err), "A should not be deletable");
  expectContains(blockers, "B", "A blockers should contain B");

  expectFalse(tree.deleteModule("A", &err), "delete A should fail while B depends on it");
  expectTrue(tree.deleteModule("C", &err), "delete C should succeed");
  expectTrue(tree.deleteModule("B", &err), "delete B should succeed");
  expectTrue(tree.canSafelyDelete("A", &blockers, &err), "A should be deletable now");
}

void testDiamondDependencies() {
  depgraph::DependencyTree tree;
  std::string err;
  for (const auto &m : {"A", "B", "C", "D"}) {
    expectTrue(tree.addModule(m, &err), std::string("add module ") + m);
  }

  // D depends on B and C; B and C depend on A.
  expectTrue(tree.addDependency("B", "A", &err), "B->A");
  expectTrue(tree.addDependency("C", "A", &err), "C->A");
  expectTrue(tree.addDependency("D", "B", &err), "D->B");
  expectTrue(tree.addDependency("D", "C", &err), "D->C");

  std::vector<std::string> blockers;
  expectFalse(tree.canSafelyDelete("A", &blockers, &err), "A should not be deletable in diamond");
  expectContains(blockers, "B", "A blockers include B");
  expectContains(blockers, "C", "A blockers include C");

  expectTrue(tree.deleteModule("D", &err), "delete D");
  expectTrue(tree.deleteModule("B", &err), "delete B");
  expectTrue(tree.deleteModule("C", &err), "delete C");
  expectTrue(tree.deleteModule("A", &err), "delete A");
}

void testCycleRejection() {
  depgraph::DependencyTree tree;
  std::string err;
  for (const auto &m : {"A", "B", "C"}) {
    expectTrue(tree.addModule(m, &err), std::string("add module ") + m);
  }
  expectTrue(tree.addDependency("B", "A", &err), "B->A");
  expectTrue(tree.addDependency("C", "B", &err), "C->B");
  expectFalse(tree.addDependency("A", "C", &err), "A->C should be rejected as cycle");
  expectTrue(!err.empty(), "cycle rejection should set err");
}

void testTopologicalOrder() {
  depgraph::DependencyTree tree;
  std::string err;
  for (const auto &m : {"A", "B", "C", "E"}) {
    expectTrue(tree.addModule(m, &err), std::string("add module ") + m);
  }
  expectTrue(tree.addDependency("B", "A", &err), "B->A");
  expectTrue(tree.addDependency("C", "B", &err), "C->B");
  expectTrue(tree.addDependency("E", "A", &err), "E->A");

  auto order = tree.topologicalOrder(&err);
  expectEqSize(order.size(), 4U, "topo order should contain all modules");
  auto idx = indexMap(order);
  expectTrue(idx["A"] < idx["B"], "A before B");
  expectTrue(idx["B"] < idx["C"], "B before C");
  expectTrue(idx["A"] < idx["E"], "A before E");
}

void testMissingNodeErrors() {
  depgraph::DependencyTree tree;
  std::string err;
  expectFalse(tree.addDependency("X", "Y", &err), "dependency between unknown nodes should fail");
  expectTrue(!err.empty(), "unknown dependency should set err");

  std::vector<std::string> blockers;
  err.clear();
  expectFalse(tree.canSafelyDelete("X", &blockers, &err), "canSafelyDelete on unknown node should fail");
  expectTrue(!err.empty(), "unknown delete check should set err");
}

void testRemoveDependency() {
  depgraph::DependencyTree tree;
  std::string err;
  expectTrue(tree.addModule("A", &err), "add A");
  expectTrue(tree.addModule("B", &err), "add B");
  expectTrue(tree.addDependency("B", "A", &err), "B->A");
  expectTrue(tree.removeDependency("B", "A", &err), "remove B->A");
  expectEqSize(tree.dependencyEdgeCount(), 0U, "edge count should be 0 after remove");

  std::vector<std::string> blockers;
  expectTrue(tree.canSafelyDelete("A", &blockers, &err), "A should be deletable after removing dependency");
}

void testTransitiveQueriesAndAnalysis() {
  depgraph::DependencyTree tree;
  std::string err;
  for (const auto &m : {"A", "B", "C", "D", "E"}) {
    expectTrue(tree.addModule(m, &err), std::string("add module ") + m);
  }
  expectTrue(tree.addDependency("B", "A", &err), "B->A");
  expectTrue(tree.addDependency("C", "B", &err), "C->B");
  expectTrue(tree.addDependency("D", "B", &err), "D->B");
  expectTrue(tree.addDependency("E", "D", &err), "E->D");

  auto transDepsE = tree.transitiveDependencies("E", &err);
  expectContains(transDepsE, "D", "E transitive deps contain D");
  expectContains(transDepsE, "B", "E transitive deps contain B");
  expectContains(transDepsE, "A", "E transitive deps contain A");

  auto transDepdA = tree.transitiveDependents("A", &err);
  expectContains(transDepdA, "B", "A transitive dependents contain B");
  expectContains(transDepdA, "C", "A transitive dependents contain C");
  expectContains(transDepdA, "D", "A transitive dependents contain D");
  expectContains(transDepdA, "E", "A transitive dependents contain E");

  depgraph::DeleteAnalysis analysis;
  expectTrue(tree.analyzeDelete("A", &analysis, &err), "analyzeDelete(A) should succeed");
  expectFalse(analysis.CanSafelyDelete, "A should not be safely deletable");
  expectContains(analysis.DirectDependents, "B", "A direct dependents include B");
  expectContains(analysis.TransitiveDependents, "E", "A transitive dependents include E");
  expectTrue(!analysis.SuggestedCascadeOrder.empty(), "cascade order should not be empty");
}

void testCascadeDelete() {
  depgraph::DependencyTree tree;
  std::string err;
  for (const auto &m : {"A", "B", "C", "D"}) {
    expectTrue(tree.addModule(m, &err), std::string("add module ") + m);
  }
  // B->A, C->B, D->A
  expectTrue(tree.addDependency("B", "A", &err), "B->A");
  expectTrue(tree.addDependency("C", "B", &err), "C->B");
  expectTrue(tree.addDependency("D", "A", &err), "D->A");

  std::vector<std::string> deleted;
  expectTrue(tree.forceDeleteWithCascade("A", &deleted, &err), "cascade delete A should succeed");
  expectEqSize(tree.moduleCount(), 0U, "all modules should be removed by cascade delete");
  expectEqSize(deleted.size(), 4U, "deleted list size should be 4");
}

void testStatsAndTopologyHelpers() {
  depgraph::DependencyTree tree;
  std::string err;
  for (const auto &m : {"A", "B", "C", "X"}) {
    expectTrue(tree.addModule(m, &err), std::string("add module ") + m);
  }
  expectTrue(tree.addDependency("B", "A", &err), "B->A");
  expectTrue(tree.addDependency("C", "B", &err), "C->B");

  auto stats = tree.stats();
  expectEqSize(stats.Modules, 4U, "stats modules");
  expectEqSize(stats.DependencyEdges, 2U, "stats edges");

  auto roots = tree.roots();
  expectContains(roots, "A", "roots include A");
  expectContains(roots, "X", "roots include X (isolated)");

  auto leaves = tree.leaves();
  expectContains(leaves, "C", "leaves include C");
  expectContains(leaves, "X", "leaves include X");

  auto iso = tree.isolatedModules();
  expectContains(iso, "X", "isolated includes X");

  auto dot = tree.toDot();
  expectTrue(dot.find("digraph ModuleDependencies") != std::string::npos, "dot header exists");
  expectTrue(dot.find("\"B\" -> \"A\"") != std::string::npos, "dot contains B->A edge");
}

void testInvariantValidation() {
  depgraph::DependencyTree tree;
  std::string err;
  expectTrue(tree.addModule("A", &err), "add A");
  expectTrue(tree.addModule("B", &err), "add B");
  expectTrue(tree.addDependency("B", "A", &err), "B->A");
  expectTrue(tree.validateInvariants(&err), "invariants should hold");
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

  if (Failures == 0) {
    std::cout << "[PASS] all dependency tree tests passed\n";
    return 0;
  }

  std::cerr << "[FAIL] total failures: " << Failures << "\n";
  return 1;
}
