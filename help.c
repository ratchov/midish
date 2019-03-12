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

char help_tlist[] =
	"tlist\n"
	"\n"
	"Return the list of tracks.";

char help_tnew[] =
	"tnew name\n"
	"\n"
	"Create a track with the given name.";

char help_tdel[] =
	"tdel\n"
	"\n"
	"Delete the current track.";

char help_tren[] =
	"tren name\n"
	"\n"
	"Rename the current track to the given name.";

char help_texists[] =
	"texists name\n"
	"\n"
	"Return 1 if a track with the give name exists, 0 otherwise.";

char help_taddev[] =
	"taddev measure beat tic event\n"
	"\n"
	"Put the given event at the give position of the current track.";
  
char help_tsetf[] =
	"tsetf filtname\n"
	"\n"
	"Set the default filter of the current track to the given filter.";

char help_tgetf[] =
	"tgetf\n"
	"\n"
	"Return the default filter of the current track, returns nil\n"
	"if none.";

char help_tcheck[] =
	"tcheck\n"
	"\n"
	"Check the current track for orphaned notes, nested notes, and "
	"other anomalies. Remove multiple controllers in the same clock "
	"tick.";

char help_trewrite[] =
	"trewrite\n"
	"\n"
	"Rewrite the current track note-by-note.";

char help_tcut[] =
	"tcut\n"
	"\n"
	"Cut the current selection of the current track.";

char help_tclr[] =
	"tclr\n"
	"\n"
	"Clear the current selection of the current track. Only events "
	"matching the current event selection (see the ev function) are "
	"removed.";

char help_tins[] =
	"tins amount\n"
	"\n"
	"Insert the given amount of empty measures at the current position "
	"in the current track.";

char help_tpaste[] =
	"tpaste\n"
	"\n"
	"Paste track portion (copied with tcopy) at the current position "
	"of the current track.";

char help_tcopy[] =
	"tcopy\n"
	"\n"
	"Copy the current selection of the current track so that it could "
	"be pasted with the tpaste function. "
	"Only events matching the current event selection "
	"are copied (see ev function).";

char help_tmerge[] =
	"tmerge source\n"
	"\n"
	"Merge the given track into the current track.";

char help_tquanta[] =
	"tquanta rate\n"
	"\n"
	"Round event positions to the nearest exact position, as defined "
	"by the setq function. This affects events of the current "
	"selection of the current track. Event positions are rounded to "
	"the nearest tick multiple of the quantization step; Rate must be "
	"between 0 and 100: 0 means no quantization and 100 means full "
	"quantization.";

char help_tquantf[] =
	"tquantf rate\n"
	"\n"
	"Round frame positions (eg. note positions) to the nearest exact "
	"position, as defined by the setq function. Frame length (eg. "
	"note lengths) is preserved. Rate must be between 0 and 100: "
	"0 means no quantization and 100 means full quantization.";

char help_ttransp[] =
	"ttransp halftones\n"
	"\n"
	"Transpose note events of current selection of the current track, "
	"by the given number of half-tones. Only events matching the current "
	"event selection are transposed (see ev function).";

char help_tvcurve[] =
	"tvcurve weight\n"
	"\n"
	"Adjust velocity of note events, using the given weight in "
	"the -63..63 range. Applies only to note events of current "
	"selection of the current track (see ev function).";

char help_tevmap[] =
	"tevmap source dest\n"
	"\n"
	"Convert events matching the given source event set to the given destination "
	"event set. Changes apply to in the current selection of the current track. "
	"Both event sets must have the same number of devices, "
	"channels, notes, controllers etc..";

char help_mute[] =
	"mute trackname\n"
	"\n"
	"Mute the given track.";

char help_unmute[] =
	"unmute trackname\n"
	"\n"
	"Unmute the given track.";

char help_getmute[] =
	"getmute trackname\n"
	"\n"
	"Return 1 if the given track is muted and 0 otherwise.";

char help_tclist[] =
	"tclist\n"
	"\n"
	"Return the list of channels used by events stored in the "
	"current track.";

