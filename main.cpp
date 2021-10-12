//
//  main.c
//  Final Project CSC412
//
//  Created by Jean-Yves Hervé on 2020-12-01
//	This is public domain code.  By all means appropriate it and change is to your
//	heart's content.
#include <iostream>
#include <sstream>
#include <string>
#include <random>
//
#include <cstdio>
#include <cstdlib>
#include <ctime>
//
#include "gl_frontEnd.h"
#include <unistd.h>
#include <pthread.h>
using namespace std;

//==================================================================================
//	Function prototypes
//==================================================================================
void initializeApplication(void);
GridPosition getNewFreePosition(void);
Direction newDirection(Direction forbiddenDir = NUM_DIRECTIONS);
TravelerSegment newTravelerSegment(const TravelerSegment& currentSeg, bool& canAdd);
void generateWalls(void);
vector<SlidingPartition> generatePartitions(void);
void* thread_run(void *data);
vector<Direction> get_free_spaces(unsigned int row, unsigned int col, Direction dir );
Direction neighbor_check(unsigned int row, unsigned int col, SquareType type);
GridPosition neighbor_find(unsigned int row, unsigned int col, SquareType type);
bool comparePositions(const GridPosition& pos1, const GridPosition& pos2);
bool neighborInfront(TravelerSegment head, Direction direction, SquareType partition);
//==================================================================================
//	Application-level global variables
//==================================================================================

//	Don't rename any of these variables
//-------------------------------------
//	The state grid and its dimensions (arguments to the program)
SquareType** grid;
unsigned int numRows = 0;	//	height of the grid
unsigned int numCols = 0;	//	width
unsigned int numTravelers = 0;	//	initial number
unsigned int numTravelersDone = 0;
unsigned int numLiveThreads = 0;		//	the number of live traveler threads
vector<Traveler> travelerList;
vector<SlidingPartition> partitionList;
GridPosition	exitPos;	//	location of the exit

//	travelers' sleep time between moves (in microseconds)
const int MIN_SLEEP_TIME = 1000;
int travelerSleepTime = 100000;
int grow_count = 5;
//	An array of C-string where you can store things you want displayed
//	in the state pane to display (for debugging purposes?)
//	Dont change the dimensions as this may break the front end
const int MAX_NUM_MESSAGES = 8;
const int MAX_LENGTH_MESSAGE = 32;
char** message;
time_t launchTime;

//	Random generators:  For uniform distributions
const unsigned int MAX_NUM_INITIAL_SEGMENTS = 6;
random_device randDev;
default_random_engine engine(randDev());
uniform_int_distribution<unsigned int> unsignedNumberGenerator(0, numeric_limits<unsigned int>::max());
uniform_int_distribution<unsigned int> segmentNumberGenerator(0, MAX_NUM_INITIAL_SEGMENTS);
uniform_int_distribution<unsigned int> segmentDirectionGenerator(0, NUM_DIRECTIONS-1);
uniform_int_distribution<unsigned int> headsOrTails(0, 1);
uniform_int_distribution<unsigned int> rowGenerator;
uniform_int_distribution<unsigned int> colGenerator;


pthread_mutex_t global_gridLock;
vector<vector<pthread_mutex_t>> gridLock;

//==================================================================================
//	These are the functions that tie the simulation with the rendering.
//	Some parts are "don't touch."  Other parts need your intervention
//	to make sure that access to critical section is properly synchronized
//==================================================================================

void drawTravelers(void)
{
	//-----------------------------
	//	You may have to sychronize things here
	//-----------------------------
	for (unsigned int k=0; k<travelerList.size(); k++)
	{
		//	here I would test if the traveler thread is still live
		if (travelerList[k].keepRunning)
		{
			drawTraveler(travelerList[k]);
		}
	}
}

void updateMessages(void)
{
	//	Here I hard-code a few messages that I want to see displayed
	//	in my state pane.  The number of live robot threads will
	//	always get displayed.  No need to pass a message about it.
	unsigned int numMessages = 5;
	sprintf(message[0], "We created %d travelers", numTravelers);
	sprintf(message[1], "%d travelers solved the maze", numTravelersDone);
	sprintf(message[2], "I like cheese");
	sprintf(message[3], "Simulation run time is %ld", time(NULL)-launchTime);
	sprintf(message[4], "Number of partitions = %lu", partitionList.size());
	
	//---------------------------------------------------------
	//	This is the call that makes OpenGL render information
	//	about the state of the simulation.
	//
	//	You *must* synchronize this call.
	//
	//---------------------------------------------------------
	drawMessages(numMessages, message);
}

void handleKeyboardEvent(unsigned char c, int x, int y)
{
	int ok = 0;

	switch (c)
	{
		//	'esc' to quit
		case 27:
			exit(0);
			break;

		//	slowdown
		case ',':
			slowdownTravelers();
			ok = 1;
			break;

		//	speedup
		case '.':
			speedupTravelers();
			ok = 1;
			break;
		case ' ':
		//only debugging purposes
		travelerList[0].move(EAST);
		break;

		default:
			ok = 1;
			break;
	}
	if (!ok)
	{
		//	do something?
	}
}


