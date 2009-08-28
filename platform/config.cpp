#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <popt.h>

#include "platform.h"
#include "port.h"
#include "snes9x.h"
#include "display.h"
#include "hgw.h"

#define DIE(format, ...) do { \
		fprintf(stderr, "Died at %s:%d: ", __FILE__, __LINE__ ); \
		fprintf(stderr, format "\n", ## __VA_ARGS__); \
		abort(); \
	} while (0);

struct config Config;

/** Path to current rom file, with extension. */
static char * romFile;
/** Path to rom file, without extension.
 *  Used as a simple optimization to S9xGetFilename
 */
static char * basePath;

static struct poptOption commonOptionsTable[] = {
	{ "disable-audio", 'a', POPT_ARG_NONE, 0, 1,
	"disable emulation and output of audio", 0 },
	{ "display-framerate", 'r', POPT_ARG_NONE, 0, 2,
	"show frames per second counter in lower left corner", 0 },
	{ "skip-frames", 's', POPT_ARG_INT, 0, 3,
	"render only 1 in every N frames", "NUM" },
	{ "fullscreen", 'f', POPT_ARG_NONE, 0, 4,
	"start in fullscreen mode", 0 },
	{ "transparency", 'y', POPT_ARG_NONE, 0, 5,
	"enable transparency effects (slower)", 0 },
	{ "hacks", 'h', POPT_ARG_STRING | POPT_ARGFLAG_OPTIONAL, 0, 6,
	"enable hacks (yes, speed-only, no)", "option" },
	{ "pal", 'p', POPT_ARG_NONE, 0, 7,
	"run in PAL mode", 0 },
	{ "ntsc", 'n', POPT_ARG_NONE, 0, 8,
	"run in NTSC mode", 0 },
	{ "turbo", 't', POPT_ARG_NONE, 0, 9,
	"turbo mode (do not try to sleep between frames)", 0 },
	{ "conf", 'c', POPT_ARG_STRING, 0, 10,
	"extra configuration file to load", "FILE" },
	{ "mouse", 'm', POPT_ARG_INT | POPT_ARGFLAG_OPTIONAL, 0, 11,
	"enable mouse on controller NUM", "NUM"},
	{ "superscope", 'e', POPT_ARG_NONE, 0, 12,
	"enable SuperScope", 0},
	{ "snapshot", 'o', POPT_ARG_NONE, 0, 13,
	"unfreeze previous game on start and freeze game on exit", 0 },
	{ "audio-rate", 'u', POPT_ARG_INT, 0, 14,
	"audio output rate", "HZ" },
	{ "audio-buffer-size", 'b', POPT_ARG_INT, 0, 15,
	"audio output buffer size", "SAMPLES" },
	{ "touchscreen", 'd', POPT_ARG_NONE, 0, 16,
	"enable touchscreen controls", 0 },
	POPT_TABLEEND
};

static struct poptOption configOptionsTable[] = {
	{ "scancode", '\0', POPT_ARG_INT, 0, 100,
	"scancode to map", "CODE" },
	{ "button", '\0', POPT_ARG_STRING, 0, 101,
	"SNES Button to press (A, B, X, Y, L, R, Up, Down, Left, Right)", "name" },
	{ "action", '\0', POPT_ARG_STRING, 0, 102,
	"emulator action to do (fullscreen, quit, ...)", "action" },
	{ "hacks-file", '\0', POPT_ARG_STRING, 0, 200,
	"path to snesadvance.dat file", "FILE" },
	POPT_TABLEEND
};

static struct poptOption optionsTable[] = {
	{ 0, '\0', POPT_ARG_INCLUDE_TABLE, commonOptionsTable, 0,
	"Common options", 0 },
	{ 0, '\0', POPT_ARG_INCLUDE_TABLE, configOptionsTable, 0,
	"Configuration file options", 0 },
	POPT_AUTOHELP
	POPT_TABLEEND
};

static unsigned short buttonNameToBit(const char *s) {
	if (strcasecmp(s, "A") == 0) {
		return SNES_A_MASK;
	} else if (strcasecmp(s, "B") == 0) {
		return SNES_B_MASK;
	} else if (strcasecmp(s, "X") == 0) {
		return SNES_X_MASK;
	} else if (strcasecmp(s, "Y") == 0) {
		return SNES_Y_MASK;
	} else if (strcasecmp(s, "L") == 0) {
		return SNES_TL_MASK;
	} else if (strcasecmp(s, "R") == 0) {
		return SNES_TR_MASK;
	} else if (strcasecmp(s, "UP") == 0) {
		return SNES_UP_MASK;
	} else if (strcasecmp(s, "DOWN") == 0) {
		return SNES_DOWN_MASK;
	} else if (strcasecmp(s, "LEFT") == 0) {
		return SNES_LEFT_MASK;
	} else if (strcasecmp(s, "RIGHT") == 0) {
		return SNES_RIGHT_MASK;
	} else if (strcasecmp(s, "START") == 0) {
		return SNES_START_MASK;
	} else if (strcasecmp(s, "SELECT") == 0) {
		return SNES_SELECT_MASK;
	} else {
		DIE("Bad button name: %s\n", s);
	}
}