char help_tinfo[] =
	"tinfo\n"
	"\n"
	"Scan the current selection of the current track, an for each "
	"measure display the number of events that match the current "
	"event selection.";

char help_inew[] =
	"inew name channel\n"
	"\n"
	"Create an new input with the given name associated "
	"to the given device and MIDI channel pair.";

char help_iset[] =
	"iset channel\n"
	"\n"
	"Set the device and channel number of the current input. All "
	"filters are updated to use the new channel setting as if the "
	"appropriate fchin function was called for each filter.";

char help_idel[] =
	"idel\n"
	"\n"
	"Delete the current input.";

char help_iren[] =
	"iren newname\n"
	"\n"
	"Rename the current input to the given name.";

char help_ilist[] =
	"ilist\n"
	"\n"
	"Return the list of inputs.";

char help_iexists[] =
	"iexists channame\n"
	"\n"
	"Return 1 if an input with the give name exists, 0 otherwise.";

char help_igetc[] =
	"igetc\n"
	"\n"
	"Return the MIDI channel number of the current input.";

char help_igetd[] =
	"igetd\n"
	"\n"
	"Return the device number of the current input.";

char help_iaddev[] =
	"iaddev event\n"
	"\n"
	"Add the given event to the configuration of the "
	"current channel, it's not used yet.";

char help_irmev[] =
	"irmev evspec\n"
	"\n"
	"Remove all events matching the given evspec from the configuration "
	"of the current channel.";

char help_iinfo[] =
	"iinfo\n"
	"\n"
	"Print all config events of the current input.";

char help_onew[] =
	"onew name channel\n"
	"\n"
	"Create an new output with the given name and the given device "
	"and MIDI channel. Outputs contain a built-in filter with the "
	"same name; by defaut it maps all inputs to the newly created "
	"output.";

char help_odel[] =
	"odel\n"
	"\n"
	"Delete current output.";

char help_oset[] =
	"oset channel\n"
	"\n"
	"Set the device and MIDI channel numbers of the current output. "
	"All filters are updated to use the new setting as if the "
	"appropriate fchout function was called for each filter.";

char help_oren[] =
	"oren newname\n"
	"\n"
	"Rename the current output to the given name.";

char help_oexists[] =
	"oexists channame\n"
	"\n"
	"Return 1 if an output with the give name exists, 0 otherwise.";

char help_ogetc[] =
	"ogetc\n"
	"\n"
	"Return the MIDI channel number of the current output.";

char help_ogetd[] =
	"ogetd\n"
	"\n"
	"Return the device number of the current output.";

char help_oaddev[] =
	"oaddev event\n"
	"\n"
	"Add the given event to the configuration of the current output.";

char help_ormev[] =
	"ormev evspec\n"
	"\n"
	"Remove all events matching the given set from the configuration "
	"of the current output.";

char help_oinfo[] =
	"oinfo\n"
	"\n"
	"Print all config events of the current output.";

char help_olist[] =
	"olist\n"
	"\n"
	"Return list of outputs.";

char help_fnew[] =
	"fnew filtname\n"
	"\n"
	"Create an new filter with the given name.";

char help_fdel[] =
	"fdel\n"
	"\n"
	"Delete the current filter.";

char help_fren[] =
	"fren newname\n"
	"\n"
	"Rename the current filter to the given name.";

char help_fexists[] =
	"fexists filtname\n"
	"\n"
	"Return 1 if a filter with the given name exists, 0 otherwise.";

char help_freset[] =
	"freset\n"
	"\n"
	"Remove all rules from the current filter.";

char help_finfo[] =
	"finfo\n"
	"\n"
	"List all rules of the current filter.";

char help_flist[] =
	"flist\n"
	"\n"
	"Return the list of existing filters.";

char help_fchgin[] =
	"fchgin old_evspec new_evspec\n"
	"\n"
	"Rewrite current filter rules to consume  new event set in place "
	"of the old event set. This means that each rule previously "
	"matching old set on the input will start "
	"matching new set instead.";

char help_fswapin[] =
	"fswapin evspec1 evspec2\n"
	"\n"
	"Similar to the fchgin function but swap the two event sets in "
	"the source events set of each rule.";