//------------------------------------------------------------------------
//	You shouldn't have to touch this one.  Definitely if you don't
//	add the "producer" threads, and probably not even if you do.
//------------------------------------------------------------------------
void speedupTravelers(void)
{
	//	decrease sleep time by 20%, but don't get too small
	int newSleepTime = (8 * travelerSleepTime) / 10;
	
	if (newSleepTime > MIN_SLEEP_TIME)
	{
		travelerSleepTime = newSleepTime;
	}
}

void slowdownTravelers(void)
{
	//	increase sleep time by 20%
	travelerSleepTime = (12 * travelerSleepTime) / 10;
}




//------------------------------------------------------------------------
//	You shouldn't have to change anything in the main function besides
//	initialization of the various global variables and lists
//------------------------------------------------------------------------
int main(int argc, char** argv)
{
	//	We know that the arguments  of the program  are going
	//	to be the width (number of columns) and height (number of rows) of the
	//	grid, the number of travelers, etc.
	//	So far, I hard code-some values
	
	//numRows = argv[0];
	//numRows = argv[0];
	//numTravelers = argv[1];
	//grow_count = argv[2];
	numRows = 12;
	numCols = 14;
	numTravelers = 4;
	numLiveThreads = 0;
	numTravelersDone = 0;

	//	Even though we extracted the relevant information from the argument
	//	list, I still need to pass argc and argv to the front-end init
	//	function because that function passes them to glutInit, the required call
	//	to the initialization of the glut library.
	initializeFrontEnd(argc, argv);
	
	//	Now we can do application-level initialization
	initializeApplication();

	launchTime = time(NULL);
	
	for(unsigned i = 0; i < numTravelers; i++ )
	{
		Traveler* t = &travelerList[i];
		t -> keepRunning = true;
		pthread_create( &t -> threadID, NULL, thread_run, t);
	}
	//last parameter need to be change
	//
	//	Now we enter the main loop of the program and to a large extend
	//	"lose control" over its execution.  The callback functions that 
	//	we set up earlier will be called when the corresponding event
	//	occurs
	glutMainLoop();
	
	//	Free allocated resource before leaving (not absolutely needed, but
	//	just nicer.  Also, if you crash there, you know something is wrong
	//	in your code.
	for (unsigned int i=0; i< numRows; i++)
		free(grid[i]);
	free(grid);
	for (int k=0; k<MAX_NUM_MESSAGES; k++)
		free(message[k]);
	free(message);
	
	//	This will probably never be executed (the exit point will be in one of the
	//	call back functions).
	return 0;
}


//==================================================================================
//
//	This is a function that you have to edit and add to.
//
//==================================================================================


void initializeApplication(void)
{
	//	Initialize some random generators
	rowGenerator = uniform_int_distribution<unsigned int>(0, numRows-1);
	colGenerator = uniform_int_distribution<unsigned int>(0, numCols-1);

	//	Allocate the grid
	grid = new SquareType*[numRows];
	for (unsigned int i=0; i<numRows; i++)
	{
		grid[i] = new SquareType[numCols];
		for (unsigned int j=0; j< numCols; j++)
			grid[i][j] = FREE_SQUARE;
		
	}
	pthread_mutex_init(&global_gridLock, nullptr);


	message = new char*[MAX_NUM_MESSAGES];
	for (unsigned int k=0; k<MAX_NUM_MESSAGES; k++)
		message[k] = new char[MAX_LENGTH_MESSAGE+1];
		
	//---------------------------------------------------------------
	//	All the code below to be replaced/removed
	//	I initialize the grid's pixels to have something to look at
	//---------------------------------------------------------------
	//	Yes, I am using the C random generator after ranting in class that the C random
	//	generator was junk.  Here I am not using it to produce "serious" data (as in a
	//	real simulation), only wall/partition location and some color
	srand((unsigned int) time(NULL));

	//	generate a random exit
	exitPos = getNewFreePosition();
	grid[exitPos.row][exitPos.col] = EXIT;

	//	Generate walls and partitions

	//12/8/20 - Code below need to be add back once traveler is moving
	generateWalls();
	partitionList = generatePartitions();
	printf("Number of partitions = %lu", partitionList.size());
	//	Initialize traveler info structs
	//	You will probably need to replace/complete this as you add thread-related data
	float** travelerColor = createTravelerColors(numTravelers);
	for (unsigned int k=0; k<numTravelers; k++) {
		GridPosition pos = getNewFreePosition();
		//	Note that treating an enum as a sort of integer is increasingly
		//	frowned upon, as C++ versions progress
		Direction dir = static_cast<Direction>(segmentDirectionGenerator(engine));

		TravelerSegment seg = {pos.row, pos.col, dir};
		Traveler traveler;
		traveler.index = k;
		traveler.segmentList.push_back(seg);
		grid[pos.row][pos.col] = TRAVELER;

		//	I add 0-n segments to my travelers
		unsigned int numAddSegments = segmentNumberGenerator(engine);
		TravelerSegment currSeg = traveler.segmentList[0];
		bool canAddSegment = true;
//cout << "Traveler " << k << " at (row=" << pos.row << ", col=" <<
//		pos.col << "), direction: " << dirStr(dir) << ", with up to " << numAddSegments << " additional segments" << endl;
//cout << "\t";

		for (unsigned int s=0; s<numAddSegments && canAddSegment; s++)
		{
			TravelerSegment newSeg = newTravelerSegment(currSeg, canAddSegment);
			if (canAddSegment)
			{
				traveler.segmentList.push_back(newSeg);
				currSeg = newSeg;
//cout << dirStr(newSeg.dir) << "  ";
			}
		}
//cout << endl;
		
		for (unsigned int c=0; c<4; c++)
			traveler.rgba[c] = travelerColor[k][c];
		
		travelerList.push_back(traveler);
	}
	
	//adding locks to each traveler
	for (unsigned int i = 0; i < travelerList.size() - 1; i++)
	{
		pthread_mutex_init(&(travelerList[i].travelerLock), nullptr);
	}
	//	free array of colors
	for (unsigned int k=0; k<numTravelers; k++)
		delete []travelerColor[k];
	delete []travelerColor;
}


