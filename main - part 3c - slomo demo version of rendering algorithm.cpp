// Portal Rendering video (Doom style 3D engine)
// =============================================
//
// Idea and video by Bisqwit - thanks!!
// Youtube: https://youtu.be/HQYsFshbkYw
//
// Implementation of part 3c - demo version with slow motion portal rendering
//
// Joseph21, august 15, 2022
//
// Dependencies:
//   *  olcPixelGameEngine.h - (olc::PixelGameEngine header file) by JavidX9 (see: https://github.com/OneLoneCoder/olcPixelGameEngine)
//   *  map-clear.txt - this is a map definition from Bisqwit. You could also use some other map, as long as it's compatible


/* Short description
   -----------------
   This implementation is built upon the implementation part 3b. See the description of that implementation as well (and check on
   the differences between that cpp file and this one).

   After studying and implementing Bisqwits code, I decided there could be a more comprehensive and modular
   build up of the code. With this progressive series of implementations (where each part builts upon the
   previous part) I take a shot at it.

   This part implements a special demo version that is aimed at demonstrating the 3D portal rendering approach. By adapting the DrawScreen()
   method, the rendering is not directly drawn to canvas, but the info needed to do that is put in a queue. This way there's full control at
   the pace and the way the rendering order is displayed. An additional rendering function RenderQueue() controls when a new frame is started
   and when a new sector is filled in. The call to DrawScreen() in OnUserUpdate() is replaced by a call to RenderQueue().

   You can use P to pause or +/- on numpad to alter the animation pace. You can use M to switch map displaying on or off.
   Off course the regular player movement works as well, however player location and angle is only used when the demo animation sequence
   starts over.

   For other portal rendering introductions I'd suggest to check on the following video's:

       Let's program Doom - Part 1 (by 3DSage) - https://www.youtube.com/watch?v=huMO4VQEwPc
       Future video's in this series (yet to appear)

    Have fun!
 */

#include <deque>

#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

// ====================   Constants   ==============================

#define WIDTH     608          // Define window size
#define HEIGHT    480
#define PIXEL       1          // pixel size

#define MAP_FILE  "map-clear.txt"      // the file containing the map definition

// Define various vision related constants
#define EyeHeight    6                 // Camera height from floor when standing
#define HeadMargin   1                 // How much room there is above camera before the head hits the ceiling
#define KneeHeight   2                 // How tall obstacles the player can simply walk over without jumping

#define hfov        (0.73f * HEIGHT)   // Affects the horizontal field of vision
#define vfov        (0.20f * HEIGHT)   // Affects the vertical field of vision

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

// ====================   Convenience functions   ==============================

// vxs(): Vector cross product
float vxs( float x0, float y0, float x1, float y1 ) { return ( x0 * y1 - x1 * y0 ); }
// Overlap(): Determine whether the two number ranges overlap
bool Overlap( float a0, float a1, float b0, float b1 ) { return ((std::min( a0, a1 ) <= std::max( b0, b1 )) && (std::min( b0, b1 ) <= std::max( a0, a1 ))); }
// IntersectBox(): Determine whether two 2D boxes intersect
bool IntersectBox( float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3 ) { return (Overlap( x0, x1, x2, x3 ) && Overlap( y0, y1, y2, y3 )); }
// PointSide(): Determine which side of a line the point is on. Return value: <0, =0 or >0
float PointSide( float px, float py, float x0, float y0, float x1, float y1 ) { return vxs( x1 - x0, y1 - y0, px - x0, py - y0 ); }
// Intersect(): Calculate the point of intersection between two lines
xy Intersect( float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4 ) {
    return {
        vxs( vxs( x1, y1, x2, y2 ), x1 - x2, vxs( x3, y3, x4, y4), x3 - x4) / vxs( x1 - x2, y1 - y2, x3 - x4, y3 - y4 ),
        vxs( vxs( x1, y1, x2, y2 ), y1 - y2, vxs( x3, y3, x4, y4), y3 - y4) / vxs( x1 - x2, y1 - y2, x3 - x4, y3 - y4 )
    };
}