static unsigned char actionNameToBit(const char *s) {
	if (strcasecmp(s, "quit") == 0) {
		return kActionQuit;
	} else if (strcasecmp(s, "fullscreen") == 0) {
		return kActionToggleFullscreen;
	} else {
		DIE("Bad action name: %s\n", s);
	}
}

const char * S9xGetFilename(FileTypes file)
{
	static char filename [PATH_MAX + 1];
	const char * ext;
	switch (file) {
		case FILE_ROM:
			return romFile;
		case FILE_SRAM:
			ext = "srm";
			break;
		case FILE_FREEZE:
			ext = "frz.gz";
			break;
		case FILE_CHT:
			ext = "cht";
			break;
		case FILE_IPS:
			ext = "ips";
			break;
		case FILE_SCREENSHOT:
			ext = "png";
			break;
		case FILE_SDD1_DAT:
			ext = "dat";
			break;
		default:
			ext = "???";
			break;
	}

	snprintf(filename, PATH_MAX, "%s.%s", basePath, ext);
	return filename;
}

static void loadDefaults()
{
	ZeroMemory(&Settings, sizeof(Settings));
	ZeroMemory(&Config, sizeof(Config)); 
	
	romFile = 0;
	basePath = 0;

	Config.quitting = false;
	Config.enableAudio = true;
	Config.fullscreen = false;
	Config.xsp = false;
	Config.hacksFile = 0;
	Config.touchscreenInput = false;

	Settings.JoystickEnabled = FALSE;
	Settings.SoundPlaybackRate = 22050;
	Settings.Stereo = TRUE;
	Settings.SoundBufferSize = 512; // in samples
	Settings.CyclesPercentage = 100;
	Settings.DisableSoundEcho = FALSE;
	Settings.APUEnabled = FALSE;
	Settings.H_Max = SNES_CYCLES_PER_SCANLINE;
	Settings.SkipFrames = AUTO_FRAMERATE;
	Settings.Shutdown = Settings.ShutdownMaster = TRUE;
	Settings.FrameTimePAL = 20;	// in msecs
	Settings.FrameTimeNTSC = 16;
	Settings.FrameTime = Settings.FrameTimeNTSC;
	Settings.DisableSampleCaching = FALSE;
	Settings.DisableMasterVolume = FALSE;
	Settings.Mouse = FALSE;
	Settings.SuperScope = FALSE;
	Settings.MultiPlayer5 = FALSE;
	Settings.ControllerOption = SNES_JOYPAD;
	
	Settings.ForceTransparency = FALSE;
	Settings.Transparency = FALSE;
	Settings.SixteenBit = TRUE;
	
	Settings.SupportHiRes = FALSE;
	Settings.NetPlay = FALSE;
	Settings.ServerName [0] = 0;
	Settings.AutoSaveDelay = 30;
	Settings.ApplyCheats = FALSE;
	Settings.TurboMode = FALSE;
	Settings.TurboSkipFrames = 15;
    
    Settings.ForcePAL = FALSE;
    Settings.ForceNTSC = FALSE;

    Settings.HacksEnabled = FALSE;
    Settings.HacksFilter = FALSE;

	Settings.HBlankStart = (256 * Settings.H_Max) / SNES_HCOUNTER_MAX;

	Settings.AutoSaveDelay = 15*60; // Autosave each 15 minutes.
}

void S9xSetRomFile(const char * path)
{
	if (romFile) {
		free(romFile);
		free(basePath);
	}

	romFile = strndup(path, PATH_MAX);
	basePath = strdup(romFile);

	// Truncate base path at the last '.' char
	char * c = strrchr(basePath, '.');
	if (c) {
		*c = '\0';
	}
}

static bool gotRomFile() 
{
	return romFile ? true : false;
}

static void setHacks(const char * value)
{
	// Unconditionally enable hacks even if no argument passed
	Settings.HacksEnabled = TRUE;

	if (!value) return;

	if (strcasecmp(value, "speed-only") == 0 ||
		strcasecmp(value, "speed") == 0 ||
		strcasecmp(value, "s") == 0) {
			Settings.HacksFilter = TRUE;
	} else if (strcasecmp(value, "yes") == 0 ||
		strcasecmp(value, "y") == 0) {
			// Do nothing
	} else if (strcasecmp(value, "no") == 0 ||
		strcasecmp(value, "n") == 0) {
			Settings.HacksEnabled = FALSE;
	} else {
		// Hack: the user probably wants to enable hacks
		// and use this argument as the ROM file.
		// Wonder why popt does not support this or if there's a better way.
		S9xSetRomFile(value);
	}
}

