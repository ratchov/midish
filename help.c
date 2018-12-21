#include <stddef.h>
#include "help.h"

char help_tlist[] =
	"tlist\n"
	"\n"
	"Return the list of tracks\n";

char help_tnew[] =
	"tnew trackname\n"
	"\n"
	"Create an empty track with the given name\n";

char help_tdel[] =
	"tdel\n"
	"\n"
	"Delete the current track.\n";

char help_tren[] =
	"tren newname\n"
	"\n"
	"Rename of the current track to the given name.\n";

char help_texists[] =
	"texists trackname\n"
	"\n"
	"Return 1 if the given track exists, 0 otherwise.\n";

char help_taddev[] =
	"taddev measure beat tic event\n"
	"\n"
	"Put the given event on the current track at the position. The\n"
	"position is in the ``{measure beat tick}'' format.\n";

char help_tsetf[] =
	"tsetf filtname\n"
	"\n"
	"Set the default filter of the current track to the given filter.\n"
	"It will be used in performace mode if there is no current filter.\n";

char help_tgetf[] =
	"tgetf\n"
	"\n"
	"Return the default filter of the current track, returns ``nil''\n"
	"if none.\n";

char help_tcheck[] =
	"tcheck\n"
	"\n"
	"Check the current track for orphaned notes, nested notes, and\n"
	"other anomalies; also removes multiple controllers in the same\n"
	"tick\n";

char help_trewrite[] =
	"trewrite\n"
	"\n"
	"Rewrite the current track.\n";

char help_tcut[] =
	"tcut\n"
	"\n"
	"Cut the current selection of the current track.\n";

char help_tclr[] =
	"tclr\n"
	"\n"
	"Clear the current selection of the current track. Only events\n"
	"matching the current event selection (see ``ev'' function). are\n"
	"removed.\n";

char help_tins[] =
	"tins amount\n"
	"\n"
	"Insert the given amount of empty measures at the current position\n"
	"in the current track.\n";

char help_tpaste[] =
	"tpaste\n"
	"\n"
	"Paste track portion (copied with ``tcopy'') at the current position\n"
	"of the current track.\n";

char help_tcopy[] =
	"tcopy\n"
	"\n"
	"Copy the current selection of the current track so that it could\n"
	"be pasted with ``tpaste''. Only events matching the current event\n"
	"selection (see ``ev'') are copied.\n";

char help_tmerge[] =
	"tmerge source\n"
	"\n"
	"Merge the given track into the current track\n";

char help_tquanta[] =
	"tquanta rate\n"
	"\n"
	"Round event positions to the nearest exact position, as defined\n"
	"by the see ``setq'' function. This affects events of the current\n"
	"selection of the current track. Event positions are rounded to\n"
	"the nearest tick multiple of the quantization step; Rate must be\n"
	"between 0 and 100: 0 means no quantization and 100 means full\n"
	"quantization.\n";

char help_tquantf[] =
	"tquantf rate\n"
	"\n"
	"Round frame positions (eg. note positions) to the nearest exact\n"
	"position, as defined by the ``setq'' function. Frame length (eg.\n"
	"note lengths) is preserved. Rate must be between 0 and 100:\n"
	"0 means no quantization and 100 means full quantization.\n";

char help_ttransp[] =
	"ttransp halftones\n"
	"\n"
	"Transpose note events of current selection of the current track,\n"
	"by the given number of half-tones. Only events matching the current\n"
	"event selection (see ``ev'') are transposed.\n";

char help_tevmap[] =
	"tevmap from to\n"
	"\n"
	"Convert events matching evspec1 (source) into events matching\n"
	"evspec2 (destination) in the current selection of the current track.\n"
	"Both evspec1 and evspec2 must have the same number of devices,\n"
	"channels, notes, controllers etc..\n";

char help_mute[] =
	"mute trackname\n"
	"\n"
	"Mute the given track\n";

char help_unmute[] =
	"unmute trackname\n"
	"\n"
	"Unmute the given track\n";

char help_getmute[] =
	"getmute trackname\n"
	"\n"
	"Return 1 if the given track is muted and 0 otherwise.\n";

