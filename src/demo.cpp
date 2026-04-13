#include <iostream>
#include <sstream>
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
    cout << "  [Stats] Modules=" << s.Modules
         << "  Edges=" << s.DependencyEdges
         << "  Roots=" << s.Roots
         << "  Leaves=" << s.Leaves << "\n";
}

int main() {
    section("LifeTree Demo — Plugin-Based Server");

    LifeTree tree;
    string error;

    cout << "\nA server starts with a core runtime, authentication, logging,\n";
    cout << "and config subsystem. Features and plugins load on top of these.\n";

    section("Step 1: Register modules");

    tree.addModule("core.runtime");
    tree.addModule("core.auth");
    tree.addModule("core.logging");
    tree.addModule("core.config");
    tree.addModule("feature.webui");
    tree.addModule("feature.api");
    tree.addModule("feature.metrics");
    tree.addModule("plugin.export.csv");
    tree.addModule("plugin.export.pdf");

    cout << "\n[PASS] 9 modules registered\n";
    printStats(tree);

    section("Step 2: Express dependencies");

    tree.addDependency("core.auth",     "core.runtime");
    tree.addDependency("core.logging",  "core.runtime");
    tree.addDependency("core.config",   "core.runtime");
    tree.addDependency("feature.webui",  "core.auth");
    tree.addDependency("feature.webui",  "core.runtime");
    tree.addDependency("feature.api",    "core.auth");
    tree.addDependency("feature.api",   "core.runtime");
    tree.addDependency("feature.metrics","core.auth");
    tree.addDependency("feature.metrics","core.runtime");
    tree.addDependency("plugin.export.csv","core.logging");
    tree.addDependency("plugin.export.pdf","core.logging");

    cout << "\n[PASS] 11 dependency edges established\n";
    printStats(tree);

    section("Step 3: Deterministic load order");

    auto order = tree.topologicalOrder(&error);
    if (order.empty() && !error.empty()) {
        cout << "\n[FAIL] " << error << "\n";
        return 1;
    }
    cout << "\n  Load sequence:\n  ";
    for (size_t i = 0; i < order.size(); ++i) {
        Node n;
        tree.getModuleById(order[i], &n);
        cout << n.Name;
        if (i < order.size() - 1) cout << " -> ";
    }
    cout << "\n";
    printStats(tree);

    section("Step 4: Circular dependency detection");

    LifeTree bad;
    bad.addModule("A");
    bad.addModule("B");
    bad.addDependency("A", "B");
    string cycleError;
    bool ok = bad.addDependency("B", "A", &cycleError);
    if (!ok) {
        cout << "\n[PASS] Cycle correctly rejected:\n";
        cout << "  " << cycleError << "\n";
    } else {
        cout << "\n[FAIL] Cycle was NOT detected!\n";
        return 1;
    }

    section("Step 5: Check before deleting core.runtime");

    cout << "\n[QUERY] Can we delete core.runtime?\n";
    vector<ModuleId> blockers;
    bool can = tree.canSafelyDelete("core.runtime", &blockers, &error);
    cout << "  CanSafelyDelete: " << (can ? "true" : "false") << "\n";
    if (!blockers.empty()) {
        cout << "  Modules that would be blocked:\n";
        for (ModuleId id : blockers) {
            Node n;
            tree.getModuleById(id, &n);
            cout << "    - " << n.Name << "\n";
        }
    }

    section("Step 6: Safely unload a leaf feature");

    auto apiDeps = tree.getDependents("feature.api");
    cout << "\n[QUERY] Who depends on feature.api? " << apiDeps.size() << " modules\n";
    if (apiDeps.empty()) {
        cout << "  -> Nothing. Safe to unload.\n";
        bool removed = tree.deleteModule("feature.api", &error);
        if (removed) {
            cout << "\n[PASS] feature.api deleted safely\n";
        } else {
            cout << "\n[FAIL] " << error << "\n";
            return 1;
        }
    }
    printStats(tree);

    section("Step 7: Cascade-delete a core module");

    cout << "\n[CASCADE] Who depends on core.logging?\n";
    auto loggingDeps = tree.getDependents("core.logging");
    vector<string> loggingDepNames;
    for (ModuleId id : loggingDeps) {
        Node n;
        tree.getModuleById(id, &n);
        cout << "  - " << n.Name << "\n";
        loggingDepNames.push_back(n.Name);
    }
    cout << "\n  Force-deleting core.logging and all its dependents...\n";
    vector<ModuleId> removed;
    bool cascade = tree.forceDeleteWithCascade("core.logging", &removed, &error);
    if (cascade) {
        cout << "\n[PASS] Cascade deleted " << removed.size() << " module(s):\n";
        for (const string& name : loggingDepNames) {
            cout << "  - " << name << "\n";
        }
    } else {
        cout << "\n[FAIL] " << error << "\n";
        return 1;
    }
    printStats(tree);

    section("Step 8: Deferred modules and garbage collection");

    LifeTree gcTree;
    gcTree.addModule("Auth");
    gcTree.addModule("Session");
    gcTree.addDependency("Session", "Auth");
    cout << "\n[SCENARIO] Session depends on Auth.\n";
    cout << "  User unloads Auth to upgrade it, then registers a new Auth.\n";

    ModuleId oldId = 0;
    gcTree.unregisterModule("Auth", &oldId);
    cout << "\n[STEP] Auth unregistered (Session still references it).\n";
    cout << "  Old Auth ModuleId: " << oldId << "\n";

    gcTree.addModule("Auth");
    cout << "[STEP] New Auth registered.\n";

    auto deps = gcTree.getDependencies("Session");
    cout << "\n[CHECK] Session still points to ModuleId: " << deps[0] << "\n";
    if (deps[0] == oldId) {
        cout << "[PASS] Ghost edge preserved — Session -> deferred Auth\n";
    } else {
        cout << "[FAIL] Ghost edge was corrupted!\n";
        return 1;
    }

    vector<ModuleId> destroyed;
    size_t swept = gcTree.garbageCollect(&destroyed);
    cout << "\n[GC] Swept " << swept << " deferred module(s)\n";
    for (ModuleId id : destroyed) {
        Node n;
        gcTree.getModuleById(id, &n);
        cout << "  - Destroyed: " << n.Name << " (#" << id << ")\n";
    }

    section("Step 9: Graph export");

    LifeTree dotTree;
    dotTree.addModule("Pipeline");
    dotTree.addModule("Filter");
    dotTree.addModule("Sink");
    dotTree.addDependency("Pipeline", "Filter");
    dotTree.addDependency("Filter", "Sink");

    cout << "\n[DOT]\n" << dotTree.toDot() << "\n";
    cout << "[JSON]\n" << dotTree.toJson() << "\n";

    section("Step 10: Invariant validation");

    LifeTree invTree;
    invTree.addModule("X");
    invTree.addModule("Y");
    invTree.addDependency("X", "Y");
    string invError;
    bool invOk = invTree.validateInvariants(&invError);
    cout << "\n[PASS] Invariants valid: " << (invOk ? "true" : "false") << "\n";
    if (!invOk) {
        cout << "[FAIL] " << invError << "\n";
        return 1;
    }

    cout << "\n";
    section("All demos passed!");
    return 0;
}
