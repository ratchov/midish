/*
 * Copyright (c) 2018 Alexandre Ratchov <alex@caoua.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <stddef.h>
#include "textio.h"
#include "help.h"

struct help help_list[] = {
	{"tlist",
	"tlist\n"
	"\n"
	"Return the list of tracks."},

	{"tnew",
	"tnew name\n"
	"\n"
	"Create a track with the given name."},

	{"tdel",
	"tdel\n"
	"\n"
	"Delete the current track."},

	{"tren",
	"tren name\n"
	"\n"
	"Rename the current track to the given name."},

	{"texists",
	"texists name\n"
	"\n"
	"Return 1 if a track with the give name exists, 0 otherwise."},

	{"taddev",
	"taddev measure beat tic event\n"
	"\n"
	"Put the given event at the give position of the current track."},
  
	{"tsetf",
	"tsetf filtname\n"
	"\n"
	"Set the default filter of the current track to the given filter."},

	{"tgetf",
	"tgetf\n"
	"\n"
	"Return the default filter of the current track, returns nil\n"
	"if none."},

	{"tcheck",
	"tcheck\n"
	"\n"
	"Check the current track for orphaned notes, nested notes, and "
	"other anomalies. Remove multiple controllers in the same clock "
	"tick."},

	{"trewrite",
	"trewrite\n"
	"\n"
	"Rewrite the current track note-by-note."},

	{"tcut",
	"tcut\n"
	"\n"
	"Cut the current selection of the current track."},

	{"tclr",
	"tclr\n"
	"\n"
	"Clear the current selection of the current track. Only events "
	"matching the current event selection (see the ev function) are "
	"removed."},

	{"tins",
	"tins amount\n"
	"\n"
	"Insert the given amount of empty measures at the current position "
	"in the current track."},

	{"tpaste",
	"tpaste\n"
	"\n"
	"Paste track portion (copied with tcopy) at the current position "
	"of the current track."},

	{"tcopy",
	"tcopy\n"
	"\n"
	"Copy the current selection of the current track so that it could "
	"be pasted with the tpaste function. "
	"Only events matching the current event selection "
	"are copied (see ev function)."},

	{"tmerge",
	"tmerge source\n"
	"\n"
	"Merge the given track into the current track."},

	{"tquanta",
	"tquanta rate\n"
	"\n"
	"Round event positions to the nearest exact position, as defined "
	"by the setq function. This affects events of the current "
	"selection of the current track. Event positions are rounded to "
	"the nearest tick multiple of the quantization step}, Rate must be "
	"between 0 and 100: 0 means no quantization and 100 means full "
	"quantization."},

	{"tquantf",
	"tquantf rate\n"
	"\n"
	"Round frame positions (eg. note positions) to the nearest exact "
	"position, as defined by the setq function. Frame length (eg. "
	"note lengths) is preserved. Rate must be between 0 and 100: "
	"0 means no quantization and 100 means full quantization."},

	{"ttransp",
	"ttransp halftones\n"
	"\n"
	"Transpose note events of current selection of the current track, "
	"by the given number of half-tones. Only events matching the current "
	"event selection are transposed (see ev function)."},

	{"tvcurve",
	"tvcurve weight\n"
	"\n"
	"Adjust velocity of note events, using the given weight in "
	"the -63..63 range. Applies only to note events of current "
	"selection of the current track (see ev function)."},

	{"tevmap",
	"tevmap source dest\n"
	"\n"
	"Convert events matching the given source event set to the given destination "
	"event set. Changes apply to in the current selection of the current track. "
	"Both event sets must have the same number of devices, "
	"channels, notes, controllers etc.."},

	{"mute",
	"mute trackname\n"
	"\n"
	"Mute the given track."},

	{"unmute",
	"unmute trackname\n"
	"\n"
	"Unmute the given track."},

	{"getmute",
	"getmute trackname\n"
	"\n"
	"Return 1 if the given track is muted and 0 otherwise."},

	{"tclist",
	"tclist\n"
	"\n"
	"Return the list of channels used by events stored in the "
	"current track."},

	{"tinfo",
	"tinfo\n"
	"\n"
	"Scan the current selection of the current track, an for each "
	"measure display the number of events that match the current "
	"event selection."},

	{"tdump",
	"tdump\n"
	"\n"
	"Display selected events of the current track."},

	{"inew",
	"inew name channel\n"
	"\n"
	"Create an new input with the given name associated "
	"to the given device and MIDI channel pair."},

	{"iset",
	"iset channel\n"
	"\n"
	"Set the device and channel number of the current input. All "
	"filters are updated to use the new channel setting as if the "
	"appropriate fchin function was called for each filter."},

	{"idel",
	"idel\n"
	"\n"
	"Delete the current input."},

	{"iren",
	"iren newname\n"
	"\n"
	"Rename the current input to the given name."},

	{"ilist",
	"ilist\n"
	"\n"
	"Return the list of inputs."},

	{"iexists",
	"iexists channame\n"
	"\n"
	"Return 1 if an input with the give name exists, 0 otherwise."},

	{"igetc",
	"igetc\n"
	"\n"
	"Return the MIDI channel number of the current input."},

	{"igetd",
	"igetd\n"
	"\n"
	"Return the device number of the current input."},

	{"iaddev",
	"iaddev event\n"
	"\n"
	"Add the given event to the configuration of the "
	"current channel, it's not used yet."},

	{"irmev",
	"irmev evspec\n"
	"\n"
	"Remove all events matching the given evspec from the configuration "
	"of the current channel."},

	{"iinfo",
	"iinfo\n"
	"\n"
	"Print all config events of the current input."},

	{"onew",
	"onew name channel\n"
	"\n"
	"Create an new output with the given name and the given device "
	"and MIDI channel. Outputs contain a built-in filter with the "
	"same name}, by defaut it maps all inputs to the newly created "
	"output."},

	{"odel",
	"odel\n"
	"\n"
	"Delete current output."},

	{"oset",
	"oset channel\n"
	"\n"
	"Set the device and MIDI channel numbers of the current output. "
	"All filters are updated to use the new setting as if the "
	"appropriate fchout function was called for each filter."},

	{"oren",
	"oren newname\n"
	"\n"
	"Rename the current output to the given name."},

	{"oexists",
	"oexists channame\n"
	"\n"
	"Return 1 if an output with the give name exists, 0 otherwise."},

	{"ogetc",
	"ogetc\n"
	"\n"
	"Return the MIDI channel number of the current output."},

	{"ogetd",
	"ogetd\n"
	"\n"
	"Return the device number of the current output."},

	{"oaddev",
	"oaddev event\n"
	"\n"
	"Add the given event to the configuration of the current output."},

	{"ormev",
	"ormev evspec\n"
	"\n"
	"Remove all events matching the given set from the configuration "
	"of the current output."},

	{"oinfo",
	"oinfo\n"
	"\n"
	"Print all config events of the current output."},

	{"olist",
	"olist\n"
	"\n"
	"Return list of outputs."},

	{"fnew",
	"fnew filtname\n"
	"\n"
	"Create an new filter with the given name."},

	{"fdel",
	"fdel\n"
	"\n"
	"Delete the current filter."},

	{"fren",
	"fren newname\n"
	"\n"
	"Rename the current filter to the given name."},

	{"fexists",
	"fexists filtname\n"
	"\n"
	"Return 1 if a filter with the given name exists, 0 otherwise."},

	{"freset",
	"freset\n"
	"\n"
	"Remove all rules from the current filter."},

	{"finfo",
	"finfo\n"
	"\n"
	"List all rules of the current filter."},

	{"flist",
	"flist\n"
	"\n"
	"Return the list of existing filters."},

	{"fchgin",
	"fchgin old_evspec new_evspec\n"
	"\n"
	"Rewrite current filter rules to consume  new event set in place "
	"of the old event set. This means that each rule previously "
	"matching old set on the input will start "
	"matching new set instead."},

	{"fswapin",
	"fswapin evspec1 evspec2\n"
	"\n"
	"Similar to the fchgin function but swap the two event sets in "
	"the source events set of each rule."},

	{"fchgout",
	"fchgout from to\n"
	"\n"
	"Rewrite all filtering rules of the current filter to produce "
	"the new event set instead of the old event set. This means "
	"that each rule previously producing the old set will "
	"start producing new set instead."},

	{"fswapout",
	"fswapout from to\n"
	"\n"
	"Similar to the fchgout function, but swap the two event sets in "
	"the destination events set of each rule."},

	{"fmap",
	"fmap source dest\n"
	"\n"
	"Add a new mapping rule to the current filter. The events matching "
	"the source event set will be converted to the destination event set. "
	"Source and destination sets must have the same number of devices, "
	"channels, notes, controllers etc.."},

	{"funmap",
	"funmap source dest\n"
	"\n"
	"Remove the given event mapping rules from the current filter. Any "
	"mapping rule with source included in the given source and destination "
	"inluded in the given destination is deleted."},

	{"ftransp",
	"ftransp evspec half_tones\n"
	"\n"
	"Transpose events matching the given event set by the given number "
	"of halftones."},

	{"fvcurve",
	"fvcurve evspec weight\n"
	"\n"
	"Adjust velocity of the given note events, using "
	"the given weight in the -63..63 range.  If the weight is "
	"negative then sensitivity is decreased. If it's positive then "
	"sensitivity is increased. If it's zero the velocity is unchanged."},

	{"xnew",
	"xnew sysexname\n"
	"\n"
	"Create a new bank of sysex messages with the given name."},

	{"xdel",
	"xdel\n"
	"\n"
	"Delete the current bank of sysex messages."},

	{"xren",
	"xren newname\n"
	"\n"
	"Rename the current sysex bank to the given name."},

	{"xexists",
	"xexists sysexname\n"
	"\n"
	"Return 1 if a sysex bank with the given name exists, 0 otherwise."},

	{"xrm",
	"xrm data\n"
	"\n"
	"Remove all sysex messages starting with the given pattern from the "
	"current sysex bank. The given pattern is a list of bytes}, an empty "
	"pattern matches any sysex message."},

	{"xsetd",
	"xsetd device pattern\n"
	"\n"
	"Set device number of all sysex messages starting "
	"with the given pattern in the current sysex bank. The given pattern "
	"is a list of bytes}, an empty pattern matches any sysex message."},

	{"xadd",
	"xadd device message\n"
	"\n"
	"Add the given sysex message to the current sysex bank. The message is a "
	"list containing the system exclusive message bytes. The given device number "
	"specifies the device to which the message will be sent when performance mode "
	"is entered."},

	{"xinfo",
	"xinfo\n"
	"\n"
	"Print all sysex messages of the current sysex bank. Messages that are "
	"too long to be desplayed on a single line are truncated and dots "
	"are displayed."},

	{"ximport",
	"ximport device path\n"
	"\n"
	"Replace contents of the current sysex bank by contents of the given "
	".syx file}, messages are assigned to the given device number."},

	{"xexport",
	"xexport path\n"
	"\n"
	"Store contents of the current sysex bank in the given .syx file."},

	{"xlist",
	"xlist\n"
	"\n"
	"Return the list of sysex banks."},

	{"i",
	"i\n"
	"\n"
	"Enter performance mode. Start processing MIDI input and "
	"generating MIDI output. Data passes through the current filter "
	"(if any) or through the current track's filter (if any)."},

	{"p",
	"p\n"
	"\n"
	"Play the song from the current position. Input passes through the "
	"current filter (if any) or through the current track's filter "
	"(if any)."},

	{"r",
	"r\n"
	"\n"
	"Play the song and record the input. Input passes through the current "
	"filter (if any) or through the current track's filter (if any). "
	"On startup, this function plays one measure of countdown before the data "
	"start being recorded."},

	{"s",
	"s\n"
	"\n"
	"Stop performance and release MIDI devices."},

	{"ev",
	"ev evspec\n"
	"\n"
	"Set the current event selection. Most track editing functions will "
	"act only on events matching evspec ignoring all other events."},

	{"setq",
	"setq step\n"
	"\n"
	"Set the current quantization step to the given note value, as follows:\n"
	"\t4 -> quarter note\n"
	"\t6 -> quarter note triplet\n"
	"\t8 -> eighth note\n"
	"\t12 -> eighth note triplet\n"
	"\tetc...\n"
	"The quantization step will be used by tquanta and tquantf funnctions "
	"to optimize event selection. If the special nil value is specified "
	"as quantization step, then quatization is disabled."},

	{"getq",
	"getq\n"
	"\n"
	"Return the current quatization step."},

	{"g",
	"g measure\n"
	"\n"
	"Set the current song position pointer to the given measure number. "
	"Record and playback will start a that position. This also defines "
	"the beginning of the current selection used by most track editing "
	"functions."},

	{"getpos",
	"getpos\n"
	"\n"
	"Return the current song position pointer which is also the start "
	"position of the current selection."},

	{"sel",
	"sel length\n"
	"\n"
	"Set the length of the current selection to "
	"the given number of  measures. "
	"The current selection start at the current position "
	"set with the g function."},

	{"getlen",
	"getlen\n"
	"\n"
	"Return the length (in measures) of the current selection."},

	{"loop",
	"loop\n"
	"\n"
	"Enable loop mode. When playback is started, the current "
	"selection is repeated forever."},

	{"noloop",
	"noloop\n"
	"\n"
	"Disable loop mode."},

	{"ct",
	"ct trackname\n"
	"\n"
	"Set the current track. The current track "
	"is the one that will be recorded. Most track editing "
	"functions act on it."},

	{"gett",
	"gett\n"
	"\n"
	"Return the current track (if any) or nil."},

	{"cf",
	"cf filtname\n"
	"\n"
	"Set the current filter to the given filter. The current filter "
	"is the one used to process input MIDI events in performance mode. "
	"It's also the one affected by all filter editing functions."},

	{"getf",
	"getf\n"
	"\n"
	"Return the current filter or nil if none."},

	{"cx",
	"cx sysexname\n"
	"\n"
	"Set the current sysex bank, i.e. the one that will be recorded. "
	"The current sysex back is the one affected by all sysex editing "
	"functions."},

	{"getx",
	"getx\n"
	"\n"
	"Return the current sysex bank or nil if none."},

	{"ci",
	"ci channame\n"
	"\n"
	"Set the current (named) input channel. All input channel "
	"editing functions will act on it."},

	{"geti",
	"geti\n"
	"\n"
	"Return the name of the current input channel or nil if none."},

	{"co",
	"co channame\n"
	"\n"
	"Set the current (named) output channel. All output channel "
	"editing functions will act on it."},

	{"geto",
	"geto\n"
	"\n"
	"Return the name of the current output channel or nil if none."},

	{"setunit",
	"setunit tics_per_unit\n"
	"\n"
	"Set the time resolution of the sequencer to given number of "
	"ticks per whole note (1 unit note = 4 quarter notes). "
	"The default is 96 ticks, which is the default of the MIDI standard."},

	{"getunit",
	"getunit\n"
	"\n"
	"Return the number of ticks in a whole note."},

	{"fac",
	"fac tempo_factor\n"
	"\n"
	"Set the tempo factor for play and record to the given integer "
	"value. The tempo factor must be between 50 (play half of the "
	"real tempo) and 200 (play at twice the real tempo)."},

	{"getfac",
	"getfac\n"
	"\n"
	"Return the current tempo factor."},

	{"t",
	"t beats_per_minute\n"
	"\n"
	"Set the tempo at the current song position."},

	{"mins",
	"mins amount sig\n"
	"\n"
	"Insert the given amount of blank measures at the "
	"current song position. "
	"The time signature used is in the {num,~denom} format. "
	"If the time signature is "
	"an empty list (i.e. {}) then the time signature at the "
	"current position is used."},

	{"mcut",
	"mcut\n"
	"\n"
	"Cut the current selection of all tracks, including the "
	"time structure."},

	{"mdup",
	"mdup where\n"
	"\n"
	"Duplicate the current selection inserting a copy of it at "
	"the given position in measures. "
	"If the given position is positive it's relative to the end "
	"of the current selection}, if it's negative it's relative to "
	"the beginning of the current selection."},

	{"minfo",
	"minfo\n"
	"\n"
	"Print the meta-track (tempo changes, time signature changes."},

	{"mtempo",
	"mtempo\n"
	"\n"
	"Return the tempo at the current song position. The unit is beats "
	"per minute."},

	{"msig",
	"msig\n"
	"\n"
	"Return the time signature at the current song position. The result "
	"is a two number list: numerator and denominator."},

	{"mend",
	"mend\n"
	"\n"
	"Return the ending measure of the song (i.e. its size in measures)."},

	{"ls",
	"ls\n"
	"\n"
	"List all tracks, channels, filters and various default values."},

	{"save",
	"save filename\n"
	"\n"
	"Save the song into the given file. The file name is a "
	"quoted string."},

	{"load",
	"load filename\n"
	"\n"
	"Load the song from the given file. The file name is a "
	"quoted string. The current song will be overwritten."},

	{"reset",
	"reset\n"
	"\n"
	"Destroy completely the song, useful to start a new song without "
	"restarting the program."},

	{"export",
	"export filename\n"
	"\n"
	"Save the song into the given standard MIDI file. The file name "
	"is a quoted string."},

	{"import",
	"import filename\n"
	"\n"
	"Load the song from the given standard MIDI file. The file name "
	"is a quoted string. The current song will be overwritten. "
	"Only MIDI file formats 0 and 1 are supported."},

	{"u",
	"u\n"
	"\n"
	"Undo last operation saved for undo."},

	{"ul",
	"ul\n"
	"\n"
	"List operations saved for undo."},

	{"dlist",
	"dlist\n"
	"\n"
	"Return the list of attached devices (list of numbers)."},

	{"dnew",
	"dnew devnum path mode\n"
	"\n"
	"Add the given MIDI device number, with the given path and mode. "
	"The mode may be on if the following:\n"
	"    ro - read-only, for input only devices\n"
	"    wo - write-only, for output only devices\n"
	"    rw - read and write.\n"
	"If midish is configured to use ALSA (default on Linux systems) "
	"then the device path is the ALSA sequencer port, as "
	"listed by the aseqdump linux command (ex. 28:0). "
	"If nil is given instead of the path, then the port is not "
	"connected to any existing port}, this allows other ALSA sequencer "
	"clients to subscribe to it and to provide events to midish or to "
	"consume events midish sends to the port."},

	{"ddel",
	"ddel devnum\n"
	"\n"
	"Remove the given device number."},

	{"dmtcrx",
	"dmtcrx devnum\n"
	"\n"
	"Use given device number as MTC source. "
	"In this case, midish will relocate, start and stop according to "
	"incoming MTC messages. Midish will generate its clock ticks from "
	"MTC, meaning that it will run at the same speed as the MTC device. "
	"This is useful to synchronize midish to an audio multi-tracker or any "
	"MTC capable audio application. If the device number is nil, then MTC "
	"messages are ignored and the internal timer will be used instead."},

	{"dmmctx",
	"dmmctx devlist\n"
	"\n"
	"Configure the given devices to transmit MMC start, stop "
	"and relocate messages. Useful to control MMC-capable audio "
	"applications from midish. By default, devices transmit MMC."},

	{"dclktx",
	"dclktx devlist\n"
	"\n"
	"Configure the given devices to transmit MIDI clock information "
	"(MIDI ticks, MIDI start and MIDI stop events). Useful to "
	"synchronize an external sequencer to midish."},

	{"dclkrx",
	"dclkrx devnum\n"
	"\n"
	"Set the given device number to be the master MIDI clock source. "
	"It will give midish MIDI ticks, MIDI start and MIDI stop events. This "
	"useful to synchronize midish to an external sequencer. "
	"If the device number is nil, then the internal clock will be "
	"used and midish will act as master clock."},

	{"dclkrate",
	"dclkrate devnum tics_per_unit\n"
	"\n"
	"Set the number of ticks in a whole note that are transmitted to the "
	"MIDI device. Default value is 96 ticks. This is the standard MIDI "
	"value and its not recommended to change it."},

	{"dinfo",
	"dinfo devnum\n"
	"\n"
	"Print some information about the MIDI device."},

	{"dixctl",
	"dixctl devnum ctlset\n"
	"\n"
	"Setup the list of controllers that are expected to be received "
	"as 14-bit numbers (i.e. both coarse and fine MIDI controller messages "
	"will be expected). By default only coarse values are used, if unsure "
	"let this list empty."},

	{"doxctl",
	"doxctl devnum ctlset\n"
	"\n"
	"Setup the list of controllers that will be transmitted "
	"as 14-bit numbers (both coarse and fine MIDI controller messages)."},

	{"diev",
	"diev devnum flags\n"
	"\n"
	"Configure the device to process as a single event the following "
	"patterns of input MIDI messages.\n\n"
	" * xpc -  group bank select controllers (0 and 32) with program\n"
	"   changes into a signle xpc event.\n\n"
	" * nrpn - group NRPN controllers (98 and 99) with data entry\n"
	"   controllers (6 and 38) into a single nrpn event.\n\n"
	" * rpn - similart to nrpn, but for RPN controllers (100 and 101).\n\n"
	"By default all of the above are enabled, which allows banks, NRPNs "
	"and RPNs to be handled by midish the standard way. "
	"It makes sense to disable grouping of above messages on rare hardware "
	"that maps above-mentioned controller numbers (0, 6, 32, 38, 98, 99, "
	"100, 101) to other parameters than bank number and NRPN/RPN."},

	{"doev",
	"doev devnum flags\n"
	"\n"
	"Same as the diev functino, but for output messages."},

	{"ctlconf",
	"ctlconf name number defval\n"
	"\n"
	"Configure the given controller number with the given name, "
	"and default value. If the default value is nil then the "
	"corresponding controller events are not grouped into frames."},

	{"ctlconfx",
	"ctlconfx name ctl defval\n"
	"\n"
	"Same as ctlconf, but for 14-bit controllers. Thus defval is in the "
	"range 0..16383."},

	{"ctlunconf",
	"ctlunconf name\n"
	"\n"
	"Unconfigure the given controller."},

	{"ctlinfo",
	"ctlinfo\n"
	"\n"
	"Print the list of configured controllers."},

	{"evpat",
	"evpat name pattern\n"
	"\n"
	"Define a new event type corresponding to the given system exclusive "
	"message pattern. The pattern is a list of bytes or event parameter "
	"identifiers (aka atoms).  The following atoms are supported: v0, v0_lo, "
	"v0_hi, v1, v1_lo, v1_hi. They correspond to the full 7-bit value (coarse "
	"parameter), the low 7-bit nibble and the high 7-bit nibble (fine grained "
	"parameters) of the first and second parameters respectively. Example:\n"
	"\n"
	"\tevpat master {0xf0 0x7f 0x7f 0x04 0x01 v0_lo v0_hi 0xf7}\n"
	"\n"
	"defines a new event type for the standard master volume system exclusive "
	"message."},

	{"evinfo",
	"evinfo\n"
	"\n"
	"Print the list of event patterns."},

	{"m",
	"m onoff\n"
	"\n"
	"Set the mode of the metronome. The following modes are available:\n"
	"    on - turned on for both playback and recording\n"
	"    rec - turned on for recording only\n"
	"    off - turned off"},

	{"metrocf",
	"metrocf eventhi eventlo\n"
	"\n"
	"Select the notes that the metronome plays. The pair of events must "
	"be note-on events."},

	{"tap",
	"tap mode\n"
	"\n"
	"Set the way start is triggered by events. The following modes are "
	"available:\n"
	"    off - no events trigger start\n"
	"    start - a single event triggers start\n"
	"    tempo - two events trigger start}, the time between them\n"
	"        corresponds to one beat and is used to determine the\n"
	"        initial tempo."},

	{"tapev",
	"tapev evspec\n"
	"\n"
	"Set events set used to trigger start when tap mode is set to "
	"start or tempo."},

	{"info",
	"info\n"
	"\n"
	"Display the list of built-in and user-defined procedures and global "
	"variables."},

	{"print",
	"print value\n"
	"\n"
	"Display the given value."},

	{"err",
	"err message\n"
	"\n"
	"Display the given string and abort the current statement."},

	{"h",
	"h function\n"
	"\n"
	"Describe the given function."},

	{"exec",
	"exec filename\n"
	"\n"
	"Read and execute the give script (the file name is a quoted "
	"string). The execution of the script is aborted on error. If the "
	"script executes an exit statement, only the script is terminated."},

	{"debug",
	"debug flag value\n"
	"\n"
	"Set given debug-flag to given the (integer) value. "
	"If the value is 0, the corresponding debug-info is turned off. "
	"The flag may be:\n"
	"    filt - show events passing through the current filter\n"
	"    mididev - show raw MIDI traffic\n"
	"    mixout - show conflicts in the output MIDI merger\n"
	"    norm - show events in the input normalizer\n"
	"    pool - show pool usage on exit\n"
	"    song - show start/stop events\n"
	"    timo - show timer internal errors\n"
	"    mem - show memory usage"},

	{"version",
	"version\n"
	"\n"
	"Display midish version."},

	{"panic",
	"panic\n"
	"\n"
	"Abort (and core-dump)."},

	{"shut",
	"shut\n"
	"\n"
	"Stop all sounds."},

	{"proclist",
	"proclist\n"
	"\n"
	"Return the list of user-defined procs."},

	{"builtinlist",
	"builtinlist\n"
	"\n"
	"Return the list of builtin commands."},

	{"intro",
	"Midish is a MIDI sequencer with shell-like interface. To "
	"obtain help about any midish command, type:\n"
	"\n"
	"    h <key_word>\n"
	"\n"
	"Once MIDI devices are setup (see dnew command), "
	"performance mode can be started or stopped with "
	"the following commants:\n"
	"    i - idle, prcess input and send it to the outputs\n"
	"    p - same as previous but also play all tracks\n"
	"    r - same as previous but also record current track\n"
	"    s - stop, exit performance mode (releases devices)\n"
	"\n"
	"In performance mode, input is filtered (if a fiter is defined) "
	"and the result stored in the current "
	"track (if recording). "
	"Then, it is mixed with played "
	"tracks (if any) and the mix is sent to the MIDI outputs."
	},

	{NULL, NULL}
};

#define IS_SEP(c) ((c) == ' ' || (c) == '\n' || (c) == 0)

void
help_fmt(char *text)
{
#define LINE_LEN	68
	char out[LINE_LEN + 1 + 1], *endl;
	size_t out_len, word_len;
	int c, t;

	endl = text;
	out_len = word_len = 0;
	for (;;) {
		c = endl[word_len];

		/* if not an end-of-word, continue */
		if (!IS_SEP(c)) {
			word_len++;
			continue;
		}

		/* finished a word, check if flush is needed */
		if (out_len + word_len > LINE_LEN) {
			/* flush */
			out[out_len++] = '\n';
			out[out_len++] = 0;
			textout_putstr(tout, out);
			out_len = 0;
			/* discard space separator */
			endl++;
			word_len--;
		}

		/* copy word */
		while (word_len > 0) {
			t = *endl++;
			if (t == '~')
				t = ' ';
			out[out_len++] = t;
			word_len--;
		}

		/* finished a word, check if flush is needed */
		if (c == '\n' || c == 0) {
			/* flush */
			out[out_len++] = '\n';
			out[out_len++] = 0;
			textout_putstr(tout, out);
			out_len = 0;
			/* discard space separator */
			endl++;
			word_len--;
		}

		if (c == 0)
			break;

		word_len++;
	}
}