//------------------------------------------------------
#if 0
#pragma mark -
#pragma mark Generation Helper Functions
#endif
//------------------------------------------------------

GridPosition getNewFreePosition(void)
{
	GridPosition pos;

	bool noGoodPos = true;
	while (noGoodPos)
	{
		unsigned int row = rowGenerator(engine);
		unsigned int col = colGenerator(engine);
		if (grid[row][col] == FREE_SQUARE)
		{
			pos.row = row;
			pos.col = col;
			noGoodPos = false;
		}
	}
	return pos;
}

Direction newDirection(Direction forbiddenDir)
{
	bool noDir = true;

	Direction dir = NUM_DIRECTIONS;
	while (noDir)
	{
		dir = static_cast<Direction>(segmentDirectionGenerator(engine));
		noDir = (dir==forbiddenDir);
	}
	return dir;
}


TravelerSegment newTravelerSegment(const TravelerSegment& currentSeg, bool& canAdd)
{
	TravelerSegment newSeg;
	switch (currentSeg.dir)
	{
		case NORTH:
			if (	currentSeg.row < numRows-1 &&
					grid[currentSeg.row+1][currentSeg.col] == FREE_SQUARE)
			{
				newSeg.row = currentSeg.row+1;
				newSeg.col = currentSeg.col;
				newSeg.dir = newDirection(SOUTH);
				grid[newSeg.row][newSeg.col] = TRAVELER;
				canAdd = true;
			}
			//	no more segment
			else
				canAdd = false;
			break;

		case SOUTH:
			if (	currentSeg.row > 0 &&
					grid[currentSeg.row-1][currentSeg.col] == FREE_SQUARE)
			{
				newSeg.row = currentSeg.row-1;
				newSeg.col = currentSeg.col;
				newSeg.dir = newDirection(NORTH);
				grid[newSeg.row][newSeg.col] = TRAVELER;
				canAdd = true;
			}
			//	no more segment
			else
				canAdd = false;
			break;

		case WEST:
			if (	currentSeg.col < numCols-1 &&
					grid[currentSeg.row][currentSeg.col+1] == FREE_SQUARE)
			{
				newSeg.row = currentSeg.row;
				newSeg.col = currentSeg.col+1;
				newSeg.dir = newDirection(EAST);
				grid[newSeg.row][newSeg.col] = TRAVELER;
				canAdd = true;
			}
			//	no more segment
			else
				canAdd = false;
			break;

		case EAST:
			if (	currentSeg.col > 0 &&
					grid[currentSeg.row][currentSeg.col-1] == FREE_SQUARE)
			{
				newSeg.row = currentSeg.row;
				newSeg.col = currentSeg.col-1;
				newSeg.dir = newDirection(WEST);
				grid[newSeg.row][newSeg.col] = TRAVELER;
				canAdd = true;
			}
			//	no more segment
			else
				canAdd = false;
			break;
		
		default:
			canAdd = false;
	}
	
	return newSeg;
}



