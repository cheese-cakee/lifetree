#include "lifetree.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

struct PluginSpec {
  std::string Name;
  std::vector<std::string> DependsOn;
};

class PluginRuntime {
public:
  bool loadPlugin(const PluginSpec &spec) {
    std::string error;

    if (!tree_.hasModule(spec.Name) && !tree_.addModule(spec.Name, &error)) {
      std::cerr << "loadPlugin(" << spec.Name << ") failed to register module: " << error << "\n";
      return false;
    }

    for (const auto &dependency : spec.DependsOn) {
      if (!tree_.hasModule(dependency) && !tree_.addModule(dependency, &error)) {
        std::cerr << "loadPlugin(" << spec.Name << ") failed to auto-register dependency "
                  << dependency << ": " << error << "\n";
        return false;
      }
      if (!tree_.addDependency(spec.Name, dependency, &error)) {
        std::cerr << "loadPlugin(" << spec.Name << ") failed to connect dependency "
                  << dependency << ": " << error << "\n";
        return false;
      }
    }

    return true;
  }

  bool unloadPluginSafely(const std::string &name) {
    std::string error;
    if (tree_.deleteModule(name, &error)) {
      std::cout << "unloadPluginSafely(" << name << "): success\n";
      return true;
    }

    lifetree::DeleteAnalysis analysis;
    if (tree_.analyzeDelete(name, &analysis, &error)) {
      std::cout << "unloadPluginSafely(" << name << "): blocked\n";
      std::cout << "  direct blockers:";
      for (const auto &blocker : analysis.DirectDependents) {
        std::cout << " " << blocker;
      }
      std::cout << "\n  suggested cascade order:";
      for (const auto &candidate : analysis.SuggestedCascadeOrder) {
        std::cout << " " << candidate;
      }
      std::cout << "\n";
      return false;
    }

    std::cerr << "unloadPluginSafely(" << name << ") failed: " << error << "\n";
    return false;
  }

  bool unloadPluginCascade(const std::string &name) {
    std::string error;
    std::vector<std::string> deleted;
    if (!tree_.forceDeleteWithCascade(name, &deleted, &error)) {
      std::cerr << "unloadPluginCascade(" << name << ") failed: " << error << "\n";
      return false;
    }

    std::cout << "unloadPluginCascade(" << name << ") removed:";
    for (const auto &plugin : deleted) {
      std::cout << " " << plugin;
    }
    std::cout << "\n";
    return true;
  }

  void printState() const {
    std::cout << "registered modules: " << tree_.registeredModuleCount()
              << ", total nodes: " << tree_.moduleCount()
              << ", edges: " << tree_.dependencyEdgeCount() << "\n";
  }

private:
  lifetree::LifeTree tree_;
};

} // namespace

int main() {
  PluginRuntime runtime;

  const std::vector<PluginSpec> startup = {
      {"core.runtime", {}},
      {"core.logging", {"core.runtime"}},
      {"core.auth", {"core.runtime", "core.logging"}},
      {"feature.api", {"core.auth"}},
      {"feature.webui", {"feature.api", "core.logging"}},
      {"feature.metrics", {"core.runtime", "core.logging"}},
  };

  for (const auto &plugin : startup) {
    if (!runtime.loadPlugin(plugin)) {
      return 1;
    }
  }

  std::cout << "after startup:\n";
  runtime.printState();

  runtime.unloadPluginSafely("core.runtime");
  runtime.unloadPluginSafely("feature.webui");
  runtime.unloadPluginSafely("feature.api");
  runtime.unloadPluginSafely("core.auth");

  std::cout << "after selective unload:\n";
  runtime.printState();

  runtime.unloadPluginCascade("core.logging");

  std::cout << "after cascade unload:\n";
  runtime.printState();
  return 0;
}