// ====================   Prototypes   ==============================

// prototypes, implementations for these functions are below main() function
bool LoadData( std::string sFileName, bool bDebugOutput = false );           // if you set the flag LoadData() will output the vertex list, sector list and player info
bool UnloadData();

// ====================   PGE derived class DoomEngine   ==============================

// DoomEngine class inherits from PGE for the graphics engine
class DoomEngine : public olc::PixelGameEngine {

public:
	DoomEngine() {
		sAppName = "DoomEngine [Bisqwit] - implementation Joseph21";
	}

private:

/*
 * To be able to render in slo mo (like bisqwit does in his demo) I created a special version of DrawScreen() called
 * DrawScreen_queue() that doesn't render the vlines, but enqueues them in a global queue.
 * That way I can have full control on the pace in which the vlines are drawn.
 *
 * To do this I need a struct to contain the parameters that the regular vline() expects, and build
 * a queue of these structs.
 */
    struct vLineStruct {
        int x, y1, y2;
        olc::Pixel top, middle, bottom;
        int signal;
        // add a constructor so you can use emplace_back()
        vLineStruct( int _x, int _y1, int _y2, olc::Pixel _top, olc::Pixel _middle, olc::Pixel _bottom, int _signal ) {
            x = _x; y1 = _y1; y2 = _y2; top = _top; middle = _middle; bottom = _bottom; signal = _signal;
        }
    };
    std::deque<vLineStruct> vLineQueue;

    // dummy vline() function that doesn't render, but merely enqueues the vline info
    void vline_queue( int x, int y1, int y2, olc::Pixel top, olc::Pixel middle, olc::Pixel bottom ) {
        vLineQueue.emplace_back( x, y1, y2, top, middle, bottom, 0 );
    }

    // enqueues a signal record - to be used if DrawScreen_queue() pops a new sector from it's queueu
    void signal_queue() {
        vLineQueue.emplace_back( -1, -1, -1, olc::BLACK, olc::BLACK, olc::BLACK, 1 );
    }

    // vline(): Draw a vertical line on screen, with a different color pixel in top & bottom
    // note that there's a conditional - vline() renders nothing is y2 < y1
    void vline( int x, int y1, int y2, olc::Pixel top, olc::Pixel middle, olc::Pixel bottom ) {

        y1 = std::clamp( y1, 0, HEIGHT - 1 );
        y2 = std::clamp( y2, 0, HEIGHT - 1 );

        if (y2 == y1) {
            Draw( x, y1, middle );
        } else if (y2 > y1) {
            Draw(     x, y1,     top    );
            DrawLine( x, y1 + 1,
                      x, y2 - 1, middle );
            Draw(     x, y2,     bottom );
        }
    }

    // MovePlayer( dx, dy ): Moves the player by (dx, dy) in the map, and
    // also updates their anglesin / anglecos/sector properties properly
    void MovePlayer( float dx, float dy ) {

        float px = player.where.x, py = player.where.y;
        // Check if this movement crosses one of this sector's edges that have a neighboring sector on
        // the other side. Because the edge vertices of each sector are defined in clockwise order, PointSide()
        // will always return -1 for a point that is outside the sector and 0 or 1 for a point that is inside.
        Sector &sect = sectors[player.sector];
        int nrVertices = (int)sect.vertex.size();
        bool bFound = false;
        for (int s = 0; s < nrVertices && !bFound; s++) {
            int next_s = (s + 1) % nrVertices;
            xy &vert      = sect.vertex[     s];
            xy &next_vert = sect.vertex[next_s];
            if (
                (sect.neighbors[next_s] >= 0) &&
                IntersectBox( px, py, px + dx, py + dy, vert.x, vert.y, next_vert.x, next_vert.y ) &&
                (PointSide(           px + dx, py + dy, vert.x, vert.y, next_vert.x, next_vert.y) < 0.0f)
            ) {
                player.sector = sect.neighbors[next_s];
                bFound = true;
            }
        }
        player.where.x += dx;
        player.where.y += dy;

        // set fixed to EyeHeight (for now)
        player.where.z = EyeHeight;

        // make sure angle is in [0.0f, 2 * PI]
        while (player.angle <  0.0f                ) player.angle += 2.0f * 3.1415926535f;
        while (player.angle >= 2.0f * 3.1415926535f) player.angle -= 2.0f * 3.1415926535f;

        player.anglesin = sin( player.angle );
        player.anglecos = cos( player.angle );
    }

