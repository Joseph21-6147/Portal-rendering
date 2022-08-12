// Portal Rendering video (Doom style 3D engine)
// =============================================
//
// Idea and video by Bisqwit - thanks!!
// Youtube: https://youtu.be/HQYsFshbkYw
//
// Implementation of part 1b - LoadData() and UnloadData()
//
// Joseph21, august 12, 2022
//
// Dependencies:
//   *  olcPixelGameEngine.h - (olc::PixelGameEngine header file) by JavidX9 (see: https://github.com/OneLoneCoder/olcPixelGameEngine)
//   *  map-clear.txt - this is a map definition from Bisqwit. You could also use some other map, as long as it's compatible


/* Short description
   -----------------
   This implementation is the follow up of implementation part 1a. See the description of that implementation as well (and check on
   the differences between that cpp file and this one).

   After studying and implementing Bisqwits code, I decided there could be a more comprensive and modular
   build up of the code. With this progressive series of implementations (where each part builts upop the
   previous part) I take a shot at it.

   This part handles data input. It reads a text file containing vertices, sector info and (initial) player info. Please check on the
   data file itself for an explanation of structure of the data.
   A number of auxiliary parsing functions is implemented, leading up to the LoadData() and UnloadData() functions that are
   called from OnUserCreate() and OnUserDestroy() respectively.

   For other portal rendering introductions I'd suggest to check on the following video's:

       Let's program Doom - Part 1 (by 3DSage) - https://www.youtube.com/watch?v=huMO4VQEwPc
       Future video's in this series (yet to appear)

    Have fun!
 */


#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

// ====================   Constants   ==============================

#define WIDTH     608          // Define window size
#define HEIGHT    480
#define PIXEL       1          // pixel size

#define MAP_FILE  "map-clear.txt"      // the file containing the map definition

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

// ====================   Prototypes   ==============================

// prototypes, implementations for these functions are below main() function
bool LoadData( std::string sFileName, bool bDebugOutput = false );           // if you set the flag LoadData() will output the vertex list, sector list and player info
bool UnloadData();

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
        LoadData( "map-clear.txt", true );    // if true is passed the input data is printed to console

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
        UnloadData();

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


// ====================   Data Input and Parsing Functions   ==============================

// Trims leading spaces from sToTrim (if there are any)
void trim_leading_spaces( std::string &sToTrim ) {
    while (sToTrim.length() > 0 && sToTrim[0] == ' ') {
        sToTrim = sToTrim.substr( 1 );
    }
}

// Returns the front token from "input_to_be_adapted", using "delim" as delimiter.
// The input string becomes shorter as a result, and may even become empty.
// Leading spaces are trimmed before and after the extraction of the token.
std::string get_front_token( const std::string &delim, std::string &input_to_be_adapted ) {

    std::string token = "";
    // trim leading spaces if any
    trim_leading_spaces( input_to_be_adapted );
    // find index of next delimiter
    size_t splitIndex = input_to_be_adapted.find( delim );

    if (splitIndex != std::string::npos) {
        // next delimiter was found - strip off token and adapt input string
        token = input_to_be_adapted.substr( 0, splitIndex );
        input_to_be_adapted = input_to_be_adapted.substr( splitIndex + 1 );
        // trim leading spaces if any
        trim_leading_spaces( input_to_be_adapted );
    } else {
        // next delimiter was NOT found ...
        if (input_to_be_adapted.length() == 0) {
            // ... if the parse string is empty, set token empty ...
            token = "";
        } else {
            // ... if the parse string is not empty, set token to parse string...
            token = input_to_be_adapted;
            // ... and set parse string to empty
            input_to_be_adapted = "";
        }
    }
    return token;
}

// Parses a text line sLine that is assumed to be a player line, and fills struct p with it.
// A player line looks like "player	2  6	0	3", where (2, 6) [ both floats ] is the
// players location, 0 [ float ] is the player angle and 3 [ int ] is the sector where the
// player currently is.
bool ParsePlayerLine( Player &p, std::string &sLine ) {
    bool bSuccess = true;
    int tokenCount = 0;
    std::string sCache[5];

    // parse all tokens from the input line and store them in sCache
    do {
        std::string sToken = get_front_token( " ", sLine );
        sCache[tokenCount] = sToken;
        tokenCount += 1;
    } while (sLine.length() > 0 && tokenCount < 5);

    // error checking on parse result
    if (tokenCount != 5)    { bSuccess = false; std::cout << "ERROR - ParsePlayerLine() --> expected 5 tokens, got: " << tokenCount << std::endl; }
    if (sLine.length() > 0) { bSuccess = false; std::cout << "ERROR - ParsePlayerLine() --> parsed line contains residual characters: " << sLine << std::endl; }

    if (bSuccess) {
        p.where.x = std::stof( sCache[1] );     // sCache[0] contains keyword "player", which is ignored
        p.where.y = std::stof( sCache[2] );
        p.angle   = std::stof( sCache[3] );
        p.sector  = std::stoi( sCache[4] );
        p.anglesin = sin( p.angle );
        p.anglecos = cos( p.angle );
    }
    return bSuccess;
}

// Parses a text line sLine that is assumed to be a vertex line, and fills std::vector data with it.
// A vertex line looks like "vertex	11.5	9 11 13 13.5 17.5", having a variable number of tokens after
// the keywoard "vertex" [ all floats ], where 11.5 is the vertex y coordinate, the others are x coordinates
bool ParseVertexLine( std::vector<xy> &data, std::string &sLine ) {
    bool bSuccess = true;
    int tokenCount = 0;
    std::vector<std::string> sCache;

    // parse all tokens from the input line and store them in sCache
    do {
        std::string sToken = get_front_token( " ", sLine );
        sCache.push_back( sToken );
        tokenCount += 1;
    } while (sLine.length() > 0);

    // error checking on parse result
    if (sLine.length() > 0) { bSuccess = false; std::cout << "ERROR - ParseVertexLine() --> parsed line contains residual characters: " << sLine << std::endl; }

    if (bSuccess) {
        float y_coord = std::stof( sCache[1] );
        for (int i = 2; i < (int)sCache.size(); i++) {
            data.push_back( { std::stof( sCache[i] ), y_coord } );
        }
    }
    return bSuccess;
}