//12/8/20
void Traveler::move(Direction dir, bool growTail)
{
//	cout << "I am Traveler " << this->index << " at location: row=" << segmentList[0].row << " col=" << segmentList[0].col << " with a move " << dir << endl;
	
	TravelerSegment &ts = segmentList[0];
	//New segment if tail is growing
	TravelerSegment newSeg;

	int prev_col = ts.col;
	int tail_col = segmentList[segmentList.size()-1].col;
	int prev_row = ts.row;
	int tail_row	= segmentList[segmentList.size()-1].row;
	Direction prev_dir = ts.dir;
	
	//chris help me with this code 12/10/20
	if(dir == EAST)
	{
		if(ts.col < numCols-1)
		{
			ts.col+=1;
		}
		
	}
	
	if(dir == WEST)
	{
		if(ts.col > 0)
		{
			ts.col-=1;
		}
		
	}
	
	if(dir == SOUTH)
	{
		if(ts.row < numRows-1)
		{
			ts.row+=1;
		}
		
	}
	
	if(dir == NORTH)
	{
		if(ts.row > 0)
		{
			ts.row-=1;
		}
		
	}
	ts.dir = dir;
	grid[ts.row][ts.col] = TRAVELER;
	//moving remaining segments

	if (growTail == true)
	{
		newSeg = segmentList[segmentList.size() - 1];
		//printf("d%", newSeg)
		segmentList.push_back(newSeg);
	}
	for(unsigned k = 1; k<segmentList.size(); k++)
	{

		//chris help me with this code 12/10/20
		//this is a link list so that the segment follows the head
		Direction temp = segmentList[k].dir;
		int temp_row = segmentList[k].row;
		int temp_col = segmentList[k].col;
		segmentList[k].row = prev_row;
		segmentList[k].col =  prev_col;
		segmentList[k].dir = prev_dir;	
		prev_dir = temp;
		prev_row = temp_row;
		prev_col = temp_col;
		
	}

	grid[tail_row][tail_col] = FREE_SQUARE;
	
	
}

bool SlidingPartition::move(int blockIndex)
{
	// makimng **copies** of partition endpoints
	GridPosition first = blockList[0];
	GridPosition last = blockList[blockList.size() - 1];

	long unsigned int partitionSize = this->blockList.size();
	bool moved = false;


	int verticalMoveUpSpaces = (int) partitionSize - blockIndex;					//The amount of spaces needed to make room by moving up
	int verticalMoveDownSpaces = blockIndex + 1;									//The amount of spaces needed to move down
	int horizontalMoveLeftSpaces = (int) partitionSize - blockIndex;				//The amount of spaces needed to move left
	int horizontalMoveRightSpaces = blockIndex + 1;									//The amount of spaces needed to move right

	if (this->isVertical)																//If partition is vertical, enter this half of move code
	{
		bool verticalMoveUpPossible = true;
		//First attempt to move partituon UP
		printf("Row = %d\n", blockList[blockIndex].row);
		printf("Spaces needed = %d\n", verticalMoveUpSpaces);
		if (first.row  >= verticalMoveUpSpaces)			//checking to see if partition is on edge of map AND if the map has enough room to move up
		{
			for (int k = 1; verticalMoveUpPossible && k <= verticalMoveUpSpaces; k++)
			{
SquareType type = grid[first.row - k][first.col];
				if (grid[first.row - k][first.col] != FREE_SQUARE)						//Checking needed spaces above to see if they are FreeSquare
				{
					printf("Not enough free spaces above\n");
					verticalMoveUpPossible = false;										//If a square is blocked, set value to false
					//break;
				}
			}	
			if (verticalMoveUpPossible)													//If all spaces are free, move up the correct amount of spaces
			{
				for (int x = 0; x < verticalMoveUpSpaces; x++)
				{
					first.row--;
					for (int i = 0; i < partitionSize; i++)								//For loop for moving each index of the blockList
					{
						blockList[i].row--;
					}
					grid[first.row][first.col] = VERTICAL_PARTITION;
					grid[last.row][last.col] = FREE_SQUARE;
				}
				printf("TRAVELER MOVED  PARTITION UP\n");

				return true;															//Set moved to true and exit
			}
		}
		else
		{
			printf("Not enough map room to move up\n");
			printf("%d\n", blockIndex);
			printf("%d\n", blockList[blockIndex].row);
		}
		

		bool verticalMoveDownPossible = true;
		if ((last.row + verticalMoveDownSpaces) < numRows)				//Given not enough spaces above partition, checking to see if map has enough spaces below partition
		{
			printf("There is enough map space for downward movement");
			for (int k = 1; verticalMoveDownPossible && k <= verticalMoveDownSpaces; k++)
			{
SquareType type = grid[last.row + k][last.col];
				if (grid[last.row + k][last.col] != FREE_SQUARE)						//Checking needed spaces below to see if they are FreeSquare
				{

					verticalMoveDownPossible = false;
				}
			}
			if (verticalMoveDownPossible)												//If all spaces are free, move down the correct amount of spaces BUT in reverse
			{
				for (int x = 0; x < verticalMoveDownSpaces; x++)
					
				{
					last.row++;
				
					for (int i = 0; i < partitionSize; i++)								//For loop for moving each index of the blockList
					{
						blockList[i].row++;
					}
					grid[first.row][first.col] = FREE_SQUARE;
					grid[last.row][last.col] = VERTICAL_PARTITION;
				}
				printf("TRAVELER MOVED PARTITION DOWN");
				return true;																//Set moved = true and exit
			}
		
		}
		printf("Not enough map room to move down\n");
	}
	else																				//Partiion is Horizontal
	{
		bool horizontalMoveLeftPossible = true;
		//First attempt to move partition Left
		if (first.col >= horizontalMoveLeftSpaces)			//checking to see if partition is on edge of map AND if the map has enough room to move Left
		{
			for (int k = 1; horizontalMoveLeftPossible && k <= horizontalMoveLeftSpaces; k++)
			{
SquareType type = grid[first.row][first.col-k];
				if (grid[first.row][first.col - k] != FREE_SQUARE)						//Checking needed spaces above to see if they are FreeSquare
				{
//					
					horizontalMoveLeftPossible = false;												//If a square is blocked, set value to false
					//break;
				}
			}
			if (horizontalMoveLeftPossible)													//If all spaces are free, move up the correct amount of spaces
			{
				for (int x = 0; x < horizontalMoveLeftSpaces; x++)
				{
					first.col--;
					last.col--;
					for (int i = 0; i < partitionSize; i++)								//For loop for moving each index of the blockList
					{
						blockList[i].col--;
					}
					grid[first.row][first.col] = HORIZONTAL_PARTITION;
					grid[last.row][last.col] = FREE_SQUARE;
				}
				printf("TRAVELER MOVED PARTIUTION LEFT\n");
				moved = true;
				return moved;															//Set moved to true and exit
			}
		}
		bool horizontalMoveRightPossible = true;
		printf("not enough free map space to move left\n");
		if ((last.col + horizontalMoveRightSpaces) < numCols)				//Given not enough spaces to the left of partition, checking to see if map has enough spaces to the right of partition
		{
			printf("There are enough spaces to move right");
			for (int k = 1; horizontalMoveRightPossible && k < horizontalMoveRightSpaces; k++)
			{
SquareType type = grid[last.row][last.col+k];
				if (grid[last.row ][last.col + k] != FREE_SQUARE)						//Checking needed spaces below to see if they are FreeSquare
				{
					printf("Not enough free spaces to the right\n");
					horizontalMoveRightPossible = false;
				
				}
			}
			if (horizontalMoveRightPossible)												//If all spaces are free, move down the correct amount of spaces
			{
				for (int x = 0; x < horizontalMoveRightSpaces; x++)
				{
					last.col++;
					first.col++;
					for (int i = 0; i < partitionSize; i++)								//For loop for moving each index of the blockList
					{
						blockList[i].col++;
					}
					grid[first.row][first.col] = FREE_SQUARE;
					grid[last.row][last.col] = HORIZONTAL_PARTITION;
				}
				printf("TRAVELER MOVED PARTITION RIGHT");
				return true;															//Set moved to true and exit
			}

		}
	printf("Not enough map space to move right\n");
	}
	
	//if code reaches here, the partition cannot be moved out of the way
	printf("could not move partition");
	return moved;
}

