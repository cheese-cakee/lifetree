#include "lifetree.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

void printList(const std::string &label, const std::vector<std::string> &values) {
  std::cout << label << ": [";
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) {
      std::cout << ", ";
    }
    std::cout << values[index];
  }
  std::cout << "]\n";
}

bool addModule(lifetree::LifeTree &tree, const std::string &name) {
  std::string error;
  if (!tree.addModule(name, &error)) {
    std::cerr << "addModule failed: " << error << "\n";
    return false;
  }
  return true;
}

bool addDependency(lifetree::LifeTree &tree, const std::string &from, const std::string &to) {
  std::string error;
  if (!tree.addDependency(from, to, &error)) {
    std::cerr << "addDependency failed: " << error << "\n";
    return false;
  }
  return true;
}

void printStats(const lifetree::GraphStats &stats) {
  std::cout << "Stats: modules=" << stats.Modules
            << " edges=" << stats.DependencyEdges
            << " roots=" << stats.Roots
            << " leaves=" << stats.Leaves << "\n";
}

} // namespace

int main() {
  lifetree::LifeTree tree;

  if (!addModule(tree, "A") || !addModule(tree, "B") || !addModule(tree, "C") || !addModule(tree, "E")) {
    return 1;
  }

  if (!addDependency(tree, "B", "A") ||
      !addDependency(tree, "C", "B") ||
      !addDependency(tree, "E", "A")) {
    return 1;
  }

  std::string error;
  const auto order = tree.topologicalOrder(&error);
  if (order.empty() && !error.empty()) {
    std::cerr << "topologicalOrder failed: " << error << "\n";
    return 1;
  }

  printList("Topological order", order);
  printList("Roots", tree.roots());
  printList("Leaves", tree.leaves());
  printStats(tree.stats());

  std::vector<std::string> blockers;
  if (!tree.canSafelyDelete("A", &blockers, &error)) {
    if (!error.empty()) {
      std::cerr << "canSafelyDelete failed: " << error << "\n";
      return 1;
    }
    printList("Delete blockers for A", blockers);
  }

  lifetree::DeleteAnalysis analysis;
  if (!tree.analyzeDelete("A", &analysis, &error)) {
    std::cerr << "analyzeDelete failed: " << error << "\n";
    return 1;
  }

  std::cout << "Can safely delete A? " << (analysis.CanSafelyDelete ? "yes" : "no") << "\n";
  printList("Direct dependents of A", analysis.DirectDependents);
  printList("Transitive dependents of A", analysis.TransitiveDependents);
  printList("Suggested cascade delete order for A", analysis.SuggestedCascadeOrder);

  if (!tree.validateInvariants(&error)) {
    std::cerr << "validateInvariants failed: " << error << "\n";
    return 1;
  }
  std::cout << "Invariant check: PASS\n";

  std::cout << "\nDOT graph output:\n" << tree.toDot() << "\n";

  if (!tree.deleteModule("C", &error)) {
    std::cerr << "deleteModule(C) failed: " << error << "\n";
    return 1;
  }
  std::cout << "Deleted C successfully\n";

  blockers.clear();
  if (!tree.canSafelyDelete("B", &blockers, &error)) {
    if (!error.empty()) {
      std::cerr << "canSafelyDelete failed: " << error << "\n";
      return 1;
    }
    printList("Delete blockers for B", blockers);
  } else {
    std::cout << "B can now be safely deleted\n";
  }

  lifetree::LifeTree cascadeTree;
  for (const auto &name : {"A", "B", "C", "D"}) {
    if (!addModule(cascadeTree, name)) {
      return 1;
    }
  }
  if (!addDependency(cascadeTree, "B", "A") ||
      !addDependency(cascadeTree, "C", "B") ||
      !addDependency(cascadeTree, "D", "A")) {
    return 1;
  }

  std::vector<std::string> deleted;
  if (!cascadeTree.forceDeleteWithCascade("A", &deleted, &error)) {
    std::cerr << "forceDeleteWithCascade failed: " << error << "\n";
    return 1;
  }

  printList("Cascade delete order for A", deleted);
  printStats(cascadeTree.stats());
  return 0;
}