char help_tclist[] =
	"tclist\n"
	"\n"
	"Return the list of channels used by events stored in the\n"
	"current track.\n";

char help_tinfo[] =
	"tinfo\n"
	"\n"
	"Scan the current selection of the current track, an for each\n"
	"measure display the number of events that match the current\n"
	"event selection\n";

char help_inew[] =
	"inew channame channum\n"
	"\n"
	"Create an new input channel with the given name\n"
	"and assigned the given device and MIDI channel.\n";

char help_iset[] =
	"iset channum\n"
	"\n"
	"Set the device and channel pair of the current input channel.\n"
	"All filters are updated to use the new channel setting as if the\n"
	"appropriate ``fchin'' function was called for each filter.\n";

char help_idel[] =
	"idel\n"
	"\n"
	"Delete current input channel.\n";

char help_iren[] =
	"iren newname\n"
	"\n"
	"Rename the current channel to the given name\n";

char help_ilist[] =
	"ilist\n"
	"\n"
	"Return list of inputs\n";

char help_iexists[] =
	"iexists channame\n"
	"\n"
	"Return 1 if an input channel with the name exists, 0 otherwise\n";

char help_igetc[] =
	"igetc\n"
	"\n"
	"Return the MIDI channel number of the current input channel\n";

char help_igetd[] =
	"igetd\n"
	"\n"
	"Return the device number of the current channel\n";

char help_iaddev[] =
	"iaddev event\n"
	"\n"
	"Add the given event to the configuration of the\n"
	"current channel, it's not used yet.\n";

char help_irmev[] =
	"irmev evspec\n"
	"\n"
	"Remove all events matching the given evspec from the configuration\n"
	"of the current channel.\n";

char help_iinfo[] =
	"iinfo\n"
	"\n"
	"Print all events on the config of the current input channel.\n";

char help_onew[] =
	"onew channame channum\n"
	"\n"
	"Create an new output with the given name and the given device\n"
	"and MIDI channel. Output channels contain a built-in filter\n"
	"having the same name; by defaut it maps all inputs to the\n"
	"newly created output channel.\n";

char help_odel[] =
	"odel\n"
	"\n"
	"Delete current output.\n";

char help_oset[] =
	"oset channum\n"
	"\n"
	"Set the device and MIDI channel numbers of the current channel.\n"
	"All filters are updated to use the new channel setting as if the\n"
	"appropriate ``fchout'' function was called for each filter.\n";

char help_oren[] =
	"oren newname\n"
	"\n"
	"Rename the current output to the given name.\n";

char help_oexists[] =
	"oexists channame\n"
	"\n"
	"Return 1 if an output with the given name exists, 0 otherwise.\n";

char help_ogetc[] =
	"ogetc\n"
	"\n"
	"Return the MIDI channel number of the current output.\n";

char help_ogetd[] =
	"ogetd\n"
	"\n"
	"Return the device number of the current output.\n";

char help_oaddev[] =
	"oaddev event\n"
	"\n"
	"Add the given event to the configuration of the current channel,\n"
	"it's not used yet.\n";

char help_ormev[] =
	"ormev evspec\n"
	"\n"
	"Remove all events matching the given evspec from the configuration\n"
	"of the current output.\n";

char help_oinfo[] =
	"oinfo\n"
	"\n"
	"Print all events of the config of the current output.\n";

char help_olist[] =
	"olist\n"
	"\n"
	"Return list of outputs\n";

char help_fnew[] =
	"fnew filtname\n"
	"\n"
	"Create an new with the given name\n";

char help_fdel[] =
	"fdel\n"
	"\n"
	"Delete the current filter.\n";

char help_fren[] =
	"fren newname\n"
	"\n"
	"Rename the current filter to the given name\n";

char help_fexists[] =
	"fexists filtname\n"
	"\n"
	"Return 1 if a filter with the given name exists, 0 otherwise\n";

char help_freset[] =
	"freset\n"
	"\n"
	"Remove all rules from the current filter.\n";

char help_finfo[] =
	"finfo\n"
	"\n"
	"List all fitering rules of the current filter\n";

char help_flist[] =
	"flist\n"
	"\n"
	"Return list of existing filters\n";