bool comparePositions(const GridPosition& pos1, const GridPosition& pos2)
{

	return (pos1.row == pos2.row && pos1.col == pos2.col);
}
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void* thread_run(void *data)
{
	Traveler* ts = (Traveler*)data;
	bool grow = false;
	bool moved = false;
	ts->num_moves = 1;
	
cout << endl;

	while (ts->keepRunning)
	{
		moved = false;
		
		if (neighbor_check(ts->segmentList[0].row, ts->segmentList[0].col, EXIT) != NO_DIRECTION)
		{
			ts->keepRunning = false;
			for (unsigned i = 0; i < ts->segmentList.size(); i++)
			{
				TravelerSegment* t = &ts->segmentList[i];
				grid[t->row][t->col] = FREE_SQUARE;
			}
			
			break;
		}
		
{
ostringstream outStr;
outStr << "Traveler " << ts->index << " goes for the lock (beginning loop)" << endl;
cout << outStr.str() << flush;
}
		pthread_mutex_lock(&global_gridLock);
{
ostringstream outStr;
outStr << "Traveler " << ts->index << " got the lock" << endl;
cout << outStr.str() << flush;
}

		//------------------------------------------------------------------------------------------------------------------------------------------------------------
		//	try to move a partition, if any
//		else
		{
			//If a vertical partition-------------------------------------------------------------------------
			Direction moveDirV = neighbor_check(ts->segmentList[0].row, ts->segmentList[0].col, VERTICAL_PARTITION);
			Direction moveDirH = neighbor_check(ts->segmentList[0].row, ts->segmentList[0].col, HORIZONTAL_PARTITION);
			//check block in moveDir (north,south,east,west) of Traveler Head to see if it is pointing to partition
			if (moveDirV != NO_DIRECTION && neighborInfront(ts->segmentList[0],moveDirV,VERTICAL_PARTITION))
			{
				printf("Traveler %d Checking to see if vertical partition can move\n", ts->index);

				//Getting position of partition segment touching traveler
				
				GridPosition position = neighbor_find(ts->segmentList[0].row, ts->segmentList[0].col, VERTICAL_PARTITION);
				
				printf("Partition Poisition is %d, %d\n", position.row, position.col);
				//search through partition list
				printf("%u\n",partitionList.size());
				for (int i = 0; !moved && i < partitionList.size(); i++)
				{
					//*******the j in this for loop is used to loop over the partitionList and find which block in the partition list the traveler is touching
					for (int j = 0; !moved && j < partitionList[i].blockList.size(); j++)
					{
						printf("%u %u\n", partitionList[i].blockList[j].row, partitionList[i].blockList[j].col);
						if (comparePositions(partitionList[i].blockList[j], position))
						{
							printf("Attempting to move vertical partition\n");
							//attempt to move partition
							printf("Index of partition = %d\n", j);
							moved = partitionList[i].move(j);
							if (moved) {
//								pthread_mutex_lock(&(ts->travelerLock));
								ts->move(moveDirV);
								ts->num_moves += 1;

{
ostringstream outStr;
outStr << "Traveler " << ts->index << " unlocks grid (move after partition move)" << endl;
cout << outStr.str() << flush;
}								pthread_mutex_unlock(&global_gridLock);

//								pthread_mutex_unlock(&(ts->travelerLock));
							}
						}
					}
				}
			}
			//If Horizontal Parition
			//moveDir = neighbor_check(ts->segmentList[0].row, ts->segmentList[0].col, HORIZONTAL_PARTITION);
			else if (!moved && moveDirH != NO_DIRECTION && neighborInfront(ts->segmentList[0], moveDirH, HORIZONTAL_PARTITION))
			{
				printf("Traveler %d Checking to see if horizontal partition can move\n", ts->index);

				//Getting position of partition segment touching traveler

				GridPosition position = neighbor_find(ts->segmentList[0].row, ts->segmentList[0].col, HORIZONTAL_PARTITION);
				
				printf("Partition Poisition is %d, %d\n", position.row, position.col);
				//search through partition list
				printf("%u\n", partitionList.size());
				for (int i = 0; !moved && i < partitionList.size(); i++)
				{
					for (int j = 0; !moved && j < partitionList[i].blockList.size(); j++)
					{
						printf("%u %u\n", partitionList[i].blockList[j].row, partitionList[i].blockList[j].col);
						if (comparePositions(partitionList[i].blockList[j], position))
						{
							printf("Attempting to move vertical partition\n");
							//attempt to move partition
							printf("Index of partition = %d\n", j);
							moved = partitionList[i].move(j);
							if (moved) {

								ts->move(moveDirH);
								ts->num_moves += 1;

{
ostringstream outStr;
outStr << "Traveler " << ts->index << " unlocks grid (move)" << endl;
cout << outStr.str() << flush;
}								pthread_mutex_unlock(&global_gridLock);

							}
						}
					}
				}
			}
		}
		if (!moved)
		{
			// if there is a move possible w/o moving a partition --> do it
			vector<Direction> dirList = get_free_spaces(ts->segmentList[0].row, ts->segmentList[0].col, ts->segmentList[0].dir);
			if (dirList.size() > 0)
			{
				Direction dir = dirList[(rand() % dirList.size())];
				if (ts->num_moves % grow_count == 0)
				{
					grow = true;
				}
				ts->move(dir, grow);
				ts->num_moves += 1;
{
ostringstream outStr;
outStr << "Traveler " << ts->index << " unlocks grid (free move)" << endl;
cout << outStr.str() << flush;
}
				pthread_mutex_unlock(&global_gridLock);
				grow = false;
				moved = true;
			}
		}
		
		
		if (!moved)
		{
{
ostringstream outStr;
outStr << "Traveler " << ts->index << " unlocks grid (dying)" << endl;
cout << outStr.str() << flush;
}
			pthread_mutex_unlock(&global_gridLock);
			ts->rgba[0] = 100;
			ts->rgba[1] = 100;
			ts->rgba[2] = 100;
			while (ts->rgba[0] > 10)
			{//change to gray and terminate the traveler once it reaches the exit
				ts->rgba[0] -= 1;
				ts->rgba[1] -= 1;
				ts->rgba[2] -= 1;
				usleep(500000);
			}
			//break;

{
ostringstream outStr;
outStr << "Traveler " << ts->index << " goes for the lock (dying 2)" << endl;
cout << outStr.str() << flush;
}
			pthread_mutex_lock(&global_gridLock);
{
ostringstream outStr;
outStr << "Traveler " << ts->index << " got the lock" << endl;
cout << outStr.str() << flush;
}

			for (unsigned i = 0; i < ts->segmentList.size(); i++)
			{
				TravelerSegment* t = &ts->segmentList[i];
				grid[t->row][t->col] = FREE_SQUARE;
			}
{
ostringstream outStr;
outStr << "Traveler " << ts->index << " unlocks grid (dying 2)" << endl;
cout << outStr.str() << flush;
}
			pthread_mutex_unlock(&global_gridLock);
			ts->segmentList.clear();
			ts->keepRunning = false;
		}
		//sleep
		usleep(500000);
	}

	//
	
return NULL;
}

