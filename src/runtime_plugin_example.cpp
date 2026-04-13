#include <iostream>
#include "lifetree.h"

using namespace lifetree;
using namespace std;

void section(const string& title) {
    cout << "\n" << string(60, '=') << "\n";
    cout << "  " << title << "\n";
    cout << string(60, '=') << "\n";
}

void printStats(const LifeTree& tree) {
    GraphStats s = tree.stats();
    cout << "  Modules=" << s.Modules
         << "  Edges=" << s.DependencyEdges
         << "  Roots=" << s.Roots
         << "  Leaves=" << s.Leaves << "\n";
}

int main() {
    section("LifeTree Runtime Plugin Simulation");

    LifeTree tree;
    string error;

    cout << "\nA plugin-based server starts up. Core services load first,\n";
    cout << "then features and export plugins register on top of them.\n";

    section("Phase 1: Register core services");

    tree.addModule("core.runtime");
    tree.addModule("core.auth");
    tree.addModule("core.logging");
    tree.addModule("core.config");
    tree.addModule("feature.webui");
    tree.addModule("feature.metrics");
    tree.addModule("feature.api");
    tree.addModule("plugin.export.csv");
    tree.addModule("plugin.export.pdf");

    cout << "\n9 modules registered\n";
    printStats(tree);

    section("Phase 2: Establish dependency graph");

    tree.addDependency("core.auth",     "core.runtime");
    tree.addDependency("core.logging",  "core.runtime");
    tree.addDependency("core.config",   "core.runtime");
    tree.addDependency("feature.webui",  "core.auth");
    tree.addDependency("feature.webui",  "core.runtime");
    tree.addDependency("feature.metrics","core.auth");
    tree.addDependency("feature.metrics","core.runtime");
    tree.addDependency("feature.api",   "core.auth");
    tree.addDependency("feature.api",    "core.runtime");
    tree.addDependency("plugin.export.csv","core.logging");
    tree.addDependency("plugin.export.pdf","core.logging");

    cout << "\n11 dependency edges established\n";
    printStats(tree);

    auto order = tree.topologicalOrder(&error);
    cout << "\n  Load order:\n  ";
    for (size_t i = 0; i < order.size(); ++i) {
        Node n; tree.getModuleById(order[i], &n);
        cout << n.Name;
        if (i < order.size() - 1) cout << " -> ";
    }
    cout << "\n";

    section("Phase 3: Reject unsafe deletion of core.runtime");

    DeleteAnalysis analysis;
    tree.analyzeDelete("core.runtime", &analysis);
    cout << "\n  Query: canSafelyDelete(core.runtime)?\n";
    cout << "  CanSafelyDelete: " << (analysis.CanSafelyDelete ? "true" : "false") << "\n";
    cout << "  Modules that would be blocked: " << analysis.DirectDependents.size() << "\n";
    for (ModuleId id : analysis.DirectDependents) {
        Node n; tree.getModuleById(id, &n);
        cout << "    - " << n.Name << "\n";
    }
    cout << "  Transitive dependents: " << analysis.TransitiveDependents.size() << "\n";

    bool rejected = tree.deleteModule("core.runtime", &error);
    if (!rejected) {
        cout << "\n  Deletion correctly rejected: " << error << "\n";
    } else {
        cout << "\n  FAIL: deletion should have been rejected\n";
        return 1;
    }

    section("Phase 4: Safe unload of feature.api");

    auto apiBlockers = tree.getDependents("feature.api");
    cout << "\n  Dependents of feature.api: " << apiBlockers.size() << "\n";
    if (apiBlockers.empty()) {
        cout << "  -> Nothing. Safe to unload.\n";
        bool removed = tree.deleteModule("feature.api", &error);
        if (removed) {
            cout << "\n  feature.api deleted\n";
        } else {
            cout << "\n  FAIL: " << error << "\n";
            return 1;
        }
    }
    printStats(tree);

    section("Phase 5: Cascade delete core.logging");

    auto loggingBlockers = tree.getDependents("core.logging");
    cout << "\n  Dependents of core.logging: " << loggingBlockers.size() << "\n";
    for (ModuleId id : loggingBlockers) {
        Node n; tree.getModuleById(id, &n);
        cout << "    - " << n.Name << "\n";
    }

    cout << "\n  forceDeleteWithCascade(core.logging)...\n";
    vector<ModuleId> removed;
    bool cascade = tree.forceDeleteWithCascade("core.logging", &removed, &error);
    if (cascade) {
        cout << "\n  Deleted " << removed.size() << " module(s):\n";
        for (ModuleId id : removed) {
            Node n; tree.getModuleById(id, &n);
            cout << "    - " << n.Name << "\n";
        }
    } else {
        cout << "\n  FAIL: " << error << "\n";
        return 1;
    }
    printStats(tree);

    section("Phase 6: Ghost edge integrity");

    cout << R"(
  Scenario: Session depends on Auth. Auth is unregistered for an upgrade,
  then a new Auth is registered before GC runs. Session's dependency edge
  must continue pointing to the original (now deferred) Auth, not hijack
  to the new one. This prevents a running Session from accidentally
  binding to a different Auth instance.
)";
    LifeTree ghostTest;
    ghostTest.addModule("Auth");
    ghostTest.addModule("Session");
    ghostTest.addDependency("Session", "Auth");

    auto depsBefore = ghostTest.getDependencies("Session");
    cout << "\n  Session depends on: " << depsBefore[0] << "\n";

    ModuleId oldAuthId = 0;
    ghostTest.unregisterModule("Auth", &oldAuthId);
    cout << "  Auth unregistered (Session ghost-depends on " << oldAuthId << ")\n";

    ghostTest.addModule("Auth");
    cout << "  New Auth registered\n";

    auto depsAfter = ghostTest.getDependencies("Session");
    cout << "\n  Session now depends on: " << depsAfter[0] << "\n";
    if (depsAfter[0] == oldAuthId) {
        cout << "  Ghost edge preserved — Session -> deferred Auth\n";
    } else {
        cout << "  FAIL: Ghost edge was corrupted\n";
        return 1;
    }

    cout << "\n  Running garbageCollect()...\n";
    vector<ModuleId> swept;
    ghostTest.garbageCollect(&swept);
    cout << "  Swept " << swept.size() << " deferred module(s)\n";

    auto deferred = ghostTest.getDeferredModules();
    cout << "  Remaining deferred: " << deferred.size() << "\n";

    section("Phase 7: Final state");

    auto finalOrder = tree.topologicalOrder(&error);
    cout << "\n  Final load order:\n  ";
    for (size_t i = 0; i < finalOrder.size(); ++i) {
        Node n; tree.getModuleById(finalOrder[i], &n);
        cout << n.Name;
        if (i < finalOrder.size() - 1) cout << " -> ";
    }
    cout << "\n";
    printStats(tree);

    cout << "\n";
    section("LifeTree Runtime Plugin Simulation Complete");
    return 0;
}
