#include <stdio.h>

#include <SDL.h>
#include <SDL_syswm.h>

#include "snes9x.h"
#include "platform.h"
#include "display.h"
#include "gfx.h"
#include "ppu.h"
#include "sdlv.h"
#include "GLUtil.h"

#define DIE(format, ...) do { \
		fprintf(stderr, "Died at %s:%d: ", __FILE__, __LINE__ ); \
		fprintf(stderr, format "\n", ## __VA_ARGS__); \
		abort(); \
	} while (0);

struct gui GUI;

/** Sets the main window title */
void S9xSetTitle(const char *title)
{
  //Nothing
}

static void freeVideoSurface()
{
	free(GFX.Screen); GFX.Screen = 0;

	free(GFX.SubScreen); GFX.SubScreen = 0;
	free(GFX.ZBuffer); GFX.ZBuffer = 0;
	free(GFX.SubZBuffer); GFX.SubZBuffer = 0;
}

static void setupVideoSurface()
{
	// Real surface area.
	const unsigned gameWidth = IMAGE_WIDTH;
	const unsigned gameHeight = IMAGE_HEIGHT;

	GUI.Width = gameWidth;
	GUI.Height = gameHeight;

#if CONF_EXIT_BUTTON
	ExitBtnReset();
#endif

	// Safeguard
	if (gameHeight > GUI.Height || gameWidth > GUI.Width)
		DIE("Video is larger than window size!");

  GL_InitTexture(gameWidth, gameHeight);
  updateOrientation();
	
	SDL_ShowCursor(SDL_DISABLE);
 
	// Each scaler may have its own pitch
	GFX.Pitch = gameWidth * 2;
	GFX.ZPitch = GFX.Pitch / 2;
	// gfx & tile.cpp depend on the zbuffer pitch being always half of the color buffer pitch.
	// Which is a pity, since the color buffer might be much larger.

	GFX.Screen = (uint8 *) calloc(1,GFX.Pitch * IMAGE_HEIGHT);
	GFX.SubScreen = (uint8 *) calloc(1,GFX.Pitch * IMAGE_HEIGHT);
	GFX.ZBuffer =  (uint8 *) calloc(1,GFX.ZPitch * IMAGE_HEIGHT);
	GFX.SubZBuffer = (uint8 *) calloc(1,GFX.ZPitch * IMAGE_HEIGHT);

	GFX.Delta = (GFX.SubScreen - GFX.Screen) >> 1;
	GFX.DepthDelta = GFX.SubZBuffer - GFX.ZBuffer;
	GFX.PPL = GFX.Pitch / 2;

  GUI.ScaleX = GUI.ScaleY = 1.0;
  GUI.RenderX = GUI.RenderY = 0;
  GUI.RenderW = gameWidth;
  GUI.RenderH = gameHeight;
}

void S9xVideoReset()
{
  //Clear all graphics buffers...
  memset(GFX.Screen,0,GFX.Pitch * IMAGE_HEIGHT);
  memset(GFX.SubScreen,0,GFX.Pitch * IMAGE_HEIGHT);
  memset(GFX.ZBuffer,0,GFX.ZPitch * IMAGE_HEIGHT);
  memset(GFX.SubZBuffer,0,GFX.ZPitch * IMAGE_HEIGHT);
}

static void drawOnscreenControls()
{
#if 0
	if (Config.touchscreenInput) {
		S9xInputScreenChanged();
	}

	if (Config.touchscreenShow) {
		SDL_FillRect(screen, NULL, 0);
		S9xInputScreenDraw(Settings.SixteenBit ? 2 : 1,
							screen->pixels, screen->pitch);
		SDL_Flip(screen);
	}
#endif
}

void S9xInitDisplay(int argc, char ** argv)
{	
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) 
		DIE("SDL_InitSubSystem(VIDEO): %s", SDL_GetError());

	setupVideoSurface();
	drawOnscreenControls();
}

void S9xDeinitDisplay()
{
	freeVideoSurface();	
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void S9xVideoToggleFullscreen()
{
	freeVideoSurface();
	Config.fullscreen = !Config.fullscreen;
	setupVideoSurface();
	drawOnscreenControls();
}

extern void pauseGame(void);

bool videoEventFilter(const SDL_Event& event)
{
		if (event.type == SDL_ACTIVEEVENT &&
		   (event.active.state & SDL_APPINPUTFOCUS)) {
      bool active = event.active.gain;
      if (!active) {
        // We lost focus!
        // Show the menu (which also saves)
        S9xDoAction(kActionMenu);

      } else {
        //XXX: Resume!
      }

      //We handled this event
      return true;
		}

  //We didn't handle this, carry on
  return false;
}

void S9xSetPalette ()
{
  //Nothing
}

/** Called before rendering a frame.
	This function must ensure GFX.Screen points to something, but we did that
	while initializing video output.
	@return TRUE if we should render the frame.
 */
bool8_32 S9xInitUpdate ()
{
	return TRUE;
}

/** Called once a complete SNES screen has been rendered into the GFX.Screen
	memory buffer.

	Now is your chance to copy the SNES rendered screen to the
	host computer's screen memory. The problem is that you have to cope with
	different sized SNES rendered screens. Width is always 256, unless you're
	supporting SNES hi-res. screen modes (Settings.SupportHiRes is TRUE), in
	which case it can be 256 or 512. The height parameter can be either 224 or
	239 if you're only supporting SNES lo-res. screen modes, or 224, 239, 448 or
	478 if hi-res. SNES screen modes are being supported.
 */
// TODO Above.
bool8_32 S9xDeinitUpdate (int width, int height)
{
  GL_RenderPix(GFX.Screen,width,height);
#if CONF_EXIT_BUTTON
	if (ExitBtnRequiresDraw()) {
		ExitBtnDraw(screen);
	}
#endif

	return TRUE;
}

void S9xVideoTakeScreenshot(void)
{
  //Get the GL-rendered screen..
  int w = NATIVE_RES_WIDTH;
  int h = NATIVE_RES_HEIGHT;
  int bpp = 4;

  char * buffer = (char *)malloc(h*w*bpp);
  glReadPixels( 0, 0,
                NATIVE_RES_WIDTH, NATIVE_RES_HEIGHT,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                buffer );
  SDL_Surface * s = SDL_CreateRGBSurface( SDL_SWSURFACE, NATIVE_RES_WIDTH, NATIVE_RES_HEIGHT, 32,
            0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);

  int line_size = w*bpp;
  for ( int i = 0; i < s->h; ++i )
  {
    memcpy( ((char *)s->pixels) + s->pitch*i, buffer + line_size*i, line_size );
  }
  //while(1)
  //{
  //  SDL_DrawSurfaceAsGLTexture( s, portrait_vertexCoords );
  //}

  //Create the filename...
  char filename[100];
  static int counter = 0;
  sprintf( filename, "screenshot-%d.bmp", counter++ );
  int res = SDL_SaveBMP( s, filename );
  SDL_FreeSurface( s );
  free(buffer);
  printf( "Wrote \"%s\"? %d\n", filename, res == 0 );
}