Direction neighbor_check(unsigned int row, unsigned int col, SquareType type)
{//checks of the next square of its type
	if(col < numCols-1 && grid[row][col+1] == type)
	{
		return EAST;
	}
	if(col > 0 && grid[row][col-1] == type)
	{
		return WEST;
	}
	if(row < numRows-1 && grid[row+1][col] == type)
	{
		return SOUTH;
	}
	if(row > 0 && grid[row-1][col] == type)
	{
		return NORTH;
	}
	return NO_DIRECTION;
}

//Function to check if partition is INFRONT of traveler
bool neighborInfront(TravelerSegment head, Direction direction, SquareType partition)
{
	bool infront;
	if (direction == NORTH) {
		if (grid[head.row - 1][head.col] == partition) {
			infront = true;
			return infront;
		}
	}
	if (direction == SOUTH) {
		if (grid[head.row +1][head.col] == partition) {
			infront = true;
			return infront;
		}
	}
	if (direction == WEST) {
		if (grid[head.row][head.col - 1] == partition) {
			infront = true;
			return infront;
		}
	}
	if (direction == EAST) {
		if (grid[head.row][head.col + 1] == partition) {
			infront = true;
			return infront;
		}
	}
	return infront;
}
//Function to recieve position of neighbor partition

GridPosition neighbor_find(unsigned int row, unsigned int col, SquareType type)
{
	GridPosition position;
	if (col < numCols - 1 && grid[row][col + 1] == type)
	{
		position.row = row;
		position.col = col + 1;
		return position;
	}
	if (col > 0 && grid[row][col - 1] == type)
	{

		position.row = row;
		position.col = col - 1;
		return position;
	}
	if (row < numRows - 1 && grid[row + 1][col] == type)
	{
		position.row = row +1;
		position.col = col;
		return position;
	}
	if (row > 0 && grid[row - 1][col] == type)
	{
		position.row = row - 1;
		position.col = col;
		return position;
	}
	printf("Neighbor not found");
	return position;
}




