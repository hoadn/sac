#pragma once
#include <vector>

#include "System.h"
#include "base/MathUtil.h"
typedef struct Combinais {
	std::vector<Vector2> points;
	int type;
} Combinais;

struct GridComponent {
	GridComponent() {
		row = 0 ;
		column = 0;
		type = 0;
		checkedV = false;
		checkedH = false;
	}
	int row, column, type;
	bool checkedV, checkedH;

};

#define theGridSystem GridSystem::GetInstance()
#define GRID(e) theGridSystem.Get(e)

UPDATABLE_SYSTEM(Grid)

public:

/* Return the Entity in pos (i,j)*/
Entity GetOnPos(int i, int j);

/* Return the finale list  of combinaisons*/ 
std::vector<Combinais> LookForCombinaison(int nbmin);

/* Set Back all entity at "not checked"*/
void ResetTest();

/* Return merged combinaisons */
std::vector<Combinais> MergeCombinaison(std::vector<Combinais> combinaisons);

/* Return true if an element is in both vector */
bool Intersec(std::vector<Vector2> v1, std::vector<Vector2> v2);

/* Return true if v2 is in v1*/
bool InVect(std::vector<Vector2> v1, Vector2 v2);

/* Merge 2 vector*/
Combinais MergeVectors(Combinais c1, Combinais c2);

int GridSize;
};