char help_fchgout[] =
	"fchgout from to\n"
	"\n"
	"Rewrite all filtering rules of the current filter to produce "
	"the new event set instead of the old event set. This means "
	"that each rule previously producing the old set will "
	"start producing new set instead.";

char help_fswapout[] =
	"fswapout from to\n"
	"\n"
	"Similar to the fchgout function, but swap the two event sets in "
	"the destination events set of each rule.";

char help_fmap[] =
	"fmap source dest\n"
	"\n"
	"Add a new mapping rule to the current filter. The events matching "
	"the source event set will be converted to the destination event set. "
	"Source and destination sets must have the same number of devices, "
	"channels, notes, controllers etc..";

char help_funmap[] =
	"funmap source dest\n"
	"\n"
	"Remove the given event mapping rules from the current filter. Any "
	"mapping rule with source included in the given source and destination "
	"inluded in the given destination is deleted.";

char help_ftransp[] =
	"ftransp evspec half_tones\n"
	"\n"
	"Transpose events matching the given event set by the given number "
	"of halftones.";

char help_fvcurve[] =
	"fvcurve evspec weight\n"
	"\n"
	"Adjust velocity of the given note events, using "
	"the given weight in the -63..63 range.  If the weight is "
	"negative then sensitivity is decreased. If it's positive then "
	"sensitivity is increased. If it's zero the velocity is unchanged.";

char help_xnew[] =
	"xnew sysexname\n"
	"\n"
	"Create a new bank of sysex messages with the given name.";

char help_xdel[] =
	"xdel\n"
	"\n"
	"Delete the current bank of sysex messages.";

char help_xren[] =
	"xren newname\n"
	"\n"
	"Rename the current sysex bank to the given name.";

char help_xexists[] =
	"xexists sysexname\n"
	"\n"
	"Return 1 if a sysex bank with the given name exists, 0 otherwise.";

char help_xrm[] =
	"xrm data\n"
	"\n"
	"Remove all sysex messages starting with the given pattern from the "
	"current sysex bank. The given pattern is a list of bytes; an empty "
	"pattern matches any sysex message.";

char help_xsetd[] =
	"xsetd device pattern\n"
	"\n"
	"Set device number of all sysex messages starting "
	"with the given pattern in the current sysex bank. The given pattern "
	"is a list of bytes; an empty pattern matches any sysex message.";

char help_xadd[] =
	"xadd device message\n"
	"\n"
	"Add the given sysex message to the current sysex bank. The message is a "
	"list containing the system exclusive message bytes. The given device number "
	"specifies the device to which the message will be sent when performance mode "
	"is entered.";

char help_xinfo[] =
	"xinfo\n"
	"\n"
	"Print all sysex messages of the current sysex bank. Messages that are "
	"too long to be desplayed on a single line are truncated and dots "
	"are displayed.";

char help_ximport[] =
	"ximport device path\n"
	"\n"
	"Replace contents of the current sysex bank by contents of the given "
	".syx file; messages are assigned to the given device number.";

char help_xexport[] =
	"xexport path\n"
	"\n"
	"Store contents of the current sysex bank in the given .syx file.";

char help_xlist[] =
	"xlist\n"
	"\n"
	"Return the list of sysex banks.";

char help_i[] =
	"i\n"
	"\n"
	"Enter performance mode. Start processing MIDI input and "
	"generating MIDI output. Data passes through the current filter "
	"(if any) or through the current track's filter (if any).";

char help_p[] =
	"p\n"
	"\n"
	"Play the song from the current position. Input passes through the "
	"current filter (if any) or through the current track's filter "
	"(if any).";

char help_r[] =
	"r\n"
	"\n"
	"Play the song and record the input. Input passes through the current "
	"filter (if any) or through the current track's filter (if any). "
	"On startup, this function plays one measure of countdown before the data "
	"start being recorded.";

char help_s[] =
	"s\n"
	"\n"
	"Stop performance and release MIDI devices.";

char help_ev[] =
	"ev evspec\n"
	"\n"
	"Set the current event selection. Most track editing functions will "
	"act only on events matching evspec ignoring all other events.";