    void DrawScreen_queue() {

        // auxiliary struct
        struct item {
            int sectorno, sx1, sx2;
        };

        std::deque<item> sQueue;     // The queue contains portals to be processed still

        int ytop[WIDTH], ybottom[WIDTH];   // Keep track of remaining window (min & max) Y coordinates in each column of the screen
        for (int x = 0; x < WIDTH; x++) {
            ytop[x] = 0;
            ybottom[x] = HEIGHT - 1;
        }

        // Begin whole-screen rendering from where the player is
        sQueue.push_back( (item){ player.sector, 0, WIDTH - 1 } );

        do {
            // Pick a sector & slice from the queue to draw
            item now = sQueue.front();
            sQueue.pop_front();
            // push a record in the queue that signals the start of a new sector analysis
            signal_queue();

            Sector &sect = sectors[now.sectorno];
            // Render each wall of this sector that is facing towards player
            int nrVertices = sect.vertex.size();
            for (int s = 0; s < nrVertices; s++) {
                int next_s = (s + 1) % nrVertices;
                xy &vert      = sect.vertex[     s];
                xy &next_vert = sect.vertex[next_s];
                // Acquire the x, y coordinates of the two endpoints (vertices) of this edge of the sector
                // Transform the vertices into the player's view
                float vx1 =      vert.x - player.where.x, vy1 =      vert.y - player.where.y;
                float vx2 = next_vert.x - player.where.x, vy2 = next_vert.y - player.where.y;
                // Rotate them around the player's view
                float pcos = player.anglecos, psin = player.anglesin;
                float tx1 = vx1 * psin - vy1 * pcos, tz1 = vx1 * pcos + vy1 * psin;
                float tx2 = vx2 * psin - vy2 * pcos, tz2 = vx2 * pcos + vy2 * psin;
                // Is the wall at least partially in front of the player?
                if (tz1 <= 0 && tz2 <= 0) continue;     // [ if not - skip this iteration ]

                // If it is partially behind the player, clip it against player's view frustum
                if (tz1 <= 0 || tz2 <= 0) {
                    float nearz = 0.0001f, farz = 5.0f, nearside = 0.00001f, farside = 20.0f;
                    // Find an intersection between the wall and the approximate edges of player's view
                    xy i1 = Intersect( tx1, tz1, tx2, tz2, -nearside, nearz, -farside, farz );
                    xy i2 = Intersect( tx1, tz1, tx2, tz2,  nearside, nearz,  farside, farz );
                    if (tz1 < nearz) { if (i1.y > 0) { tx1 = i1.x; tz1 = i1.y; } else { tx1 = i2.x; tz1 = i2.y; } }
                    if (tz2 < nearz) { if (i1.y > 0) { tx2 = i1.x; tz2 = i1.y; } else { tx2 = i2.x; tz2 = i2.y; } }
                }

                // Do perspective transformation
                float xscale1 = hfov / tz1, yscale1 = vfov / tz1;  int x1 = WIDTH / 2 - int( tx1 * xscale1 );
                float xscale2 = hfov / tz2, yscale2 = vfov / tz2;  int x2 = WIDTH / 2 - int( tx2 * xscale2 );
                if (x1 >= x2 || x2 < now.sx1 || x1 > now.sx2) continue;   // Only render if it's visible
                // Acquire the floor and ceiling heights, relative to where the player's view is
                float yceil  = sect.ceil  - player.where.z;
                float yfloor = sect.floor - player.where.z;

                // Check the edge type, neighbor = -1 means wall, other = boundary between two sectors
                int neighbor = sect.neighbors[next_s];

                float nyceil  = 0,
                      nyfloor = 0;
                if (neighbor >= 0) {  // Is another sector showing through this portal?
                    nyceil  = sectors[neighbor].ceil  - player.where.z;
                    nyfloor = sectors[neighbor].floor - player.where.z;
                }

                // Project our ceiling & floor heights into screen coordinates (Y coordinate)
                int y1a = HEIGHT / 2 - int( yceil * yscale1 ), y1b = HEIGHT / 2 - int( yfloor * yscale1 );
                int y2a = HEIGHT / 2 - int( yceil * yscale2 ), y2b = HEIGHT / 2 - int( yfloor * yscale2 );
                // The same for the neighboring sector
                int ny1a = HEIGHT / 2 - int( nyceil * yscale1 ), ny1b = HEIGHT / 2 - int( nyfloor * yscale1 );
                int ny2a = HEIGHT / 2 - int( nyceil * yscale2 ), ny2b = HEIGHT / 2 - int( nyfloor * yscale2 );

                // Render the wall
                int beginx = std::max( x1, now.sx1 ), endx = std::min( x2, now.sx2 );
                for (int x = beginx; x <= endx; x++) {
                    // Acquire the Y coordinates for our ceiling & floor for this X coordinate. Clamp them.
                    int ya = (x - x1) * (y2a - y1a) / (x2 - x1) + y1a, cya = std::clamp( ya, ytop[x], ybottom[x] );  // top
                    int yb = (x - x1) * (y2b - y1b) / (x2 - x1) + y1b, cyb = std::clamp( yb, ytop[x], ybottom[x] );  // bottom

                    // Render ceiling: everything above this sector's ceiling height
                    vline_queue( x, ytop[x], cya - 1, olc::DARK_GREY, olc::VERY_DARK_GREY, olc::DARK_GREY );
                    // Render floor: everything below this sector's floor height
                    vline_queue( x, cyb + 1, ybottom[x], olc::BLUE, olc::DARK_BLUE, olc::BLUE );

                    // Is there another sector behind this edge?
                    if (neighbor >= 0) {
                        // Same for _their_ floor and ceiling
                        int nya = (x - x1) * (ny2a - ny1a) / (x2 - x1) + ny1a, cnya = std::clamp( nya, ytop[x], ybottom[x] );  // top
                        int nyb = (x - x1) * (ny2b - ny1b) / (x2 - x1) + ny1b, cnyb = std::clamp( nyb, ytop[x], ybottom[x] );  // bottom

                        // If our ceiling is higher than their ceiling, render upper wall
                        vline_queue( x, cya, cnya - 1, olc::BLACK, ((x == x1 || x == x2) ? olc::BLACK : olc::GREY), olc::BLACK );     // Between our and their ceiling
                        ytop[x] = std::clamp( std::max( cya, cnya ), ytop[x], HEIGHT - 1 );         // Shrink remaining window below these ceilings

                        // If our floor is lower than their floor, render bottom wall
                        vline_queue( x, cnyb + 1, cyb, olc::BLACK, ((x == x1 || x == x2) ? 0 : olc::Pixel( 191, 64, 191 )), olc::BLACK );     // Between their and our floor
                        ybottom[x] = std::clamp( std::min( cyb, cnyb ), 0, ybottom[x] );            // Shrink remaining window above these floors

                        // draw remaining part in red
                        vline_queue( x, ytop[x], ybottom[x], olc::RED, olc::DARK_RED, olc::RED );

                    } else {
                        // There's no neighbor. Render wall from top (cya = ceiling level) to bottom (cyb = floor level)
                        vline_queue( x, cya, cyb, olc::BLACK, ((x == x1 || x == x2) ? olc::BLACK : olc::GREY), olc::BLACK );
                    }
                }
                // Schedule the neighboring sector for rendering within the window formed by this wall
                if (neighbor >= 0 && endx >= beginx) {
                    sQueue.push_back( (item) { neighbor, beginx, endx } );
                }
            }  // for s in sector's edges
        } while (!sQueue.empty());  // render any other queued sectors
    }

#define SLOMO_RENDER_SPEED 3
int  glb_render_speed = SLOMO_RENDER_SPEED;
bool glb_paused = false;
bool glb_showmap = false;

