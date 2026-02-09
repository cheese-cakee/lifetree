#include "dependency_tree.h"

#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

void printVec(const std::string &label, const std::vector<std::string> &v) {
  std::cout << label << ": [";
  for (std::size_t i = 0; i < v.size(); ++i) {
    std::cout << v[i];
    if (i + 1 < v.size()) {
      std::cout << ", ";
    }
  }
  std::cout << "]\n";
}

bool addModule(depgraph::DependencyTree &tree, const std::string &name) {
  std::string err;
  if (!tree.addModule(name, &err)) {
    std::cerr << "addModule failed: " << err << "\n";
    return false;
  }
  return true;
}

bool addEdge(depgraph::DependencyTree &tree, const std::string &from, const std::string &to) {
  std::string err;
  if (!tree.addDependency(from, to, &err)) {
    std::cerr << "addDependency failed: " << err << "\n";
    return false;
  }
  return true;
}

void printStats(const depgraph::GraphStats &stats) {
  std::cout << "Stats: modules=" << stats.Modules
            << " edges=" << stats.DependencyEdges
            << " roots=" << stats.Roots
            << " leaves=" << stats.Leaves << "\n";
}

} // namespace

int main() {
  depgraph::DependencyTree tree;

  if (!addModule(tree, "A") || !addModule(tree, "B") || !addModule(tree, "C") || !addModule(tree, "E")) {
    return 1;
  }

  if (!addEdge(tree, "B", "A") || !addEdge(tree, "C", "B") || !addEdge(tree, "E", "A")) {
    return 1;
  }

  std::string topoErr;
  auto order = tree.topologicalOrder(&topoErr);
  if (order.empty() && !topoErr.empty()) {
    std::cerr << "topologicalOrder failed: " << topoErr << "\n";
    return 1;
  }
  printVec("Topological Order", order);
  printVec("Roots", tree.roots());
  printVec("Leaves", tree.leaves());
  printStats(tree.stats());

  std::vector<std::string> blockers;
  std::string err;
  if (!tree.canSafelyDelete("A", &blockers, &err)) {
    if (!err.empty()) {
      std::cerr << "canSafelyDelete error: " << err << "\n";
      return 1;
    }
    printVec("Delete blockers for A", blockers);
  }

  depgraph::DeleteAnalysis analysis;
  if (!tree.analyzeDelete("A", &analysis, &err)) {
    std::cerr << "analyzeDelete failed: " << err << "\n";
    return 1;
  }
  std::cout << "Can safely delete A? " << (analysis.CanSafelyDelete ? "yes" : "no") << "\n";
  printVec("Direct dependents of A", analysis.DirectDependents);
  printVec("Transitive dependents of A", analysis.TransitiveDependents);
  printVec("Suggested cascade delete order for A", analysis.SuggestedCascadeOrder);

  std::string invariantErr;
  if (!tree.validateInvariants(&invariantErr)) {
    std::cerr << "invariants failed: " << invariantErr << "\n";
    return 1;
  }
  std::cout << "Invariant check: PASS\n";

  std::cout << "\nDOT graph output:\n" << tree.toDot() << "\n";

  if (!tree.deleteModule("C", &err)) {
    std::cerr << "deleteModule(C) failed: " << err << "\n";
    return 1;
  }
  std::cout << "Deleted C successfully\n";

  blockers.clear();
  if (!tree.canSafelyDelete("B", &blockers, &err)) {
    if (!err.empty()) {
      std::cerr << "canSafelyDelete error: " << err << "\n";
      return 1;
    }
    printVec("Delete blockers for B", blockers);
  } else {
    std::cout << "B can now be safely deleted\n";
  }

  // Showcase cascade delete on a secondary graph.
  depgraph::DependencyTree cascadeTree;
  for (const auto &m : {"A", "B", "C", "D"}) {
    if (!addModule(cascadeTree, m)) {
      return 1;
    }
  }
  if (!addEdge(cascadeTree, "B", "A") || !addEdge(cascadeTree, "C", "B") || !addEdge(cascadeTree, "D", "A")) {
    return 1;
  }

  std::vector<std::string> deleted;
  if (!cascadeTree.forceDeleteWithCascade("A", &deleted, &err)) {
    std::cerr << "forceDeleteWithCascade failed: " << err << "\n";
    return 1;
  }
  printVec("Cascade delete order for A", deleted);
  printStats(cascadeTree.stats());

  return 0;
}