char help_setq[] =
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
	"as quantization step, then quatization is disabled.";

char help_getq[] =
	"getq\n"
	"\n"
	"Return the current quatization step.";

char help_g[] =
	"g measure\n"
	"\n"
	"Set the current song position pointer to the given measure number. "
	"Record and playback will start a that position. This also defines "
	"the beginning of the current selection used by most track editing "
	"functions.";

char help_getpos[] =
	"getpos\n"
	"\n"
	"Return the current song position pointer which is also the start "
	"position of the current selection.";

char help_sel[] =
	"sel length\n"
	"\n"
	"Set the length of the current selection to "
	"the given number of  measures. "
	"The current selection start at the current position "
	"set with the g function.";

char help_getlen[] =
	"getlen\n"
	"\n"
	"Return the length (in measures) of the current selection.";

char help_ct[] =
	"ct trackname\n"
	"\n"
	"Set the current track. The current track "
	"is the one that will be recorded. Most track editing "
	"functions act on it.";

char help_gett[] =
	"gett\n"
	"\n"
	"Return the current track (if any) or nil.";

char help_cf[] =
	"cf filtname\n"
	"\n"
	"Set the current filter to the given filter. The current filter "
	"is the one used to process input MIDI events in performance mode. "
	"It's also the one affected by all filter editing functions.";

char help_getf[] =
	"getf\n"
	"\n"
	"Return the current filter or nil if none.";

char help_cx[] =
	"cx sysexname\n"
	"\n"
	"Set the current sysex bank, i.e. the one that will be recorded. "
	"The current sysex back is the one affected by all sysex editing "
	"functions.";

char help_getx[] =
	"getx\n"
	"\n"
	"Return the current sysex bank or nil if none.";

char help_ci[] =
	"ci channame\n"
	"\n"
	"Set the current (named) input channel. All input channel "
	"editing functions will act on it.";

char help_geti[] =
	"geti\n"
	"\n"
	"Return the name of the current input channel or nil if none.";

char help_co[] =
	"co channame\n"
	"\n"
	"Set the current (named) output channel. All output channel "
	"editing functions will act on it.";

char help_geto[] =
	"geto\n"
	"\n"
	"Return the name of the current output channel or nil if none.";

char help_setunit[] =
	"setunit tics_per_unit\n"
	"\n"
	"Set the time resolution of the sequencer to given number of "
	"ticks per whole note (1 unit note = 4 quarter notes). "
	"The default is 96 ticks, which is the default of the MIDI standard.";

char help_getunit[] =
	"getunit\n"
	"\n"
	"Return the number of ticks in a whole note.";

char help_fac[] =
	"fac tempo_factor\n"
	"\n"
	"Set the tempo factor for play and record to the given integer "
	"value. The tempo factor must be between 50 (play half of the "
	"real tempo) and 200 (play at twice the real tempo).";

char help_getfac[] =
	"getfac\n"
	"\n"
	"Return the current tempo factor.";

char help_t[] =
	"t beats_per_minute\n"
	"\n"
	"Set the tempo at the current song position.";

char help_mins[] =
	"mins amount sig\n"
	"\n"
	"Insert the given amount of blank measures at the "
	"current song position. "
	"The time signature used is in the {num,~denom} format. "
	"If the time signature is "
	"an empty list (i.e. {}) then the time signature at the "
	"current position is used.";

char help_mcut[] =
	"mcut\n"
	"\n"
	"Cut the current selection of all tracks, including the "
	"time structure.";

char help_mdup[] =
	"mdup where\n"
	"\n"
	"Duplicate the current selection inserting a copy of it at "
	"the given position in measures. "
	"If the given position is positive it's relative to the end "
	"of the current selection; if it's negative it's relative to "
	"the beginning of the current selection.";

char help_minfo[] =
	"minfo\n"
	"\n"
	"Print the meta-track (tempo changes, time signature changes.";

char help_mtempo[] =
	"mtempo\n"
	"\n"
	"Return the tempo at the current song position. The unit is beats "
	"per minute.";

