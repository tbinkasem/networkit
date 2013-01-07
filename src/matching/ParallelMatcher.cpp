/*
 * ParallelMatcher.cpp
 *
 *  Created on: 05.12.2012
 *      Author: cls
 */


#include "ParallelMatcher.h"

namespace EnsembleClustering {

ParallelMatcher::ParallelMatcher() {
	// TODO Auto-generated constructor stub

}

ParallelMatcher::~ParallelMatcher() {
	// TODO Auto-generated destructor stub
}

Matching& ParallelMatcher::run(Graph& G) {

	// TODO: exclude isolated nodes?

	int64_t n = G.numberOfNodes();
	NodeMap<node> candidate(n, 0);					//!< candidate[v] is the preferred matching partner of v
	NodeMap<std::set<node> > S(n, std::set<node>()); 	//!< S[v] is a set with the potential
														//!< candidates of node v
	std::vector<node> D;	//!< targets of dominating edges
	Matching* M = new Matching(n);


	G.forallNodes([&](node v){
		// S[v] <- N(v)
		G.forallNeighborsOf(v, [&](node w){
			S[v].insert(w);
		});
		// set candidate of v to neighbor x with strongest connection
		// INFO: argmax is equivalent to Python max(collection, key=function)
		auto cv = argmax(S[v], [&](node w){
			return G.weight(v, w);
		});
		candidate[v] = cv;


		// if nodes mutually prefer eachother:
		if (candidate[candidate[v]] == v) {
			D.push_back(v);
			D.push_back(candidate[v]);
			M->match(v, candidate[v]);
		}
	});


	while (! D.empty()) {
		node v = D.back();
		D.pop_back();

		for (node x : S[v]) {
			if ((x != candidate[v]) && (! M->areMatched(x, candidate[x]))) {
				S[x].erase(v);
				if (! S[x].empty()) {
					// find new candidate
					auto cx = argmax(S[x], [&](node y) {
						return G.weight(x, y);
					});
					candidate[x] = cx;
				} else {
					// TODO: what if no potential candidates are left?
				}
				if (candidate[candidate[x]] == x) {
					D.push_back(x);
					D.push_back(candidate[x]);
					M->match(x, candidate[x]);
				}

			}
		}
	} // end while

	return *M;
}

} /* namespace EnsembleClustering */
