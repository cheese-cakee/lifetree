#include "lifetree.h"

#include <algorithm>
#include <iostream>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

bool assertInvariants(const lifetree::LifeTree &tree, int step) {
  std::string error;
  if (!tree.validateInvariants(&error)) {
    std::cerr << "[FAIL] invariant violation at step " << step << ": " << error << "\n";
    return false;
  }
  return true;
}

std::vector<std::string> visibleModules(const lifetree::LifeTree &tree,
                                        const std::vector<std::string> &pool) {
  std::vector<std::string> visible;
  visible.reserve(pool.size());
  for (const auto &name : pool) {
    if (tree.hasModule(name)) {
      visible.push_back(name);
    }
  }
  return visible;
}

void cleanupDeferredIds(const lifetree::LifeTree &tree, std::unordered_set<lifetree::ModuleId> *deferredIds) {
  std::vector<lifetree::ModuleId> staleIds;
  staleIds.reserve(deferredIds->size());

  for (const auto id : *deferredIds) {
    lifetree::Node node;
    std::string error;
    if (!tree.getModuleById(id, &node, &error)) {
      staleIds.push_back(id);
    }
  }

  for (const auto id : staleIds) {
    deferredIds->erase(id);
  }
}

bool runStressScenario(std::uint32_t seed, int steps) {
  lifetree::LifeTree tree;
  std::mt19937 rng(seed);
  std::uniform_int_distribution<int> operationDist(0, 7);
  std::uniform_int_distribution<int> moduleDist(0, 63);

  std::vector<std::string> namePool;
  namePool.reserve(64);
  for (int index = 0; index < 64; ++index) {
    namePool.push_back("M" + std::to_string(index));
  }

  std::unordered_set<lifetree::ModuleId> deferredIds;

  for (int step = 0; step < steps; ++step) {
    const int op = operationDist(rng);
    std::string error;

    if (op == 0) {
      const std::string name = namePool[static_cast<std::size_t>(moduleDist(rng))];
      tree.addModule(name, &error);
    } else if (op == 1) {
      const auto visible = visibleModules(tree, namePool);
      if (visible.size() >= 2U) {
        const std::string from = visible[static_cast<std::size_t>(rng() % visible.size())];
        const std::string to = visible[static_cast<std::size_t>(rng() % visible.size())];
        if (from != to) {
          tree.addDependency(from, to, &error);
        }
      }
    } else if (op == 2) {
      const auto visible = visibleModules(tree, namePool);
      if (visible.size() >= 2U) {
        const std::string from = visible[static_cast<std::size_t>(rng() % visible.size())];
        const std::string to = visible[static_cast<std::size_t>(rng() % visible.size())];
        if (from != to) {
          tree.removeDependency(from, to, &error);
        }
      }
    } else if (op == 3) {
      const auto visible = visibleModules(tree, namePool);
      if (!visible.empty()) {
        const std::string name = visible[static_cast<std::size_t>(rng() % visible.size())];
        tree.deleteModule(name, &error);
      }
    } else if (op == 4) {
      const auto visible = visibleModules(tree, namePool);
      if (!visible.empty()) {
        const std::string name = visible[static_cast<std::size_t>(rng() % visible.size())];
        lifetree::ModuleId id = 0;
        if (tree.unregisterModule(name, &id, &error) && id != 0) {
          deferredIds.insert(id);
        }
      }
    } else if (op == 5) {
      cleanupDeferredIds(tree, &deferredIds);
      if (!deferredIds.empty()) {
        auto it = deferredIds.begin();
        std::advance(it, static_cast<long>(rng() % deferredIds.size()));
        const lifetree::ModuleId id = *it;
        if (tree.destroyModule(id, &error)) {
          deferredIds.erase(id);
        }
      }
    } else if (op == 6) {
      const auto visible = visibleModules(tree, namePool);
      if (!visible.empty()) {
        std::vector<std::string> deleted;
        const std::string name = visible[static_cast<std::size_t>(rng() % visible.size())];
        tree.forceDeleteWithCascade(name, &deleted, &error);
        cleanupDeferredIds(tree, &deferredIds);
      }
    } else {
      const auto visible = visibleModules(tree, namePool);
      if (!visible.empty()) {
        const std::string name = visible[static_cast<std::size_t>(rng() % visible.size())];
        lifetree::DeleteAnalysis analysis;
        std::vector<std::string> blockers;
        tree.analyzeDelete(name, &analysis, &error);
        error.clear();
        tree.canSafelyDelete(name, &blockers, &error);
      }
    }

    if (!assertInvariants(tree, step)) {
      return false;
    }
  }

  std::string finalError;
  if (!tree.validateInvariants(&finalError)) {
    std::cerr << "[FAIL] invariant violation at scenario end: " << finalError << "\n";
    return false;
  }

  return true;
}

} // namespace

int main() {
  constexpr int kSteps = 10000;
  const std::vector<std::uint32_t> seeds = {7U, 17U, 77U, 777U};

  for (const auto seed : seeds) {
    if (!runStressScenario(seed, kSteps)) {
      std::cerr << "[FAIL] stress scenario failed (seed=" << seed << ", steps=" << kSteps << ")\n";
      return 1;
    }
  }

  std::cout << "[PASS] lifetree randomized stress tests passed"
            << " (seeds=" << seeds.size() << ", steps_per_seed=" << kSteps << ")\n";
  return 0;
}