vector<Direction> get_free_spaces(unsigned int row, unsigned int col, Direction dir )
{
	vector<Direction> dirs;
	//check for if not north than it can go south
	if(row < numRows-1 && dir != NORTH && grid[row+1][col] == FREE_SQUARE)
	{
		dirs.push_back(SOUTH);
	}
	// c//check for if not south than it can go north
	if(row > 0 && dir != SOUTH && grid[row-1][col] == FREE_SQUARE)
	{
		dirs.push_back(NORTH);
	}
	//check for if not west than it can go east
	if(col < numCols-1 && dir != WEST && grid[row][col+1] == FREE_SQUARE)
	{
		dirs.push_back(EAST);
	}
	//check for if not east than it can go west
	if(col > 0 && dir != EAST && grid[row][col-1] == FREE_SQUARE)
	{
		dirs.push_back(WEST);
	}
	
	return dirs;
}

void generateWalls(void)
{
	const unsigned int NUM_WALLS = (numCols+numRows)/4;

	//	I decide that a wall length  cannot be less than 3  and not more than
	//	1/4 the grid dimension in its Direction
	const unsigned int MIN_WALL_LENGTH = 3;
	const unsigned int MAX_HORIZ_WALL_LENGTH = numCols / 3;
	const unsigned int MAX_VERT_WALL_LENGTH = numRows / 3;
	const unsigned int MAX_NUM_TRIES = 20;

	bool goodWall = true;
	
	//	Generate the vertical walls
	for (unsigned int w=0; w< NUM_WALLS; w++)
	{
		goodWall = false;
		
		//	Case of a vertical wall
		if (headsOrTails(engine))
		{
			//	I try a few times before giving up
			for (unsigned int k=0; k<MAX_NUM_TRIES && !goodWall; k++)
			{
				//	let's be hopeful
				goodWall = true;
				
				//	select a column index
				unsigned int HSP = numCols/(NUM_WALLS/2+1);
				unsigned int col = (1+ unsignedNumberGenerator(engine)%(NUM_WALLS/2-1))*HSP;
				unsigned int length = MIN_WALL_LENGTH + unsignedNumberGenerator(engine)%(MAX_VERT_WALL_LENGTH-MIN_WALL_LENGTH+1);
				
				//	now a random start row
				unsigned int startRow = unsignedNumberGenerator(engine)%(numRows-length);
				for (unsigned int row=startRow, i=0; i<length && goodWall; i++, row++)
				{
					if (grid[row][col] != FREE_SQUARE)
						goodWall = false;
				}
				
				//	if the wall first, add it to the grid
				if (goodWall)
				{
					for (unsigned int row=startRow, i=0; i<length && goodWall; i++, row++)
					{
						grid[row][col] = WALL;
					}
				}
			}
		}
		// case of a horizontal wall
		else
		{
			goodWall = false;
			
			//	I try a few times before giving up
			for (unsigned int k=0; k<MAX_NUM_TRIES && !goodWall; k++)
			{
				//	let's be hopeful
				goodWall = true;
				
				//	select a column index
				unsigned int VSP = numRows/(NUM_WALLS/2+1);
				unsigned int row = (1+ unsignedNumberGenerator(engine)%(NUM_WALLS/2-1))*VSP;
				unsigned int length = MIN_WALL_LENGTH + unsignedNumberGenerator(engine)%(MAX_HORIZ_WALL_LENGTH-MIN_WALL_LENGTH+1);
				
				//	now a random start row
				unsigned int startCol = unsignedNumberGenerator(engine)%(numCols-length);
				for (unsigned int col=startCol, i=0; i<length && goodWall; i++, col++)
				{
					if (grid[row][col] != FREE_SQUARE)
						goodWall = false;
				}
				
				//	if the wall first, add it to the grid
				if (goodWall)
				{
					for (unsigned int col=startCol, i=0; i<length && goodWall; i++, col++)
					{
						grid[row][col] = WALL;
					}
				}
			}
		}
	}
}