char help_fchgin[] =
	"fchgin from to\n"
	"\n"
	"Rewrite all current filter rules to consume ``new_evspec'' events\n"
	"instead of ``old_evspec'' events. This means that each rule that\n"
	"would consume ``old_evspec'' on the input will start\n"
	"consuming ``new_evspec'' instead.\n";

char help_fswapin[] =
	"fswapin from to\n"
	"\n"
	"Similar to ``fchgin'' but swap ``evspec1'' and ``evspec2'' in\n"
	"the source events set of each rule.\n";

char help_fchgout[] =
	"fchgout from to\n"
	"\n"
	"Rewrite all filtering rules of the current filter to produce\n"
	"``new_evspec'' events instead of ``old_evspec'' events. This means\n"
	"that each rule that would produce ``old_evspec'' on the output will\n"
	"start producing ``new_evspec'' instead.\n";

char help_fswapout[] =
	"fswapout from to\n"
	"\n"
	"Similar to ``fchgou'' but swap ``evspec1'' and ``evspec2'' in the\n"
	"destination events set of each rule.\n";

char help_fmap[] =
	"fmap from to\n"
	"\n"
	"Add a new rule to the current filter, to make it convert events\n"
	"matching evspec1 (source) into events matching evspec2 (destination).\n"
	"Both evspec1 and evspec2 must have the same number of devices,\n"
	"channels, notes, controllers etc..\n";

char help_funmap[] =
	"funmap from to\n"
	"\n"
	"Remove event maps from the current filter. Any\n"
	"mapping with source included in evspec1 and destination\n"
	"inluded in evspec2 is deleted.\n";

char help_ftransp[] =
	"ftransp evspec plus\n"
	"\n"
	"Transpose events generated by the filter and matching ``evspec''\n"
	"by the give number of halftones\n";

char help_fvcurve[] =
	"fvcurve evspec weight\n"
	"\n"
	"Adjust velocity of note events produced by the filter, using\n"
	"the given ``weight'' in the -63..63 range.  If ``weight'' is\n"
	"negative then sensitivity is decreased. If it's positive then\n"
	"sensitivity is increased. If it's zero the velocity is unchanged\n";

char help_xnew[] =
	"xnew sysexname\n"
	"\n"
	"Create a new bank of sysex messages with the given name\n";

char help_xdel[] =
	"xdel\n"
	"\n"
	"Delete the current bank of sysex messages.\n";

char help_xren[] =
	"xren newname\n"
	"\n"
	"Rename the current sysex bank to the given name\n";

char help_xexists[] =
	"xexists sysexname\n"
	"\n"
	"Return 1 if there's a sysex bank with the given name, 0 otherwise.\n";

char help_xrm[] =
	"xrm data\n"
	"\n"
	"Remove all sysex messages starting with the given pattern from the\n"
	"current sysex bank. The given pattern is a list of bytes; an empty\n"
	"pattern matches any sysex message.\n";

char help_xsetd[] =
	"xsetd devnum data\n"
	"\n"
	"Set device number to ``newdev'' on all sysex messages starting\n"
	"with ``pattern'' in the current sysex bank. The given pattern\n"
	"is a list of bytes; an empty pattern matches any sysex message.\n";

char help_xadd[] =
	"xadd devnum data\n"
	"\n"
	"Add to the current sysex bank a new sysex message. ``data'' is a list\n"
	"containing the MIDI system exclusive message and ``devname'' is the\n"
	"device number to which the message will be sent when performance mode\n"
	"is entered\n";

char help_xinfo[] =
	"xinfo\n"
	"\n"
	"Print all sysex messages of the current sysex bank. Messages that are\n"
	"too long to be desplayed on a single line are truncated and the\n"
	"``...'' string is displayed.\n";

char help_ximport[] =
	"ximport devnum path\n"
	"\n"
	"Replace contents of the current sysex bank by contents of the given\n"
	".syx file; messages are assigned to ``devnum'' device number.\n";

char help_xexport[] =
	"xexport path\n"
	"\n"
	"Store contents of the current sysex bank in the given .syx file\n";

