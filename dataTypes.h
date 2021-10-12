//
//  dataTypes.h
//  Final
//
//  Created by Jean-Yves Hervé on 2019-05-01.
//	Revised for Fall 2020, 2020-12-01

#ifndef DATAS_TYPES_H
#define DATAS_TYPES_H

#include <vector>
#include <string>
#include <pthread.h>

/**	Travel Direction data type.
 *	Note that if you define a variable
 *	Direction dir = whatever;
 *		you get the opposite Orientations from dir as (NUM_OrientationS - dir)
 *	you get left turn from dir as (dir + 1) % NUM_OrientationS
 */
enum Direction
{
	NO_DIRECTION = -1,
	//
	NORTH = 0,
	WEST,
	SOUTH,
	EAST,
	//
	NUM_DIRECTIONS
};


/**	Grid square types for this simulation
 */
enum SquareType 
{
	FREE_SQUARE,
	EXIT,
	WALL,
	VERTICAL_PARTITION,
	HORIZONTAL_PARTITION,
	TRAVELER,
	//
	NUM_SQUARE_TYPES
};

/**	Data type to store the position of *things* on the grid
 */
struct GridPosition
{
	/**	row index
	 */
	unsigned int row;
	/** column index
     */
	unsigned int col;
};

/**
 *	Data type to store the position and Direction of a traveler's segment
 */
struct TravelerSegment
{
	/**	row index
	 */
	unsigned int row;
	/** column index
     */
	unsigned int col;
	/**	One of four possible orientations
	 */
	Direction dir;


};


/**
 *	Data type for storing all information about a traveler
 *	Feel free to add anything you need.
 */
struct Traveler
{
	/**	The traveler's index
	 */
	unsigned int index;
	
	/**	The color assigned to the traveler, in rgba format
	 */
	float rgba[4];
	
	/**	The list of segments that form the 'tail' of the traveler
	 */
	std::vector<TravelerSegment> segmentList;
	
	void move(Direction dir, bool growTail = false);
	
	unsigned int num_moves = 0;
	pthread_t threadID;
	bool keepRunning;

	//Traveler Lock
	pthread_mutex_t travelerLock;
};



/**
 *	Data type to represent a sliding partition
 */
struct SlidingPartition
{
	/*	vertical vs. horizontal partition
	 */
	bool isVertical;

	/**	The blocks making up the partition, listed
	 *		top-to-bottom for a vertical list
	 *		left-to-right for a horizontal list
	 */
	std::vector<GridPosition> blockList;

	bool move(int blockIndex);
};


/**	Ugly little function to return a direction as a string
*	@param dir the direction
*	@return the direction in readable string form
*/
std::string dirStr(const Direction& dir);

/**	Ugly little function to return a square type as a string
*	@param type the suqare type
*	@return the square type in readable string form
*/
std::string typeStr(const SquareType& type);


#endif //	DATAS_TYPES_H