    void RenderQueue() {
        if (!glb_paused) {
            if (vLineQueue.empty()) {
                // the queue is empty, meaning previous frame is finished. Fill queue again and clear screen
                DrawScreen_queue();
                Clear( olc::BLACK );
            } else {
                for (int j = 0; j < glb_render_speed; j++) {
                    if (!vLineQueue.empty()) {
                        vLineStruct ri = vLineQueue.front();
                        vLineQueue.pop_front();
                        if (ri.signal > 0) {
                            // the record was a signal that a new sector has started
                            // render all the slices from this sector green before continuing
                            int nQsize = vLineQueue.size();
                            bool bGoOn = true;
                            for (int i = 0; i < nQsize && bGoOn; i++) {
                                if (vLineQueue[i].signal > 0) {
                                    bGoOn = false;
                                } else {
                                    vline( vLineQueue[i].x, vLineQueue[i].y1, vLineQueue[i].y2, olc::GREEN, olc::DARK_GREEN, olc::GREEN );
                                }
                            }
                        } else {
                            vline( ri.x, ri.y1, ri.y2, ri.top, ri.middle, ri.bottom );
                        }
                    }
                }
            }
        }
    }

    int moving = 0;

public:
	bool OnUserCreate() override {

		// Called once at the start, so create things here
        LoadData( "map-clear.txt", false );    // if true is passed the input data is printed to console

		return true;
	}