char help_xlist[] =
	"xlist\n"
	"\n"
	"Return list of sysex banks.\n";

char help_i[] =
	"i\n"
	"\n"
	"Enter ``idle'' performance mode. Start processing MIDI input and\n"
	"generating MIDI output. Data passes through the current filter\n"
	"(if any) or through the current track's filter (if any).\n";

char help_p[] =
	"p\n"
	"\n"
	"Play the song from the current position. Input passes through the\n"
	"current filter (if any) or through the current track's filter\n"
	"(if any).\n";

char help_r[] =
	"r\n"
	"\n"
	"Play the song and record the input. Input passes through the current\n"
	"filter (if any) or through the current track's filter (if any).\n"
	"On startup,this function play one measure of countdown before the data\n"
	"start being recorded.\n";

char help_s[] =
	"s\n"
	"\n"
	"Stop performance and release MIDI devices. I.e. stop the effect ``i'',\n"
	"``p'', and ``r''.\n";

char help_ev[] =
	"ev evspec\n"
	"\n"
	"Set the current event selection. Most track editing functions will\n"
	"act only on events matching evspec ignoring all other events.\n";

char help_setq[] =
	"setq step\n"
	"\n"
	"Set the current quantization step to the given note value, as follows:\n"
	"   4 -> quarter note\n"
	"   6 -> quarter note triplet\n"
	"   8 -> eighth note\n"
	"  12 -> eighth note triplet\n"
	"  etc...\n"
	"The quantization step will be used by tquanta and tquantf funnctions\n"
	"to optimize event selection. If the special ``nil'' value is specified\n"
	"as quantization step, then quatization is disabled.\n";

char help_getq[] =
	"getq\n"
	"\n"
	"Return the current quatization step\n";

char help_g[] =
	"g measure\n"
	"\n"
	"Set the current song position pointer to the given measure number.\n"
	"Record and playback will start a that position. This also defines\n"
	"the beginning of the current selection used by most track editing\n"
	"functions.\n";

char help_getpos[] =
	"getpos\n"
	"\n"
	"Return the current song position pointer which is also the start\n"
	"position of the current selection.\n";

char help_sel[] =
	"sel length\n"
	"\n"
	"Set the length of the current selection to ``length'' measures. The\n"
	"current selection start at the current position set with the ``g''\n"
	"function\n";

char help_getlen[] =
	"getlen\n"
	"\n"
	"Return the length (in measures) of the current selection.\n";

char help_ct[] =
	"ct trackname\n"
	"\n"
	"Set the current track. The current track\n"
	"is the one that will be recorded. Most track editing\n"
	"functions act on it.\n";

char help_gett[] =
	"gett\n"
	"\n"
	"Return the current track (if any) or ``nil''\n";

char help_cf[] =
	"cf filtname\n"
	"\n"
	"Set the current filter to ``filtname''. The current filter\n"
	"is the one used to process input MIDI events in performance mode.\n"
	"It's also the one affected by all filter editing functions.\n";

char help_getf[] =
	"getf\n"
	"\n"
	"Return the current filter or ``nil'' if none.\n";

char help_cx[] =
	"cx sysexname\n"
	"\n"
	"Set the current sysex bank, i.e. the one that will be recorded.\n"
	"The current sysex back is the one affected by all sysex editing\n"
	"functions.\n";

char help_getx[] =
	"getx\n"
	"\n"
	"Return the current sysex bank or ``nil'' if none\n";

char help_ci[] =
	"ci channame\n"
	"\n"
	"Set the current (named) input channel. All input channel\n"
	"editing functions will act on it.\n";

char help_geti[] =
	"geti\n"
	"\n"
	"Return the name of the current input channel or ``nil'' if none\n";

char help_co[] =
	"co channame\n"
	"\n"
	"Set the current (named) output channel. All output channel\n"
	"editing functions will act on it.\n";

char help_geto[] =
	"geto\n"
	"\n"
	"Return the name of the current output channel or ``nil'' if none\n";

char help_setunit[] =
	"setunit tics_per_unit\n"
	"\n"
	"Set the time resolution of the sequencer to ``tpu'' ticks in a\n"
	"whole note (1 unit note = 4 quarter notes). The default is 96 ticks,\n"
	"which is the default of the MIDI standard.\n";