char help_msig[] =
	"msig\n"
	"\n"
	"Return the time signature at the current song position. The result "
	"is a two number list: numerator and denominator.";

char help_mend[] =
	"mend\n"
	"\n"
	"Return the ending measure of the song (i.e. its size in measures).";

char help_ls[] =
	"ls\n"
	"\n"
	"List all tracks, channels, filters and various default values.";

char help_save[] =
	"save filename\n"
	"\n"
	"Save the song into the given file. The file name is a "
	"quoted string.";

char help_load[] =
	"load filename\n"
	"\n"
	"Load the song from the given file. The file name is a "
	"quoted string. The current song will be overwritten.";

char help_reset[] =
	"reset\n"
	"\n"
	"Destroy completely the song, useful to start a new song without "
	"restarting the program.";

char help_export[] =
	"export filename\n"
	"\n"
	"Save the song into the given standard MIDI file. The file name "
	"is a quoted string.";

char help_import[] =
	"import filename\n"
	"\n"
	"Load the song from the given standard MIDI file. The file name "
	"is a quoted string. The current song will be overwritten. "
	"Only MIDI file formats 0 and 1 are supported.";

char help_u[] =
	"u\n"
	"\n"
	"Undo last operation saved for undo.";

char help_ul[] =
	"ul\n"
	"\n"
	"List operations saved for undo.";

char help_dlist[] =
	"dlist\n"
	"\n"
	"Return the list of attached devices (list of numbers).";

char help_dnew[] =
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
	"connected to any existing port; this allows other ALSA sequencer "
	"clients to subscribe to it and to provide events to midish or to "
	"consume events midish sends to the port.";

char help_ddel[] =
	"ddel devnum\n"
	"\n"
	"Remove the given device number.";

char help_dmtcrx[] =
	"dmtcrx devnum\n"
	"\n"
	"Use given device number as MTC source. "
	"In this case, midish will relocate, start and stop according to "
	"incoming MTC messages. Midish will generate its clock ticks from "
	"MTC, meaning that it will run at the same speed as the MTC device. "
	"This is useful to synchronize midish to an audio multi-tracker or any "
	"MTC capable audio application. If the device number is nil, then MTC "
	"messages are ignored and the internal timer will be used instead.";

char help_dmmctx[] =
	"dmmctx devlist\n"
	"\n"
	"Configure the given devices to transmit MMC start, stop "
	"and relocate messages. Useful to control MMC-capable audio "
	"applications from midish. By default, devices transmit MMC.";

char help_dclktx[] =
	"dclktx devlist\n"
	"\n"
	"Configure the given devices to transmit MIDI clock information "
	"(MIDI ticks, MIDI start and MIDI stop events). Useful to "
	"synchronize an external sequencer to midish.";

char help_dclkrx[] =
	"dclkrx devnum\n"
	"\n"
	"Set the given device number to be the master MIDI clock source. "
	"It will give midish MIDI ticks, MIDI start and MIDI stop events. This "
	"useful to synchronize midish to an external sequencer. "
	"If the device number is nil, then the internal clock will be "
	"used and midish will act as master clock.";

char help_dclkrate[] =
	"dclkrate devnum tics_per_unit\n"
	"\n"
	"Set the number of ticks in a whole note that are transmitted to the "
	"MIDI device. Default value is 96 ticks. This is the standard MIDI "
	"value and its not recommended to change it.";

char help_dinfo[] =
	"dinfo devnum\n"
	"\n"
	"Print some information about the MIDI device.";

char help_dixctl[] =
	"dixctl devnum ctlset\n"
	"\n"
	"Setup the list of controllers that are expected to be received "
	"as 14-bit numbers (i.e. both coarse and fine MIDI controller messages "
	"will be expected). By default only coarse values are used, if unsure "
	"let this list empty.";

char help_doxctl[] =
	"doxctl devnum ctlset\n"
	"\n"
	"Setup the list of controllers that will be transmitted "
	"as 14-bit numbers (both coarse and fine MIDI controller messages).";

char help_diev[] =
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
	"100, 101) to other parameters than bank number and NRPN/RPN.";

