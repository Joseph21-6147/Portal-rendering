// Portal Rendering video (Doom style 3D engine)
// =============================================
//
// Idea and video by Bisqwit - thanks!!
// Youtube: https://youtu.be/HQYsFshbkYw
//
// Implementation of part 1a - Setup & Datastructures for sectors and player
//
// Joseph21, august 12, 2022
//
// Dependencies:
//   *  olcPixelGameEngine.h - (olc::PixelGameEngine header file) by JavidX9 (see: https://github.com/OneLoneCoder/olcPixelGameEngine)


/* Short description
   -----------------
   This is the first part of this implementation series. There are no implementations for previous parts.

   After studying and implementing Bisqwits code, I decided there could be a more comprensive and modular
   build up of the code. With this progressive series of implementations (where each part builts upop the
   previous part) I take a shot at it.

   This first implementation starts with setting up the famous Pixel Game Engine (PGE for short that was
   created by Javidx9 and the olc community - thanks again!
   This code file also contains the basis for the portal rendering algorithm, being the datastructures for
   sectors, and a datastructure for the player info.

   For other portal rendering introductions I'd suggest to check on the following video's:

       Let's program Doom - Part 1 (by 3DSage) - https://www.youtube.com/watch?v=huMO4VQEwPc
       Future video's in this series (yet to appear)

    Have fun!
 */


#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

// ====================   Constants   ==============================

#define WIDTH   608            // Define window size
#define HEIGHT  480
#define PIXEL     1            // pixel size

// ====================   Data Structures   ==============================

// 2D and 3D coordinate types
struct xy  { float x = 0.0f, y = 0.0f;           };
struct xyz { float x = 0.0f, y = 0.0f, z = 0.0f; };

// all info to describe one sector
struct Sector {
    float            floor = 0.0f,     // the floor and ceiling height of a sector is constant
                     ceil  = 0.0f;
    std::vector<xy>  vertex;           // each vertex has an x and y coordinate
    std::vector<int> neighbors;        // each edge may have a corresponding neighboring sector
    // IMPORTANT NOTE: the neigbor corresponding to the edge between vertex n and vertex n+1
    //                 is indexed as neigbor[ n+1 ]!!
};
// global list of sectors
std::vector<Sector> sectors;

// player info
struct Player {
    xyz where,                         // current position
        velocity;                      // current motion vector
    float angle    = 0.0f,             // looking towards (and sin() and cos() thereof
          anglesin = 0.0f,
          anglecos = 1.0f,
          yaw      = 0.0f;
    int sector = 0;                    // which sector the player is currently in
} player;

// ====================   PGE derived class DoomEngine   ==============================

// DoomEngine class inherits from PGE for the graphics engine
class DoomEngine : public olc::PixelGameEngine
{
public:
	DoomEngine() {
		sAppName = "DoomEngine [Bisqwit] - implementation Joseph21";
	}

public:
	bool OnUserCreate() override {
		// Called once at the start, so create things here
		return true;
	}

	bool OnUserUpdate(float fElapsedTime) override {
		// Called once per frame, draws random coloured pixels (for now)
		for (int x = 0; x < ScreenWidth(); x++)
			for (int y = 0; y < ScreenHeight(); y++)
				Draw(x, y, olc::Pixel(rand() % 256, rand() % 256, rand() % 256));
		return true;
	}

	bool OnUserDestroy() override {
        // your cleanup code here
        return true;
	}
};

// ====================   main()   ==============================

int main() {
	DoomEngine demo;
	if (demo.Construct( WIDTH / PIXEL, HEIGHT / PIXEL, PIXEL, PIXEL ))
		demo.Start();
	return 0;
}