// Parses a text line sLine that is assumed to be a sector line, and fills Sector struct res with it.
// A sector line looks like "sector	0 20	 3 14 29 49             -1 1 11 22 ", having a variable
// number of tokens after the keywoard "sector". The first two numbers are floor and ceiling height [ floats ]
// for this sector, the first half of the remaining numbers [int] are indices into the vertex list inData.
// The second half of the remaining numbers [int] are indices into the sector list
bool ParseSectorLine( std::vector<xy> &inData, Sector &res, std::string &sLine ) {
    bool bSuccess = true;
    int tokenCount = 0;
    std::vector<std::string> sCache;

    // parse all tokens from the input line and store them in sCache
    do {
        std::string sToken = get_front_token( " ", sLine );
        sCache.push_back( sToken );
        tokenCount += 1;
    } while (sLine.length() > 0);

    // error checking on parse result
    if (sLine.length() > 0)     { bSuccess = false; std::cout << "ERROR - ParseVertexLine() --> parsed line contains residual characters: " << sLine << std::endl; }
    if (sCache.size() % 2 != 1) { bSuccess = false; std::cout << "ERROR - ParseVertexLine() --> nr of tokens is incorrect (not odd): " << sCache.size() << std::endl; }

    if (bSuccess) {
        res.floor = std::stof( sCache[1] );
        res.ceil  = std::stof( sCache[2] );
        int halfWay = (sCache.size() - 3) / 2;

        for (int i = 0; i < halfWay; i++) {
            int vertIx = std::stoi( sCache[i + 3          ] );
            int nghbIx = std::stoi( sCache[i + 3 + halfWay] );
            res.vertex.push_back( inData[vertIx] );
            res.neighbors.push_back( nghbIx );
        }
    }
    return bSuccess;
}

// this function prints the contents of the global sectors list and the global player struct to console
void PrintLoadedData() {
    std::cout << "Sector Data" << std::endl;
    std::cout << "===========" << std::endl;

    for (int i = 0; i < (int)sectors.size(); i++) {
        Sector &s = sectors[i];
        std::cout << "Sector index: " << i;
        std::cout << " floor = " << s.floor << ", ceiling = " << s.ceil << std::endl;
        for (int i = 0; i < (int)s.vertex.size(); i++) {
            std::cout << "    Vertex index: " << i;
            std::cout << ", point   : (" << s.vertex[i].x << ", " << s.vertex[i].y << ")";
            std::cout << ", portal to: " << s.neighbors[i] << std::endl;
        }
    }
    std::cout << std::endl;
    std::cout << "Player Data" << std::endl;
    std::cout << "===========" << std::endl;
    std::cout << "    location ("  << player.where.x << ", " << player.where.y << ")" << std::endl;
    std::cout << "    angle    "   << player.angle   << std::endl;
    std::cout << "    sector   "   << player.sector  << std::endl;
}

// This function reads the input file, and converts it into
//   * a number of sectors in the global sectors list
//   * the initial player data
// It uses its own local vertex list to build the sectors.
// Returns true upon succes, false otherwise.
bool LoadData( std::string sFileName, bool bDebugOutput) {

    bool bSuccess = true;
    std::ifstream theFile;
    theFile.open( sFileName );

    if (!theFile.is_open()) {
        std::cout << "ERROR: LoadData() --> could not open file: " << sFileName << std::endl;
        bSuccess = false;
    } else {
        // temp list of vertices, needed to build sectors from
        std::vector<xy> pointData;
        // iterate over all lines of the input file
        std::string theLine;
        while (getline( theFile, theLine ) && bSuccess) {
            // skip empty lines
            if (!theLine.empty()) {
                // input data may contain tab characters, replace them with spaces
                std::replace( theLine.begin(), theLine.end(), '\t', ' ' );
                // determine what type of line it is using the first character, and act accordingly
                switch (theLine[0]) {
                    case '#': // comment line, ignore it
                        break;
                    case 'v': // vertex line
                        bSuccess = ParseVertexLine( pointData, theLine );
                        break;
                    case 's': // sector line
                        {
                            Sector aux;
                            bSuccess = ParseSectorLine( pointData, aux, theLine );
                            sectors.push_back( aux );
                        }
                        break;
                    case 'p': // player line
                        bSuccess = ParsePlayerLine( player, theLine );
                        break;
                    default:  // other lines are not allowed
                        std::cout << "ERROR: LoadData() --> parse line not recognized: " << theLine << std::endl;
                        bSuccess = false;
                }
            }
        }
        theFile.close();

        if (bDebugOutput) {
            // if the debug output flag is set, output the temp vertex list ...
            std::cout << "Vertex Data" << std::endl;
            std::cout << "===========" << std::endl;
            for (int i = 0; i < (int)pointData.size(); i++) {
                xy aux = pointData[i];
                std::cout << "Vertex index: " << i;
                std::cout << " = (" << aux.x << ", " << aux.y << ") " << std::endl;
            }
            std::cout << std::endl;
            // ... and output the sector list and player initial settings
            PrintLoadedData();
        }
    }
    return bSuccess;
}

// just clear the sectors list
bool UnloadData() {
    sectors.clear();
    return true;
}