char help_doev[] =
	"doev devnum flags\n"
	"\n"
	"Same as the diev functino, but for output messages.";

char help_ctlconf[] =
	"ctlconf name number defval\n"
	"\n"
	"Configure the given controller number with the given name, "
	"and default value. If the default value is nil then the "
	"corresponding controller events are not grouped into frames.";

char help_ctlconfx[] =
	"ctlconfx name ctl defval\n"
	"\n"
	"Same as ctlconf, but for 14-bit controllers. Thus defval is in the "
	"range 0..16383.";

char help_ctlunconf[] =
	"ctlunconf name\n"
	"\n"
	"Unconfigure the given controller.";

char help_ctlinfo[] =
	"ctlinfo\n"
	"\n"
	"Print the list of configured controllers.";

char help_evpat[] =
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
	"message.";

char help_evinfo[] =
	"evinfo\n"
	"\n"
	"Print the list of event patterns.";

char help_m[] =
	"m onoff\n"
	"\n"
	"Set the mode of the metronome. The following modes are available:\n"
	"    on - turned on for both playback and recording\n"
	"    rec - turned on for recording only\n"
	"    off - turned off";

char help_metrocf[] =
	"metrocf eventhi eventlo\n"
	"\n"
	"Select the notes that the metronome plays. The pair of events must "
	"be note-on events.";

char help_tap[] =
	"tap mode\n"
	"\n"
	"Set the way start is triggered by events. The following modes are "
	"available:\n"
	"    off - no events trigger start\n"
	"    start - a single event triggers start\n"
	"    tempo - two events trigger start; the time between them\n"
	"        corresponds to one beat and is used to determine the\n"
	"        initial tempo.";

char help_tapev[] =
	"tapev evspec\n"
	"\n"
	"Set events set used to trigger start when tap mode is set to "
	"start or tempo.";

char help_info[] =
	"info\n"
	"\n"
	"Display the list of built-in and user-defined procedures and global "
	"variables.";

char help_print[] =
	"print value\n"
	"\n"
	"Display the given value.";

char help_err[] =
	"err message\n"
	"\n"
	"Display the given string and abort the current statement.";

char help_h[] =
	"h function\n"
	"\n"
	"Describe the given function.";

char help_exec[] =
	"exec filename\n"
	"\n"
	"Read and execute the give script (the file name is a quoted "
	"string). The execution of the script is aborted on error. If the "
	"script executes an exit statement, only the script is terminated.";

char help_debug[] =
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
	"    mem - show memory usage";

char help_version[] =
	"version\n"
	"\n"
	"Display midish version.";

char help_panic[] =
	"panic\n"
	"\n"
	"Abort (and core-dump).";

char help_shut[] =
	"shut\n"
	"\n"
	"Stop all sounds.";

char help_proclist[] =
	"proclist\n"
	"\n"
	"Return the list of user-defined procs.";

char help_builtinlist[] =
	"builtinlist\n"
	"\n"
	"Return the list of builtin commands.";