char help_getunit[] =
	"getunit\n"
	"\n"
	"Return the number of ticks in a whole note\n";

char help_fac[] =
	"fac tempo_factor\n"
	"\n"
	"Set the tempo factor for play and record to the given integer\n"
	"value. The tempo factor must be between 50 (play half of the\n"
	"real tempo) and 200 (play at twice the real tempo).\n";

char help_getfac[] =
	"getfac\n"
	"\n"
	"Return the current tempo factor.\n";

char help_t[] =
	"t beats_per_minute\n"
	"\n"
	"Set the tempo at the current song position.\n";

char help_mins[] =
	"mins amount sig\n"
	"\n"
	"Insert ``amount'' blank measures at the current song position.\n"
	"The time signature used is num/denom. If the time signature is\n"
	"an empty list (i.e. ``{}'') then the time signature at the\n"
	"current position is used.\n";

char help_mcut[] =
	"mcut\n"
	"\n"
	"Cut the current selection of all tracks, including the\n"
	"time structure.\n";

char help_mdup[] =
	"mdup where\n"
	"\n"
	"Duplicate the current selection inserting a copy of it at\n"
	"the position given by the ``where'' parameter. The target\n"
	"position is a measure number relative to the current selection\n"
	"to be copied. If ``where'' is positive it's relative to the end\n"
	"of the current selection; if it's negative it's relative to\n"
	"the beginning of the current selection.\n";

char help_minfo[] =
	"minfo\n"
	"\n"
	"Print the meta-track (tempo changes, time signature changes.\n";

char help_mtempo[] =
	"mtempo\n"
	"\n"
	"Return the tempo at the current song position. The unit is beats\n"
	"per minute.\n";

char help_msig[] =
	"msig\n"
	"\n"
	"Return the time signature at the current song position. The result\n"
	"is a two number list: numerator and denominator.\n";

char help_mend[] =
	"mend\n"
	"\n"
	"Return the ending measure of the song (i.e. its size in measures).\n";

char help_ls[] =
	"ls\n"
	"\n"
	"List all tracks, channels, filters and various default values\n";

char help_save[] =
	"save filename\n"
	"\n"
	"Save the song into the given file. The ``filename'' is a\n"
	"quoted string.\n";

char help_load[] =
	"load filename\n"
	"\n"
	"Load the song from a file named ``filename''. the current song is\n"
	"destroyed, even if the load command fails.\n";

char help_reset[] =
	"reset\n"
	"\n"
	"Destroy completely the song, useful to start a new song without\n"
	"restarting the program\n";

char help_export[] =
	"export filename\n"
	"\n"
	"Save the song into a standard MIDI file, ``filename'' is a quoted\n"
	"string.\n";

char help_import[] =
	"import filename\n"
	"\n"
	"Load the song from a standard MIDI file, ``filename'' is a quoted\n"
	"string. Only MIDI file ``type 1'' and ``type 0'' are supported.\n";

char help_u[] =
	"u\n"
	"\n"
	"Undo last operation saved for undo.\n";

char help_ul[] =
	"ul\n"
	"\n"
	"List operations saved for undo.\n";

char help_dlist[] =
	"dlist\n"
	"\n"
	"Return the list of attached devices (list of numbers)\n";

char help_dnew[] =
	"dnew devnum path mode\n"
	"\n"
	"Attach MIDI device ``filename'' as device number ``devnum'';\n"
	"``filename'' is a quoted string. The ``mode'' argument is the\n"
	"name of the mode, it can be on if the following:\n"
	"    ro - read-only, for input only devices\n"
	"    wo - write-only, for output only devices\n"
	"    rw - read and write.\n"
	"If midish is configured to use ALSA (default on Linux systems)\n"
	"then ``filename'' should contain the ALSA sequencer port, as\n"
	"listed by ``aseqdump -l'', (eg. ``28:0'', ``FLUID Synth (qsynth)'').\n"
	"If ``nil'' is given instead of the path, then the port is not\n"
	"connected to any existing port; this allows other ALSA sequencer\n"
	"clients to subscribe to it and to provide events to midish or to\n"
	"consume events midish sends to it.\n";