	bool OnUserUpdate(float fElapsedTime) override {

        bool done = false;

        // rendering
//        Clear( olc::BLACK );

//        DrawScreen();
        RenderQueue();

        if (glb_showmap) {
            float fMapScale = 5.0f;
            int   mapOrgX   = 10;
            int   mapOrgY   = 10;
            DrawMap2D(       fMapScale, mapOrgX, mapOrgY, olc::CYAN, false, false );
            DrawPlayerStats( fMapScale, mapOrgX, mapOrgY, olc::MAGENTA );
        }

        // Horizontal collision detection
        if (moving) {
            float px = player.where.x,    py = player.where.y;
            float dx = player.velocity.x, dy = player.velocity.y;

            Sector *sect = &sectors[player.sector];
            // Check if the players is about to cross one of the sector's edges
            int nrPoints = sect->vertex.size();
            for (int s = 0; s < nrPoints; s++) {
                int next_s = (s + 1) % nrPoints;
                xy &vert      = sect->vertex[     s];
                xy &next_vert = sect->vertex[next_s];
                if (
                    IntersectBox( px, py, px + dx, py + dy, vert.x, vert.y, next_vert.x, next_vert.y ) &&
                    PointSide( px + dx, py + dy, vert.x, vert.y, next_vert.x, next_vert.y ) < 0
                ) {
                    // Check where the hole is
                    float hole_low  = sect->neighbors[next_s] < 0 ? 9e9 : std::max( sect->floor, sectors[sect->neighbors[next_s]].floor );
                    float hole_high = sect->neighbors[next_s] < 0 ? 9e9 : std::min( sect->ceil,  sectors[sect->neighbors[next_s]].ceil  );

                    // Check whether we're bumping into a wall
                    if (
                        hole_high < player.where.z + HeadMargin ||
                        hole_low  > player.where.z - EyeHeight + KneeHeight
                    ) {
                        // Bumps into a wall! Slide along the wall
                        // This formula is from Wikipedia article "vector projection"
                        float xd = next_vert.x - vert.x, yd = next_vert.y - vert.y;
                        dx = xd * (dx * xd + yd * dy) / (xd * xd + yd * yd);
                        dy = yd * (dx * xd + yd * dy) / (xd * xd + yd * yd);
                        moving = 0;
                    }
                }
            }
            MovePlayer( dx, dy );
        }

        // user interaction

        // keep shift pressed for speed up or Insert for speed down
        float fSpeedup = GetKey( olc::Key::SHIFT ).bHeld ? 5.0f : (GetKey( olc::Key::INS ).bHeld ? 0.2f : 2.0f);
        // rotate
        if (GetKey( olc::Key::A ).bHeld) { player.angle -= fSpeedup * fElapsedTime; }
        if (GetKey( olc::Key::D ).bHeld) { player.angle += fSpeedup * fElapsedTime; }
        MovePlayer( 0, 0 );

        // move and strafe
        bool pushing = false;
        float move_vec[2] = { 0.0f, 0.0f };
        if (GetKey( olc::Key::W ).bHeld) { pushing = true; move_vec[0] += player.anglecos * 2.0f * fSpeedup * fElapsedTime; move_vec[1] += player.anglesin * 2.0f * fSpeedup * fElapsedTime; }
        if (GetKey( olc::Key::S ).bHeld) { pushing = true; move_vec[0] -= player.anglecos * 2.0f * fSpeedup * fElapsedTime; move_vec[1] -= player.anglesin * 2.0f * fSpeedup * fElapsedTime; }
        if (GetKey( olc::Key::Q ).bHeld) { pushing = true; move_vec[0] += player.anglesin * 2.0f * fSpeedup * fElapsedTime; move_vec[1] -= player.anglecos * 2.0f * fSpeedup * fElapsedTime; }
        if (GetKey( olc::Key::E ).bHeld) { pushing = true; move_vec[0] -= player.anglesin * 2.0f * fSpeedup * fElapsedTime; move_vec[1] += player.anglecos * 2.0f * fSpeedup * fElapsedTime; }

        if (GetKey( olc::Key::P      ).bPressed) glb_paused  = !glb_paused;
        if (GetKey( olc::Key::M      ).bPressed) glb_showmap = !glb_showmap;
        if (GetKey( olc::Key::NP_SUB ).bPressed) { glb_render_speed = std::max(  1, glb_render_speed - 1 ); }
        if (GetKey( olc::Key::NP_ADD ).bPressed) { glb_render_speed = std::min( 20, glb_render_speed + 1 ); }
        if (GetKey( olc::Key::ESCAPE ).bPressed) done = true;

        float acceleration = pushing ? 0.4f : 0.2f;
        player.velocity.x = player.velocity.x * (1.0f - acceleration) + move_vec[0] * acceleration;
        player.velocity.y = player.velocity.y * (1.0f - acceleration) + move_vec[1] * acceleration;

        if (pushing) moving = 1;

        return !done;
	}