struct help help_list[] = {
	{"tlist", help_tlist},
	{"tnew", help_tnew},
	{"tdel", help_tdel},
	{"tren", help_tren},
	{"texists", help_texists},
	{"taddev", help_taddev},
	{"tsetf", help_tsetf},
	{"tgetf", help_tgetf},
	{"tcheck", help_tcheck},
	{"trewrite", help_trewrite},
	{"tcut", help_tcut},
	{"tclr", help_tclr},
	{"tins", help_tins},
	{"tpaste", help_tpaste},
	{"tcopy", help_tcopy},
	{"tquanta", help_tquanta},
	{"tquantf", help_tquantf},
	{"ttransp", help_ttransp},
	{"tvcurve", help_tvcurve},
	{"tevmap", help_tevmap},
	{"tmerge", help_tmerge},
	{"mute", help_mute},
	{"unmute", help_unmute},
	{"getmute", help_getmute},
	{"tclist", help_tclist},
	{"tinfo", help_tinfo},
	{"inew", help_inew},
	{"iset", help_iset},
	{"idel", help_idel},
	{"iren", help_iren},
	{"iexists", help_iexists},
	{"igetc", help_igetc},
	{"igetd", help_igetd},
	{"iaddev", help_iaddev},
	{"irmev", help_irmev},
	{"iinfo", help_iinfo},
	{"ilist", help_ilist},
	{"onew", help_onew},
	{"oset", help_oset},
	{"odel", help_odel},
	{"oren", help_oren},
	{"oexists", help_oexists},
	{"ogetc", help_ogetc},
	{"ogetd", help_ogetd},
	{"oaddev", help_oaddev},
	{"ormev", help_ormev},
	{"oinfo", help_oinfo},
	{"olist", help_olist},
	{"fnew", help_fnew},
	{"fdel", help_fdel},
	{"fren", help_fren},
	{"fexists", help_fexists},
	{"freset", help_freset},
	{"finfo", help_finfo},
	{"flist", help_flist},
	{"fchgin", help_fchgin},
	{"fswapin", help_fswapin},
	{"fchgout", help_fchgout},
	{"fswapout", help_fswapout},
	{"fmap", help_fmap},
	{"funmap", help_funmap},
	{"ftransp", help_ftransp},
	{"fvcurve", help_fvcurve},
	{"xnew", help_xnew},
	{"xdel", help_xdel},
	{"xren", help_xren},
	{"xexists", help_xexists},
	{"xrm", help_xrm},
	{"xsetd", help_xsetd},
	{"xadd", help_xadd},
	{"xinfo", help_xinfo},
	{"ximport", help_ximport},
	{"xexport", help_xexport},
	{"xlist", help_xlist},
	{"i", help_i},
	{"p", help_p},
	{"r", help_r},
	{"s", help_s},
	{"ev", help_ev},
	{"setq", help_setq},
	{"getq", help_getq},
	{"g", help_g},
	{"getpos", help_getpos},
	{"sel", help_sel},
	{"getlen", help_getlen},
	{"ct", help_ct},
	{"gett", help_gett},
	{"cf", help_cf},
	{"getf", help_getf},
	{"cx", help_cx},
	{"getx", help_getx},
	{"ci", help_ci},
	{"geti", help_geti},
	{"co", help_co},
	{"geto", help_geto},
	{"setunit", help_setunit},
	{"getunit", help_getunit},
	{"fac", help_fac},
	{"getfac", help_getfac},
	{"t", help_t},
	{"mins", help_mins},
	{"mcut", help_mcut},
	{"mdup", help_mdup},
	{"minfo", help_minfo},
	{"mtempo", help_mtempo},
	{"msig", help_msig},
	{"mend", help_mend},
	{"ls", help_ls},
	{"save", help_save},
	{"load", help_load},
	{"reset", help_reset},
	{"export", help_export},
	{"import", help_import},
	{"u", help_u},
	{"ul", help_ul},
	{"dlist", help_dlist},
	{"dnew", help_dnew},
	{"ddel", help_ddel},
	{"dmtcrx", help_dmtcrx},
	{"dmmctx", help_dmmctx},
	{"dclktx", help_dclktx},
	{"dclkrx", help_dclkrx},
	{"dclkrate", help_dclkrate},
	{"dinfo", help_dinfo},
	{"dixctl", help_dixctl},
	{"doxctl", help_doxctl},
	{"diev", help_diev},
	{"doev", help_doev},
	{"ctlconf", help_ctlconf},
	{"ctlconfx", help_ctlconfx},
	{"ctlunconf", help_ctlunconf},
	{"ctlinfo", help_ctlinfo},
	{"evpat", help_evpat},
	{"evinfo", help_evinfo},
	{"m", help_m},
	{"metrocf", help_metrocf},
	{"tap", help_tap},
	{"tapev", help_tapev},
	{"info", help_info},
	{"print", help_print},
	{"err", help_err},
	{"h", help_h},
	{"exec", help_exec},
	{"debug", help_debug},
	{"version", help_version},
	{"panic", help_panic},
	{"shut", help_shut},
	{"proclist", help_proclist},
	{"builtinlist", help_builtinlist},
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