char help_ddel[] =
	"ddel devnum\n"
	"\n"
	"Detach device number ``devnum''\n";

char help_dmtcrx[] =
	"dmtcrx devnum\n"
	"\n"
	"Use device number ``devnum'' as MTC source.\n"
	"In this case, midish will relocate, start and stop according to\n"
	"incoming MTC messages. Midish will generate its clock ticks from\n"
	"MTC, meaning that it will run at the same speed as the MTC device.\n"
	"This is useful to synchronize midish to an audio multi-tracker or any\n"
	"MTC capable audio application. If ``devnum'' is ``nil'', then MTC\n"
	"messages are ignored and the internal timer will be used instead.\n";

char help_dmmctx[] =
	"dmmctx devlist\n"
	"\n"
	"Configure the given devices to transmit MMC start, stop\n"
	"and relocate messages. Useful to control MMC-capable audio\n"
	"applications from midish. By default, devices transmit MMC.\n";

char help_dclktx[] =
	"dclktx devlist\n"
	"\n"
	"Configure the given devices to transmit MIDI clock information\n"
	"(MIDI ticks, MIDI start and MIDI stop events). Useful to\n"
	"synchronize an external sequencer to midish.\n";

char help_dclkrx[] =
	"dclkrx devnum\n"
	"\n"
	"Set device number ``devnum'' to be the master MIDI clock source.\n"
	"It will give midish MIDI ticks, MIDI start and MIDI stop events. This\n"
	"useful to synchronize midish to an external sequencer. If ``devnum'' is\n"
	"``nil'', then the internal clock will be used and midish will act as\n"
	"master device.\n";

char help_dclkrate[] =
	"dclkrate devnum tics_per_unit\n"
	"\n"
	"Set the number of ticks in a whole note that are transmitted to the\n"
	"MIDI device. Default value is 96 ticks. This is the standard MIDI\n"
	"value and its not recommended to change it.\n";

char help_dinfo[] =
	"dinfo devnum\n"
	"\n"
	"Print some information about the MIDI device.\n";

char help_dixctl[] =
	"dixctl devnum ctlset\n"
	"\n"
	"Setup the list of controllers that are expected to be received\n"
	"as 14-bit numbers (i.e. both coarse and fine MIDI controller messages\n"
	"will be expected). By default only coarse values are used, if unsure\n"
	"let this list empty.\n";

char help_doxctl[] =
	"doxctl devnum ctlset\n"
	"\n"
	"Setup the list of controllers that will be transmitted\n"
	"as 14-bit numbers (both coarse and fine MIDI controller messages).\n";

char help_diev[] =
	"diev devnum flags\n"
	"\n"
	"Configure the device to process as a single event the following\n"
	"patterns of input MIDI messages.\n"
	"    xpc: group bank select controllers (0 and 32) with program\n"
	"         changes into a signle ``xpc'' event.\n"
	"   nrpn: group NRPN controllers (98 and 99) with data entry\n"
	"         controllers (6 and 38) into a single ``nrpn'' event.\n"
	"    rpn: same as ``nrpn'', but for RPN controllers (100 and 101).\n"
	"By default all of the above are enabled, which allows banks, NRPNs\n"
	"and RPNs to be handled by midish the standard way.\n"
	"It makes sense to disable grouping of above messages on rare hardware\n"
	"that maps above-mentioned controller numbers (0, 6, 32, 38, 98, 99,\n"
	"100, 101) to other parameters than bank number and NRPN/RPN.\n";

char help_doev[] =
	"doev devnum flags\n"
	"\n"
	"Same as ``diev'', but for output messages\n";

char help_ctlconf[] =
	"ctlconf name ctl defval\n"
	"\n"
	"Configure controller number ``ctlnumber'' with name ``ctlname'',\n"
	"and default value ``defval''. If defval is ``nil'' then there is\n"
	"no default value and corresponding controller events are not grouped\n"
	"into frames.\n";

char help_ctlconfx[] =
	"ctlconfx name ctl defval\n"
	"\n"
	"Same as ctlconf, but for 14-bit controllers. Thus defval is in the\n"
	"range 0..16383.\n";

