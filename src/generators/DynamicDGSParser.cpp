/*
 * DynamicDGSParser.cpp
 *
 *  Created on: Jun 17, 2013
 *      Author: forigem
 */

#include "DynamicDGSParser.h"

namespace NetworKit {

DynamicDGSParser::DynamicDGSParser(std::string path) : graphInitialized(false) {
	dgsFile.open(path.c_str(), std::ifstream::in);
}

DynamicDGSParser::~DynamicDGSParser() {

}

void DynamicDGSParser::initializeGraph() {
	if (! dgsFile.is_open()) {
		throw std::runtime_error("DGS input file could not be opened.");
	}
	else {
		DEBUG("Opened DGS file");

		std::string line;
		std::string cookie = "DGS004";

		// handle first line
		std::getline(dgsFile, line);
		if (!line.compare(0, cookie.size(), cookie)) { // compare prefix
			DEBUG("found magic cookie: DGS004");
		} else {
			DEBUG("First line: " << line);
			throw std::runtime_error("This does not seem to be a valid DGS file. Expected magic cookie 'DGS004' in first line");
		}

		// handle second line: optional name of file, number of clock ticks, total number of events
		std::getline(dgsFile, line);

		// Throw away the st0
		std::getline(dgsFile, line);

		graphInitialized = true;
	}
}

void DynamicDGSParser::generate() {
	if (graphInitialized == false) {
		throw std::runtime_error("Can not call generate() before graph was initialized.");
	}
	std::string line;
	bool breakTimeStep = false; // true if breaking from the while loop was due to a time step event

	while (std::getline(dgsFile, line)) {
		std::vector<std::string> split = Aux::StringTools::split(line);
		std::string tag = split[0];

		//std::unordered_map<std::string, node> edgeNames;

		if (tag.compare("st") == 0 && split.size() == 2) { // clock
			Gproxy->timeStep();
			breakTimeStep = true;
			break;

		} else if (tag.compare("an") == 0 && split.size() >= 2) { // add node
			// Get the node name from the input
			std::string nodeName = split[1];
			// Add a node to a graph, mapping it to the node name inside the nodeNames map
			nodeNames[nodeName] = Gproxy->addNode();
			if (split.size() == 4) { // DGS with ground truth

				std::string categoriesFullString = split[2]; /// Example: category="cond-mat.stat-mech, q-fin.ST"
				std::vector<std::string> categoriesFullStringSplit = Aux::StringTools::split(categoriesFullString, '"');

				std::string categoriesCommaSeparated = categoriesFullStringSplit[1]; // Example: cond-mat.stat-mech, q-fin.ST
				std::vector<std::string> categories = Aux::StringTools::split(categoriesCommaSeparated, ',');

				std::vector<std::string> currentNodeCategories;
				for (std::string category : categories) {
					currentNodeCategories.push_back(category);
				}
				nodeCategories.push_back(currentNodeCategories);

				std::string dateFullString = split[3]; // Example: date="08-1997"
				std::vector<std::string> dateFullStringSplit = Aux::StringTools::split(dateFullString, '"');
				std::string date = dateFullStringSplit[1];
				nodeDates.push_back(date);
			}

		} else if (tag.compare("ae") == 0 && split.size() >= 4) { // add edge
			std::string edge_from = split[2];
			std::string edge_to = split[3];
			std::string edge_name = split[1];
			Gproxy->addEdge(nodeNames[edge_from], nodeNames[edge_to], 1.0);

		} else if (tag.compare("ce") == 0 && split.size() == 3) { // update edge. Only the "weight" attribute is supported so far

			std::string from_to_edges = split[1];
			std::vector<std::string> edgesSplit = Aux::StringTools::split(from_to_edges, '-');
			std::string edge_from = edgesSplit[0];
			std::string edge_to = edgesSplit[1];

			std::string weight = split[2];
			std::vector<std::string> weightSplit = Aux::StringTools::split(weight, '=');
			double weightValue = atoi(weightSplit[1].c_str() );

			Gproxy->setWeight(nodeNames[edge_from], nodeNames[edge_to], weightValue);

		} else if (tag.compare("dn") == 0 && split.size() == 2) {
			std::string nodeName = split[1];
			node deleteNode = nodeNames[nodeName];
			// Delete the nodes only if there are no edges connected to it
			if (Gproxy->G->degree(deleteNode) == 0) {
				Gproxy->removeNode(deleteNode);
			} else {
				throw std::runtime_error("The node was not deleted, since there are edges attached to it.");
			}

		} else if (tag.compare("de") == 0 && split.size() == 2) {
			std::string from_to_edges = split[1];
			std::vector<std::string> edgesSplit = Aux::StringTools::split(from_to_edges, '-');
			std::string edge_from = edgesSplit[0];
			std::string edge_to = edgesSplit[1];
			node u = nodeNames[edge_from];
			node v = nodeNames[edge_to];

			Gproxy->removeEdge(u, v);
		}

	} // end while
	if (!breakTimeStep) {
		// while loop finished because it hit the end of the file
		throw std::logic_error("reached the end of the .dgs file");
	}


}



} /* namespace NetworKit */