static void loadConfig(poptContext optCon, const char * file)
{
	char * out;
	int newargc, ret;
	const char ** newargv;
	FILE * fp;

	fp = fopen (file, "r");
	if (!fp) {
		fprintf(stderr, "Cannot open config file %s\n", file);
		return;
	}

	ret = poptConfigFileToString (fp, &out, 0);
	if (ret)
	    DIE("Cannot parse config file %s. ret=%d\n", file, ret);

	poptParseArgvString(out, &newargc, &newargv);

	poptStuffArgs(optCon, newargv);

	free(out);
	fclose(fp);
	/* XXX: currently leaking newargv */
}

static void parseArgs(poptContext optCon)
{
	int rc;
	unsigned char scancode = 0;
	
	while ((rc = poptGetNextOpt(optCon)) > 0) {
		const char * val;
		switch (rc) {
			case 1:
				Config.enableAudio = false;
				break;
			case 2:
				Settings.DisplayFrameRate = TRUE;
				break;
			case 3:
				Settings.SkipFrames = atoi(poptGetOptArg(optCon));
				break;
			case 4:
				Config.fullscreen = true;
				break;
			case 5:
				Settings.SixteenBit = TRUE;
				Settings.Transparency = TRUE;
				break;
			case 6:
				Settings.HacksEnabled = TRUE;
				setHacks(poptGetOptArg(optCon));
				break;
			case 7:
				Settings.ForcePAL = TRUE;
				break;
			case 8:
				Settings.ForceNTSC = TRUE;
				break;
			case 9:
				Settings.TurboMode = TRUE;
				break;
			case 10:
				loadConfig(optCon, poptGetOptArg(optCon));
				break;
			case 11:
				val = poptGetOptArg(optCon);
				Settings.Mouse = TRUE;
				if (!val || atoi(val) <= 1) {
					// Enable mouse on first controller
					Settings.ControllerOption = SNES_MOUSE_SWAPPED;
				} else {
					// Enable mouse on second controller
					Settings.ControllerOption = SNES_MOUSE;
				}
				break;
			case 12:
				Settings.SuperScope = TRUE;
				Settings.ControllerOption = SNES_SUPERSCOPE;
				break;
			case 13:
				Config.snapshotLoad = true;
				Config.snapshotSave = true;
				break;
			case 14:
				Settings.SoundPlaybackRate = atoi(poptGetOptArg(optCon));
				break;
			case 15:
				Settings.SoundBufferSize = atoi(poptGetOptArg(optCon));
				break;
			case 16:
				Config.touchscreenInput = true;
				break;
			case 100:
				scancode = atoi(poptGetOptArg(optCon));
				break;
			case 101:
				Config.joypad1Mapping[scancode] |= 
					buttonNameToBit(poptGetOptArg(optCon));
				break;
			case 102:
				Config.action[scancode] |= 
					actionNameToBit(poptGetOptArg(optCon));
				break;
			case 200:
				free(Config.hacksFile);
				Config.hacksFile = strdup(poptGetOptArg(optCon));
				break;
			default:
				DIE("Invalid popt argument (this is a bug): %d", rc);
				break;
		}
	}
	
	if (rc < -1) {
		/* an error occurred during option processing */
		fprintf(stderr, "%s: %s\n",
			poptBadOption(optCon, 0),
			poptStrerror(rc));
		exit(2);
	}

	/* if there's an extra unparsed arg it's our rom file */
	const char * extra_arg = poptGetArg(optCon);
	if (extra_arg) 
		S9xSetRomFile(extra_arg);
}

void S9xLoadConfig(int argc, const char ** argv)
{
	poptContext optCon =
		poptGetContext("drnoksnes", argc, argv, optionsTable, 0);
	poptSetOtherOptionHelp(optCon, "<rom>");

	// Builtin defaults
	loadDefaults();

	// Read config file ~/apps/DrNokSnes.txt
	char defConfFile[PATH_MAX];
	sprintf(defConfFile, "%s/%s", getenv("HOME"), "apps/DrNokSnes.txt");
	loadConfig(optCon, defConfFile);

	// Command line parameters (including --conf args)
	parseArgs(optCon);

	if (!gotRomFile() && !hgwLaunched) {
		// User did not specify a ROM file, 
		// and we're not being launched from D-Bus.
		fprintf(stderr, "You need to specify a ROM, like this:\n");
		poptPrintUsage(optCon, stdout, 0);
		poptFreeContext(optCon); 
		exit(2);
	}

	poptFreeContext(optCon);
}

void S9xUnloadConfig()
{
	if (romFile) {
		free(romFile);
		romFile = 0;
	}
	if (basePath) {
		free(basePath);
		basePath = 0;
	}
	if (Config.hacksFile) {
		free(Config.hacksFile);
		Config.hacksFile = 0;
	}
}