	bool OnUserDestroy() override {
        // your cleanup code here
        UnloadData();

        return true;
	}

private:
    // parameters:
    //  * s - the sector to Draw
    //  * fScale - the scale to draw it in
    //  * col - the color for drawing
    //  * (orgX, orgY) - the origin to start drawing in screen coordinates
    //  * bNoRed - if true then no red edges will be drawn, otherwise portals will be drawn red
    //  * bFlipped - for drawing the sector in a vertical (portrait) orientation
    void DrawSector2D( Sector &s, float fScale = 1.0f, olc::Pixel col = olc::WHITE, int orgX = 0, int orgY = 0, bool bNoRed = false, bool bFlipped = false ) {

        int nLastElt = (int)s.vertex.size() - 1;
        olc::Pixel useCol;
        for (int i = 1; i <= nLastElt; i++) {
            useCol = (s.neighbors[i] == -1 || bNoRed) ? col : olc::RED;
            if (bFlipped) {
                DrawLine( orgX + fScale * s.vertex[i - 1].y, orgY + fScale * s.vertex[i - 1].x,
                          orgX + fScale * s.vertex[i    ].y, orgY + fScale * s.vertex[i    ].x, useCol );
            } else {
                DrawLine( orgX + fScale * s.vertex[i - 1].x, orgY + fScale * s.vertex[i - 1].y,
                          orgX + fScale * s.vertex[i    ].x, orgY + fScale * s.vertex[i    ].y, useCol );
            }
        }
        useCol = (s.neighbors[0] == -1 || bNoRed) ? col : olc::RED;
        if (bFlipped) {
            DrawLine ( orgX + fScale * s.vertex[nLastElt].y, orgY + fScale * s.vertex[nLastElt].x,
                       orgX + fScale * s.vertex[0       ].y, orgY + fScale * s.vertex[0       ].x, useCol );
        } else {
            DrawLine ( orgX + fScale * s.vertex[nLastElt].x, orgY + fScale * s.vertex[nLastElt].y,
                       orgX + fScale * s.vertex[0       ].x, orgY + fScale * s.vertex[0       ].y, useCol );
        }
    }

