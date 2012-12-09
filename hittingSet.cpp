//============================================================================
// Name        : hittingSet.cpp
// Author      : Haokun Luo
// Version     :
// Description : Find the minimum hitting set
//============================================================================

#include <iostream>
#include <set>
#include <map>
using namespace std;

void printSet(set<unsigned int> result) {
	cout << "Set is { ";
	int count = 0;
	for (set<unsigned int>::iterator BB = result.begin(), EE = result.end(); BB != EE; BB++) {
		count++;
		if (count == (int)result.size()) {
			cout << *BB;
		} else {
			cout << *BB << ", ";
		}
	}
	cout << "}\n";
}

void printMap(map<unsigned int, int> Map) {
	for (map<unsigned int, int>::iterator B = Map.begin(), E = Map.end(); B != E; B++) {
		cout << B->first << " -> " << B->second << endl;
	}
}

void printMap(map<int, unsigned int> Map) {
	for (map<int,unsigned int>::iterator B = Map.begin(), E = Map.end(); B != E; B++) {
		cout << B->first << " -> " << B->second << endl;
	}
}

void print2Darray(int* array, int len, map<int, unsigned int>indexToid) {
	cout << "  ";
	for (map<int, unsigned int>::iterator B = indexToid.begin(), E = indexToid.end(); B != E; B++) {
		cout << B->second << " ";
	}
	cout << endl;
	for (int i = 0; i < len; i++) {
		cout << indexToid[i] << " ";
		for (int j = 0; j < len; j++) {
			cout << array[i*len+j] << " ";
		}
		cout << endl;
	}
}

unsigned int largestCountId(map<unsigned int, int> count) {
	int max = 0;
	unsigned int max_id = 0;
	for (map<unsigned int, int>::iterator B = count.begin(), E = count.end(); B != E; B++) {
		if (B->second > max) {
			max = B->second;
			max_id = B->first;
		}
	}
	return max_id;
}

/*******************************************************************/
/****************** Interface for CS 583 project *******************/
/*******************************************************************/
set<unsigned int> findHittingSet(set< set<unsigned int> > domCollection) {
	// find the maximum covering set
	set<unsigned int> commonNode;
	map<unsigned int, int> countNode;
	map<unsigned int, int> idToindex;
	map<int, unsigned int> indexToid;
	int totalCount = 0;
	for (set< set<unsigned int> >::iterator B = domCollection.begin(), E = domCollection.end(); B != E; B++) {
		for (set<unsigned int>::iterator BB = B->begin(), EE = B->end(); BB != EE; BB++) {
			commonNode.insert(*BB);
		}
	}
	cout << "Total Count is " << totalCount << endl;
	// create a id to index map
	int mapDim = commonNode.size();
	int index = 0;
	for (set<unsigned int>::iterator BB = commonNode.begin(), EE = commonNode.end(); BB != EE; BB++, index++) {
		idToindex[*BB] = index;
		indexToid[index] = *BB;
	}
	int *edgeMap = new int[mapDim*mapDim];
	printSet(commonNode);
	cout << "Id to index map:" << endl;
	printMap(idToindex);
	cout << "Index to id map:" << endl;
	printMap(indexToid);
	// create a dynamically allocated array
	for (set< set<unsigned int> >::iterator B = domCollection.begin(), E = domCollection.end(); B != E; B++) {
		for (set<unsigned int>::iterator BB_first = B->begin(), EE_first = B->end(); BB_first != EE_first; BB_first++) {
			for (set<unsigned int>::iterator BB_second = B->begin(), EE_second = B->end(); BB_second != EE_second; BB_second++) {
				if (*BB_first != *BB_second) {
					if (edgeMap[idToindex[*BB_first]*mapDim+idToindex[*BB_second]] == 0) {
						edgeMap[idToindex[*BB_first]*mapDim+idToindex[*BB_second]] = 1;
						// only count upper triangular
						if (idToindex[*BB_first] > idToindex[*BB_second]) {
							countNode[*BB_first] += 1;
							countNode[*BB_second] += 1;
							totalCount++;
						}
					}
				}
			}
		}
	}
	cout << "Count map:" << endl;
	printMap(countNode);
	print2Darray(edgeMap, mapDim, indexToid);
	// construct hitting set
	set<unsigned int> hittingSet;
	unsigned int curID;
	int curRowIndex;
	int edgeCount;
	while(totalCount > 0) {
		curID = largestCountId(countNode);
		curRowIndex = idToindex[curID];
		edgeCount = 0;
		// count the number of columns
		for (int i = 0; i < mapDim; i++) {
			if (edgeMap[curRowIndex*mapDim + i] == 1) {
				// reset the other corresponding element
				edgeMap[i*mapDim + curRowIndex] = 0;
				edgeCount++;
			}
		}
		hittingSet.insert(curID);
		countNode.erase(curID);
		totalCount -= edgeCount;
	}
	delete [] edgeMap;
	cout << "Hitting set is " << endl ;
	return hittingSet;
}

int main() {
	set<unsigned int> first;	// {5}
	set<unsigned int> second;	// {3,5,7}
	set<unsigned int> third;	// {5,7,9}
	set< set<unsigned int> > outer;
	for (int i = 5; i <= 5; i += 2) {
		first.insert(i);
	}
	for (int i = 3; i <= 7; i += 2) {
		second.insert(i);
	}
	for (int i = 5; i <= 9; i += 2) {
		third.insert(i);
	}
	outer.insert(first);
	outer.insert(second);
	outer.insert(third);

	printSet(findHittingSet(outer));
	return 0;
}
