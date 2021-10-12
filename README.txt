To compile
g++ -g -Wall main.cpp gl_frontEnd.cpp utils.cpp dataTypes.h gl_frontEnd.h -lm -lGL -lglut -lpthread -o test

To change the size of the grid you have to change the code in the main. We didn't have the versions to take arguments and hard coded everything. You need to change the size and grid size in the main.

Note:
We stated that we did the extra credit of taking control of the traveler in Version1 but I am doing this just in case you skimmed over the part where I mention it. I only did the taking over traveler in Version1 and not any of the other versions. It uses the w-a-s-d keys to move.