    // parameters:
    //  * fScale - the scale to draw it in
    //  * col - the color for drawing
    //  * (orgX, orgY) - the origin to start drawing in screen coordinates
    //  * bFlipped - for drawing the player in a vertical orientation (portrait)
    void DrawPlayer2D( float fScale = 1.0f, olc::Pixel col = olc::WHITE, int orgX = 0, int orgY = 0, bool bFlipped = false ) {
        // draw player as a little circle + a direction line where he's looking at
        if (bFlipped) {
            FillCircle( orgX + fScale * player.where.y, orgY + fScale * player.where.x, 4, col );
            DrawLine(   orgX + fScale * player.where.y, orgY + fScale * player.where.x, orgX + fScale * (player.where.y + sin( player.angle ) * 1.0f), orgY + fScale * (player.where.x + cos( player.angle ) * 1.0f), col );
        } else {
            FillCircle( orgX + fScale * player.where.x, orgY + fScale * player.where.y, 4, col );
            DrawLine(   orgX + fScale * player.where.x, orgY + fScale * player.where.y, orgX + fScale * (player.where.x + cos( player.angle ) * 1.0f), orgY + fScale * (player.where.y + sin( player.angle ) * 1.0f), col );
        }
        // highlight the sector where the player currently is by highlighting the sector's corner points
        for (auto p : sectors[ player.sector ].vertex ) {
            DrawCircle( orgX + fScale * p.x, orgY + fScale * p.y, 2, olc::YELLOW );
        }
    }

    // parameters:
    //  * fScale - the scale to draw the 2D map in
    //  * col - the base color for drawing the edges
    //  * (orgX, orgY) - the origin to start drawing in screen coordinates
    //  * bNoRed - if true then no red edges will be drawn, otherwise portals will be drawn red
    //  * bFlipped - for drawing the map in a vertical orientation (portrait)
    void DrawMap2D( float fScale, int orgX, int orgY, olc::Pixel col, bool bNoRed, bool bFlipped ) {

        // Draw map background
        FillRect( orgX - 5, orgY - 5, orgX + 28 * fScale, orgY + 18 * fScale, olc::VERY_DARK_GREY );
        // Draw all the sectors
        for (auto s : sectors)
            DrawSector2D( s, fScale, col, orgX, orgY, bNoRed, bFlipped );
        // Draw the player (and the active sector) on top
        DrawPlayer2D( fScale, olc::MAGENTA, orgX, orgY, bFlipped );
    }

    // parameters:
    //  * fScale - the scale to draw the 2D map in
    //  * col - the base color for displaying the text
    //  * (orgX, orgY) - the origin to start drawing in screen coordinates
    void DrawPlayerStats( float fScale, int orgX, int orgY, olc::Pixel col = olc::MAGENTA ) {

        DrawString( orgX, orgY + 20 * fScale +  0, "Position: " + std::to_string( player.where.x ) +
                                                   ", "         + std::to_string( player.where.y ) +
                                                   ", "         + std::to_string( player.where.z ), col );
        DrawString( orgX, orgY + 20 * fScale + 10, "Angle   : " + std::to_string( player.angle   ), col );
        DrawString( orgX, orgY + 20 * fScale + 20, "Sector  : " + std::to_string( player.sector  ), col );
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

