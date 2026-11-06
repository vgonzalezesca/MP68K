/*
 * MP368K resources: menus, alerts and memory size. All user-visible text is US English
 * (project rule: see CLAUDE.md). \0xC9 is the MacRoman ellipsis character.
 */
#include "Dialogs.r"
#include "Menus.r"
#include "Processes.r"

resource 'MENU' (128) {
	128, textMenuProc, allEnabled, enabled, apple,
	{
		"About MP368K\0xC9", noIcon, noKey, noMark, plain;
	}
};

resource 'MENU' (129) {
	129, textMenuProc, allEnabled, enabled, "File",
	{
		"Open\0xC9", noIcon, "O", noMark, plain;
		"Add Files\0xC9", noIcon, "D", noMark, plain;
		"Add Folder\0xC9", noIcon, "F", noMark, plain;
		"Open Playlist\0xC9", noIcon, noKey, noMark, plain;
		"-", noIcon, noKey, noMark, plain;
		"Close Window", noIcon, "W", noMark, plain;
		"-", noIcon, noKey, noMark, plain;
		"Quit", noIcon, "Q", noMark, plain;
	}
};

resource 'MENU' (130) {
	130, textMenuProc, allEnabled, enabled, "Controls",
	{
		"Play/Pause", noIcon, "P", noMark, plain;
		"Stop", noIcon, ".", noMark, plain;
		"-", noIcon, noKey, noMark, plain;
		"Previous Track", noIcon, "[", noMark, plain;
		"Next Track", noIcon, "]", noMark, plain;
		"-", noIcon, noKey, noMark, plain;
		"Shuffle", noIcon, noKey, noMark, plain;
		"-", noIcon, noKey, noMark, plain;
		"Repeat Off", noIcon, noKey, noMark, plain;
		"Repeat All", noIcon, noKey, noMark, plain;
		"Repeat One", noIcon, noKey, noMark, plain;
	}
};

resource 'MENU' (131) {
	131, textMenuProc, allEnabled, enabled, "Quality",
	{
		"Automatic", noIcon, noKey, noMark, plain;
		"Full", noIcon, noKey, noMark, plain;
		"Half Rate", noIcon, noKey, noMark, plain;
		"Mono", noIcon, noKey, noMark, plain;
		"Half Rate Mono", noIcon, noKey, noMark, plain;
	}
};

resource 'MENU' (132) {
	132, textMenuProc, allEnabled, enabled, "Options",
	{
		"Audio Output: Automatic", noIcon, noKey, noMark, plain;
		"Audio Output: 8-bit", noIcon, noKey, noMark, plain;
		"Audio Output: 16-bit", noIcon, noKey, noMark, plain;
		"-", noIcon, noKey, noMark, plain;
		"Show Technical Info", noIcon, noKey, noMark, plain;
	}
};

resource 'MENU' (133) {
	133, textMenuProc, allEnabled, enabled, "Window",
	{
		"Playlist", noIcon, "L", noMark, plain;
	}
};

resource 'MBAR' (128) {
	{ 128, 129, 130, 131, 132, 133 }
};

/* About box */
resource 'ALRT' (128) {
	{ 60, 60, 190, 400 },
	128,
	{
		OK, visible, silent,
		OK, visible, silent,
		OK, visible, silent,
		OK, visible, silent
	},
	alertPositionMainScreen
};

resource 'DITL' (128) {
	{
		{ 98, 262, 118, 330 },
		Button { enabled, "OK" };

		{ 10, 10, 90, 330 },
		StaticText { disabled, "MP368K 0.2\0x0DAn MP3 player for 68k Macintosh computers.\0x0DDecoder: libmad (GPL). Personal use." };
	}
};

/* Error alert: the text arrives through ParamText (^0) */
resource 'ALRT' (129) {
	{ 70, 60, 180, 400 },
	129,
	{
		OK, visible, silent,
		OK, visible, silent,
		OK, visible, silent,
		OK, visible, silent
	},
	alertPositionMainScreen
};

resource 'DITL' (129) {
	{
		{ 78, 262, 98, 330 },
		Button { enabled, "OK" };

		{ 10, 10, 70, 330 },
		StaticText { disabled, "^0" };
	}
};

/*
 * Partition: 1.5 MB preferred, 1 MB minimum. canBackground keeps decoding (and playing)
 * while another application is in front.
 */
resource 'SIZE' (-1) {
	reserved,
	acceptSuspendResumeEvents,
	reserved,
	canBackground,
	doesActivateOnFGSwitch,
	backgroundAndForeground,
	dontGetFrontClicks,
	ignoreChildDiedEvents,
	is32BitCompatible,
	notHighLevelEventAware,
	onlyLocalHLEvents,
	notStationeryAware,
	dontUseTextEditServices,
	reserved,
	reserved,
	reserved,
	1536 * 1024,
	1024 * 1024
};