char help_ctlunconf[] =
	"ctlunconf name\n"
	"\n"
	"Unconfigure the given controller. ``ctlname'' is the identifier that\n"
	"was used with ctlconf or ctlconfx\n";

char help_ctlinfo[] =
	"ctlinfo\n"
	"\n"
	"Print the list of configured controllers\n";

char help_evpat[] =
	"evpat name pattern\n"
	"\n"
	"Define a new event type corresponding to the given system exclusive\n"
	"message pattern. The pattern is a list of bytes or event parameter\n"
	"identifiers (aka atoms).  The following atoms are supported: v0, v0_lo,\n"
	"v0_hi, v1, v1_lo, v1_hi. They correspond to the full 7-bit value (coarse\n"
	"parameter), the low 7-bit nibble and the high 7-bit nibble (fine grained\n"
	"parameters) of the first and second parameters respectively. Example:\n"
	"\n"
	"\tevpat master {0xf0 0x7f 0x7f 0x04 0x01 v0_lo v0_hi 0xf7}\n"
	"\n"
	"defines a new event type for the standard master volume system exclusive\n"
	"message.\n";

char help_evinfo[] =
	"evinfo\n"
	"\n"
	"Print the list of event patterns.\n";

char help_m[] =
	"m onoff\n"
	"\n"
	"Set the mode of the metronome. The following modes are available:\n"
	"    on - turned on for both playback and record\n"
	"    rec - turned on for record only\n"
	"    off - turned off\n";

char help_metrocf[] =
	"metrocf eventhi eventlo\n"
	"\n"
	"Select the notes that the metronome plays. The pair of events must\n"
	"be note-ons\n";

char help_tap[] =
	"tap mode\n"
	"\n"
	"Set the way start is triggered by events. The following modes are\n"
	"available:\n"
	"    off - no events trigger start\n"
	"    start - a single event triggers start\n"
	"    tempo - two events trigger start; the time between them\n"
	"        corresponds to one beat and is used to determine the\n"
	"        initial tempo.\n";

char help_tapev[] =
	"tapev evspec\n"
	"\n"
	"Events set used to trigger start when tap mode is ``start'' or\n"
	"``tempo''.\n";

char help_info[] =
	"info\n"
	"\n"
	"Display the list of built-in and user-defined procedures and global\n"
	"variables\n";

char help_print[] =
	"print value\n"
	"\n"
	"Display the value of the expression \n";

char help_err[] =
	"err message\n"
	"\n"
	"Display the given string and abort the statement being executed.\n";

char help_h[] =
	"h function\n"
	"\n"
	"Display list of arguments function ``funcname''.\n";

char help_exec[] =
	"exec filename\n"
	"\n"
	"Read and executes the script from a file, ``filename'' is a quoted\n"
	"string. The execution of the script is aborted on error. If the script\n"
	"executes an exit statement, only the script is terminated.\n";

char help_debug[] =
	"debug flag value\n"
	"\n"
	"Set debug-flag ``flag'' to (integer) value ``val''. It's a developer knob.\n"
	"If ``val=0'' the corresponding debug-info are turned off. ``flag'' can be:\n"
	"    filt - show events passing through the current filter\n"
	"    mididev - show raw MIDI traffic\n"
	"    mixout - show conflicts in the output MIDI merger\n"
	"    norm - show events in the input normalizer\n"
	"    pool - show pool usage on exit\n"
	"    song - show start/stop events\n"
	"    timo - show timer internal errors\n"
	"    mem - show memory usage\n";

char help_version[] =
	"version\n"
	"\n"
	"Display midish version.\n";

char help_panic[] =
	"panic\n"
	"\n"
	"Cause the sequencer to core-dump, useful to developpers.\n";

char help_shut[] =
	"shut\n"
	"\n"
	"Stop all sounds\n";

char help_proclist[] =
	"proclist\n"
	"\n"
	"Return the list of all user defined procs.\n";

char help_builtinlist[] =
	"builtinlist\n"
	"\n"
	"Return a list of all builtin commands.\n";

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
