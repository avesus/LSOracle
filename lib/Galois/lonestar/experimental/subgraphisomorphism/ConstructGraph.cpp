/*
 * This file belongs to the Galois project, a C++ library for exploiting parallelism.
 * The code is being released under the terms of the 3-Clause BSD License (a
 * copy is located in LICENSE.txt at the top-level directory).
 *
 * Copyright (C) 2018, The University of Texas at Austin. All rights reserved.
 * UNIVERSITY EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES CONCERNING THIS
 * SOFTWARE AND DOCUMENTATION, INCLUDING ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR ANY PARTICULAR PURPOSE, NON-INFRINGEMENT AND WARRANTIES OF
 * PERFORMANCE, AND ANY WARRANTY THAT MIGHT OTHERWISE ARISE FROM COURSE OF
 * DEALING OR USAGE OF TRADE.  NO WARRANTY IS EITHER EXPRESS OR IMPLIED WITH
 * RESPECT TO THE USE OF THE SOFTWARE OR DOCUMENTATION. Under no circumstances
 * shall University be liable for incidental, special, indirect, direct or
 * consequential damages or loss of profits, interruption of business, or
 * related expenses which may arise from use of Software or Documentation,
 * including but not limited to those resulting from defects in Software and/or
 * Documentation, or loss or inaccuracy of data of any kind.
 */

#include "galois/Galois.h"
#include "galois/Bag.h"
#include "galois/Timer.h"
#include "galois/Timer.h"
#include "galois/graphs/Graph.h"
#include "galois/graphs/TypeTraits.h"
#include "llvm/Support/CommandLine.h"
#include "Lonestar/BoilerPlate.h"

#include <chrono>
#include <random>

#include <vector>
#include <iostream>
#include <fstream>

namespace cll = llvm::cl;

static const char* name = "Graph Construction";
static const char* desc = "Construct a random graph";
static const char* url  = "construct_graph";

static cll::opt<bool> undirected("undirected",
                                 cll::desc("construct undirected graphs"),
                                 cll::init(false));

static cll::opt<unsigned int> numNodes("numNodes", cll::desc("# nodes"),
                                       cll::init(9));
static cll::opt<unsigned int> numEdges("numEdges", cll::desc("# edges"),
                                       cll::init(20));

static cll::opt<bool>
    rndSeedByTime("rndSeedByTime",
                  cll::desc("rndSeed generated by system time"),
                  cll::init(false));
static cll::opt<unsigned int> rndSeed("rndSeed", cll::desc("random seed"),
                                      cll::init(0));

static std::minstd_rand0 generator;
static std::uniform_int_distribution<unsigned> distribution;

template <typename Graph>
void constructGraph(Graph& g) {
  typedef typename Graph::GraphNode GNode;

  // construct a set of nodes
  std::vector<GNode> nodes(numNodes);
  for (unsigned int i = 0; i < numNodes; ++i) {
    GNode n = g.createNode(0);
    g.addNode(n);
    g.getData(n) = i;
    nodes[i]     = n;
  }

  // add edges
  for (unsigned int i = 0; i < numEdges;) {
    unsigned int src = distribution(generator) % numNodes;
    unsigned int dst = distribution(generator) % numNodes;

    // no self loops and repeated edges
    if (src != dst &&
        g.findEdge(nodes[src], nodes[dst]) == g.edge_end(nodes[src])) {
      g.addEdge(nodes[src], nodes[dst]);
      if (undirected) {
        g.addEdge(nodes[dst], nodes[src]);
      }
      ++i;
    }
  }
}

template <typename Graph>
void printGraph(Graph& g) {
  auto output = std::ofstream("query.txt");
  for (auto ni = g.begin(), ne = g.end(); ni != ne; ++ni) {
    auto& src = g.getData(*ni);
    for (auto ei = g.edge_begin(*ni), ee = g.edge_end(*ni); ei != ee; ++ei) {
      auto& dst = g.getData(g.getEdgeDst(ei));
      output << src << " " << dst << std::endl;
    }
  }
  output.close();
}

int main(int argc, char** argv) {
  galois::StatManager statManager;
  LonestarStart(argc, argv, name, desc, url);

  if (rndSeedByTime) {
    rndSeed = std::chrono::system_clock::now().time_since_epoch().count();
  }
  std::cout << "rndSeed: " << rndSeed << std::endl;
  unsigned int seed = rndSeed;
  generator.seed(seed);

  unsigned int maxNumEdges = numNodes * (numNodes - 1);
  if (undirected) {
    maxNumEdges /= 2;
  }
  if (numEdges > maxNumEdges) {
    numEdges = maxNumEdges;
  }

  galois::graphs::MorphGraph<int, void, true> g;
  constructGraph(g);
  printGraph(g);

  return 0;
}
