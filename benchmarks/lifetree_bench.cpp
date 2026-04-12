#include "lifetree.h"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct BenchmarkResult {
  std::string Name;
  std::size_t Operations = 0;
  double TotalMilliseconds = 0.0;
  double NanosecondsPerOperation = 0.0;
};

std::vector<std::string> makeNames(std::size_t count) {
  std::vector<std::string> names;
  names.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    names.push_back("M" + std::to_string(index));
  }
  return names;
}

void fail(const std::string &message, const std::string &error) {
  std::cerr << "[FAIL] " << message;
  if (!error.empty()) {
    std::cerr << ": " << error;
  }
  std::cerr << "\n";
  std::exit(1);
}

void buildMutationGraph(const std::vector<std::string> &names, lifetree::LifeTree *tree) {
  std::string error;

  for (const auto &name : names) {
    if (!tree->addModule(name, &error)) {
      fail("addModule failed while building mutation graph", error);
    }
  }
}

void buildAnalysisGraph(const std::vector<std::string> &names,
                        std::uint32_t seed,
                        lifetree::LifeTree *tree) {
  std::string error;

  for (const auto &name : names) {
    if (!tree->addModule(name, &error)) {
      fail("addModule failed while building analysis graph", error);
    }
  }

  std::mt19937 rng(seed);
  for (std::size_t index = 1; index < names.size(); ++index) {
    if (!tree->addDependency(names[index], names[index - 1], &error)) {
      fail("addDependency(chain) failed while building analysis graph", error);
    }

    const std::size_t fanIn = std::min<std::size_t>(3, index);
    for (std::size_t edge = 0; edge < fanIn; ++edge) {
      std::uniform_int_distribution<std::size_t> priorDist(0, index - 1);
      const std::size_t prior = priorDist(rng);
      tree->addDependency(names[index], names[prior], &error);
    }
  }
}

BenchmarkResult benchmarkMutationAddRemove() {
  constexpr std::size_t kModules = 1024;
  constexpr std::size_t kOperations = 50000;
  constexpr std::uint32_t kSeed = 1201U;

  const auto names = makeNames(kModules);
  lifetree::LifeTree tree;
  buildMutationGraph(names, &tree);
  std::mt19937 rng(kSeed);

  auto start = Clock::now();
  std::size_t executed = 0;

  for (std::size_t operation = 0; operation < kOperations; ++operation) {
    std::uniform_int_distribution<std::size_t> fromDist(1, kModules - 1);
    const std::size_t from = fromDist(rng);
    std::uniform_int_distribution<std::size_t> toDist(0, from - 1);
    const std::size_t to = toDist(rng);

    std::string error;
    tree.addDependency(names[from], names[to], &error);
    tree.removeDependency(names[from], names[to], &error);
    ++executed;
  }

  const auto end = Clock::now();
  const auto elapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  const double elapsedMs = static_cast<double>(elapsedNs) / 1'000'000.0;

  BenchmarkResult result;
  result.Name = "mutation_add_remove_dependency";
  result.Operations = executed;
  result.TotalMilliseconds = elapsedMs;
  result.NanosecondsPerOperation = static_cast<double>(elapsedNs) / static_cast<double>(executed);
  return result;
}

BenchmarkResult benchmarkAnalyzeDelete() {
  constexpr std::size_t kModules = 1500;
  constexpr std::size_t kOperations = 15000;
  constexpr std::uint32_t kGraphSeed = 2203U;
  constexpr std::uint32_t kQuerySeed = 2207U;

  const auto names = makeNames(kModules);
  lifetree::LifeTree tree;
  buildAnalysisGraph(names, kGraphSeed, &tree);
  std::mt19937 rng(kQuerySeed);
  std::uniform_int_distribution<std::size_t> queryDist(0, kModules - 1);

  auto start = Clock::now();
  std::size_t executed = 0;

  for (std::size_t operation = 0; operation < kOperations; ++operation) {
    const auto &target = names[queryDist(rng)];
    lifetree::DeleteAnalysis analysis;
    std::string error;
    if (!tree.analyzeDelete(target, &analysis, &error)) {
      fail("analyzeDelete failed in benchmark", error);
    }
    ++executed;
  }

  const auto end = Clock::now();
  const auto elapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  const double elapsedMs = static_cast<double>(elapsedNs) / 1'000'000.0;

  BenchmarkResult result;
  result.Name = "analysis_analyze_delete";
  result.Operations = executed;
  result.TotalMilliseconds = elapsedMs;
  result.NanosecondsPerOperation = static_cast<double>(elapsedNs) / static_cast<double>(executed);
  return result;
}

void printResult(const BenchmarkResult &result) {
  std::cout << std::fixed << std::setprecision(3);
  std::cout << result.Name << '\n';
  std::cout << "  operations: " << result.Operations << '\n';
  std::cout << "  total_ms: " << result.TotalMilliseconds << '\n';
  std::cout << "  ns_per_op: " << result.NanosecondsPerOperation << '\n';
}

} // namespace

int main() {
  std::cout << "LifeTree benchmark run\n";
  std::cout << "compiler: g++ -O2 -std=c++17\n";
  std::cout << "seed.mutation: 1201\n";
  std::cout << "seed.analysis.graph: 2203\n";
  std::cout << "seed.analysis.query: 2207\n";

  const auto mutation = benchmarkMutationAddRemove();
  const auto analysis = benchmarkAnalyzeDelete();

  printResult(mutation);
  printResult(analysis);
  return 0;
}