vector<SlidingPartition> generatePartitions(void)
{
	const unsigned int NUM_PARTS = (numCols+numRows)/4;
	//Added to return it
	//vector<SlidingPartition> partitionList;
	//	I decide that a partition length  cannot be less than 3  and not more than
	//	1/4 the grid dimension in its Direction
	const unsigned int MIN_PARTITION_LENGTH = 3;
	const unsigned int MAX_HORIZ_PART_LENGTH = numCols / 3;
	const unsigned int MAX_VERT_PART_LENGTH = numRows / 3;
	const unsigned int MAX_NUM_TRIES = 20;

	bool goodPart = true;

	for (unsigned int w=0; w< NUM_PARTS; w++)
	{
		goodPart = false;
		
		//	Case of a vertical partition
		if (headsOrTails(engine))
		{
			//	I try a few times before giving up
			for (unsigned int k=0; k<MAX_NUM_TRIES && !goodPart; k++)
			{
				//	let's be hopeful
				goodPart = true;
				
				//	select a column index
				unsigned int HSP = numCols/(NUM_PARTS/2+1);
				unsigned int col = (1+ unsignedNumberGenerator(engine)%(NUM_PARTS/2-2))*HSP + HSP/2;
				unsigned int length = MIN_PARTITION_LENGTH + unsignedNumberGenerator(engine)%(MAX_VERT_PART_LENGTH-MIN_PARTITION_LENGTH+1);
				
				//	now a random start row
				unsigned int startRow = unsignedNumberGenerator(engine)%(numRows-length);
				for (unsigned int row=startRow, i=0; i<length && goodPart; i++, row++)
				{
					if (grid[row][col] != FREE_SQUARE)
						goodPart = false;
				}
				
				//	if the partition is possible,
				if (goodPart)
				{
					//	add it to the grid and to the partition list
					SlidingPartition part;
					part.isVertical = true;
					for (unsigned int row=startRow, i=0; i<length && goodPart; i++, row++)
					{
						grid[row][col] = VERTICAL_PARTITION;
						GridPosition pos = {row, col};
						part.blockList.push_back(pos);
					}
					partitionList.push_back(part);
				}

				
			}
		}
		// case of a horizontal partition
		else
		{
			goodPart = false;
			
			//	I try a few times before giving up
			for (unsigned int k=0; k<MAX_NUM_TRIES && !goodPart; k++)
			{
				//	let's be hopeful
				goodPart = true;
				
				//	select a column index
				unsigned int VSP = numRows/(NUM_PARTS/2+1);
				unsigned int row = (1+ unsignedNumberGenerator(engine)%(NUM_PARTS/2-2))*VSP + VSP/2;
				unsigned int length = MIN_PARTITION_LENGTH + unsignedNumberGenerator(engine)%(MAX_HORIZ_PART_LENGTH-MIN_PARTITION_LENGTH+1);
				
				//	now a random start row
				unsigned int startCol = unsignedNumberGenerator(engine)%(numCols-length);
				for (unsigned int col=startCol, i=0; i<length && goodPart; i++, col++)
				{
					if (grid[row][col] != FREE_SQUARE)
						goodPart = false;
				}
				
				//	if the wall first, add it to the grid and build SlidingPartition object
				if (goodPart)
				{
					SlidingPartition part;
					part.isVertical = false;
					for (unsigned int col=startCol, i=0; i<length && goodPart; i++, col++)
					{
						grid[row][col] = HORIZONTAL_PARTITION;
						GridPosition pos = {row, col};
						part.blockList.push_back(pos);
					}
					partitionList.push_back(part);
				}
			}
		}
	}

	return partitionList;
}

