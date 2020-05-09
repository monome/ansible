#include <stdlib.h>

#include "monome.h"
#include "init_common.h"
#include "main.h"
#include "print_funcs.h"

#include "ansible_preset_docdef.h"
#include "gitversion.h"

#define ALLOC_DEBUG 1

// WARNING: order must match definition order of
// connected_t, ansible_mode_t in main.h
const char* connected_t_options[] = {
	"conNONE",
	"conARC",
	"conGRID",
	"conMIDI",
	"conFLASH",
};
const char* ansible_mode_options[] = {
	"mArcLevels",
	"mArcCycles",
	"mGridKria",
	"mGridMP",
	"mGridES",
	"mMidiStandard",
	"mMidiArp",
	"mTT",
};
// END WARNING

json_read_object_state_t ansible_root_object_state;
json_read_object_state_t ansible_section_object_state;
json_read_buffer_state_t ansible_json_read_buffer_state;
json_read_object_state_t ansible_app_object_state[4];
json_read_array_state_t ansible_json_read_array_state[4];

DECLARE_STATIC_ALLOC(kria_data_t, k)
DECLARE_STATIC_ALLOC(mp_data_t, m)
DECLARE_STATIC_ALLOC(es_data_t, e)
DECLARE_STATIC_ALLOC(cycles_data_t, l)
DECLARE_STATIC_ALLOC(levels_data_t, l)

json_docdef_t ansible_meta_docdefs[] = {
	{
		.name = "firmware",
		.read = json_match_string,
		.write = json_write_constant_string,
		.params = &((json_match_string_params_t) {
			.to_match = ANSIBLE_FIRMWARE_NAME,
		}),
	},
	{
		.name = "version",
		.read = json_match_string,
		.write = json_write_constant_string,
		.params = &((json_match_string_params_t) {
			.to_match = git_version,
			.skip = true,
		}),
	},
	{
		.name = "grid_varibrightness",
		.read = json_read_scalar,
		.write = json_write_number,
		.params = &((json_read_scalar_params_t) {
			.dst_offset = offsetof(nvram_data_t, state.grid_varibrightness),
			.dst_size = sizeof_field(nvram_data_t, state.grid_varibrightness),
		}),
	},
	{
		.name = "i2c_addr",
		.read = json_read_scalar,
		.write = json_write_number,
		.params = &((json_read_scalar_params_t){
			.dst_offset = offsetof(nvram_data_t, state.i2c_addr),
			.dst_size = sizeof_field(nvram_data_t, state.i2c_addr),
		}),
	},
	{
		.name = "followers",
		.read = json_read_array,
		.write = json_write_array,
		.state = &ansible_json_read_array_state[0],
		.params = &((json_read_array_params_t) {
			.array_len = sizeof_field(nvram_data_t, state.followers) / sizeof_field(nvram_data_t, state.followers[0]),
			.item_size = sizeof_field(nvram_data_t, state.followers[0]),
			.item_docdef = &((json_docdef_t) {
				.read = json_read_object,
				.write = json_write_object,
				.state = &ansible_app_object_state[0],
				.params = &((json_read_object_params_t) {
					.docdef_ct = 4,
					.docdefs = ((json_docdef_t[]) {
						{
							.name = "active",
							.read = json_read_scalar,
							.write = json_write_bool,
							.params = &((json_read_scalar_params_t) {
								.dst_size = sizeof_field(nvram_data_t, state.followers[0].active),
								.dst_offset = offsetof(nvram_data_t, state.followers[0].active),
							}),
						},
						{
							.name = "track_en",
							.read = json_read_scalar,
							.write = json_write_number,
							.params = &((json_read_scalar_params_t) {
								.dst_size = sizeof_field(nvram_data_t, state.followers[0].track_en),
								.dst_offset = offsetof(nvram_data_t, state.followers[0].track_en),
							}),
						},
						{
							.name = "oct",
							.read = json_read_scalar,
							.write = json_write_number,
							.params = &((json_read_scalar_params_t) {
								.dst_size = sizeof_field(nvram_data_t, state.followers[0].oct),
								.dst_offset = offsetof(nvram_data_t, state.followers[0].oct),
							}),
						},
						{
							.name = "active_mode",
							.read = json_read_scalar,
							.write = json_write_number,
							.params = &((json_read_scalar_params_t) {
								.dst_size = sizeof_field(nvram_data_t, state.followers[0].active_mode),
								.dst_offset = offsetof(nvram_data_t, state.followers[0].active_mode),
							}),
						},
					}),
				}),
			}),
		}),
	},
	{
		.name = "connected",
		.read = json_read_enum,
		.write = json_write_enum,
		.params = &((json_read_enum_params_t) {
			.option_ct = sizeof(connected_t_options) / sizeof(connected_t_options[0]),
			.options = connected_t_options,
			.dst_offset = offsetof(nvram_data_t, state.connected),
			.default_val = (int)conNONE,
		}),
	},
	{
		.name = "arc_mode",
		.read = json_read_enum,
		.write = json_write_enum,
		.params = &((json_read_enum_params_t) {
			.option_ct = sizeof(ansible_mode_options) / sizeof(ansible_mode_options[0]),
			.options = ansible_mode_options,
			.dst_offset = offsetof(nvram_data_t, state.arc_mode),
			.default_val = (int)mArcLevels,
		}),
	},
	{
		.name = "grid_mode",
		.read = json_read_enum,
		.write = json_write_enum,
		.params = &((json_read_enum_params_t) {
			.option_ct = sizeof(ansible_mode_options) / sizeof(ansible_mode_options[0]),
			.options = ansible_mode_options,
			.dst_offset = offsetof(nvram_data_t, state.grid_mode),
			.default_val = (int)mGridKria,
		}),
	},
	{
		.name = "midi_mode",
		.read = json_read_enum,
		.write = json_write_enum,
		.params = &((json_read_enum_params_t) {
			.option_ct = sizeof(ansible_mode_options) / sizeof(ansible_mode_options[0]),
			.options = ansible_mode_options,
			.dst_offset = offsetof(nvram_data_t, state.midi_mode),
			.default_val = (int)mMidiStandard,
		}),
	},
	{
		.name = "none_mode",
		.read = json_read_enum,
		.write = json_write_enum,
		.params = &((json_read_enum_params_t) {
			.option_ct = sizeof(ansible_mode_options) / sizeof(ansible_mode_options[0]),
			.options = ansible_mode_options,
			.dst_offset = offsetof(nvram_data_t, state.none_mode),
			.default_val = (int)mTT,
		}),
	},
};

json_docdef_t ansible_shared_docdefs[] = {
	{
		.name = "scales",
		.read = json_read_array,
		.write = json_write_array,
		.state = &ansible_json_read_array_state,
		.params = &((json_read_array_params_t) {
			.array_len = sizeof_field(nvram_data_t, scale) / sizeof_field(nvram_data_t, scale[0]),
			.item_size = sizeof_field(nvram_data_t, scale[0]),
			.item_docdef = &((json_docdef_t) {
				.read = json_read_buffer,
				.write = json_write_buffer,
				.state = &ansible_json_read_buffer_state,
				.params = &((json_read_buffer_params_t) {
					.dst_size = sizeof_field(nvram_data_t, scale[0]),
					.dst_offset = offsetof(nvram_data_t, scale),
				}),
			})
		})
	},
	{
		.name = "tuning_table",
		.read = json_read_array,
		.write = json_write_array,
		.state = &ansible_json_read_array_state,
		.params = &((json_read_array_params_t) {
			.array_len = sizeof_field(nvram_data_t, tuning_table) / sizeof_field(nvram_data_t, tuning_table[0]),
			.item_size = sizeof_field(nvram_data_t, tuning_table[0]),
			.item_docdef = &((json_docdef_t) {
				.read = json_read_buffer,
				.write = json_write_buffer,
				.state = &ansible_json_read_buffer_state,
				.params = &((json_read_buffer_params_t) {
					.dst_size = sizeof_field(nvram_data_t, tuning_table[0]),
					.dst_offset = offsetof(nvram_data_t, tuning_table),
				}),
			})
		})
	},
};

/////////
// apps

json_docdef_t ansible_app_docdefs[] = {
	{
		.name = "kria",
		.read = json_read_object,
		.write = json_write_object,
		.state = &ansible_app_object_state[0],
		.params = &((json_read_object_params_t) {
			.docdef_ct = 11,
			.docdefs = ((json_docdef_t[]) {
				{
					.name = "clock_period",
					.read = json_read_scalar,
					.write = json_write_number,
					.params = &((json_read_scalar_params_t) {
						.dst_size = sizeof_field(nvram_data_t, kria_state.clock_period),
						.dst_offset = offsetof(nvram_data_t, kria_state.clock_period),
					}),
				},
				{
					.name = "curr_preset",
					.read = json_read_scalar,
					.write = json_write_number,
					.params = &((json_read_scalar_params_t) {
						.dst_size = sizeof_field(nvram_data_t, kria_state.preset),
						.dst_offset = offsetof(nvram_data_t, kria_state.preset),
					}),
				},
				{
					.name = "sync_mode",
					.read = json_read_scalar,
					.write = json_write_number,
					.params = &((json_read_scalar_params_t) {
						.dst_size = sizeof_field(nvram_data_t, kria_state.sync_mode),
						.dst_offset = offsetof(nvram_data_t, kria_state.sync_mode),
					}),
				},
				{
					.name = "note_sync",
					.read = json_read_scalar,
					.write = json_write_bool,
					.params = &((json_read_scalar_params_t) {
						.dst_size = sizeof_field(nvram_data_t, kria_state.note_sync),
						.dst_offset = offsetof(nvram_data_t, kria_state.note_sync),
					}),
				},
				{
					.name = "loop_sync",
					.read = json_read_scalar,
					.write = json_write_number,
					.params = &((json_read_scalar_params_t) {
						.dst_size = sizeof_field(nvram_data_t, kria_state.loop_sync),
						.dst_offset = offsetof(nvram_data_t, kria_state.loop_sync),
					}),
				},
				{
					.name = "note_div_sync",
					.read = json_read_scalar,
					.write = json_write_number,
					.params = &((json_read_scalar_params_t) {
						.dst_size = sizeof_field(nvram_data_t, kria_state.note_div_sync),
						.dst_offset = offsetof(nvram_data_t, kria_state.note_div_sync),
					}),
				},
				{
					.name = "div_sync",
					.read = json_read_scalar,
					.write = json_write_number,
					.params = &((json_read_scalar_params_t) {
						.dst_size = sizeof_field(nvram_data_t, kria_state.div_sync),
						.dst_offset = offsetof(nvram_data_t, kria_state.div_sync),
					}),
				},
				{
					.name = "cue_div",
					.read = json_read_scalar,
					.write = json_write_number,
					.params = &((json_read_scalar_params_t) {
						.dst_size = sizeof_field(nvram_data_t, kria_state.cue_div),
						.dst_offset = offsetof(nvram_data_t, kria_state.cue_div),
					}),
				},
				{
					.name = "cue_steps",
					.read = json_read_scalar,
					.write = json_write_number,
					.params = &((json_read_scalar_params_t) {
						.dst_size = sizeof_field(nvram_data_t, kria_state.cue_steps),
						.dst_offset = offsetof(nvram_data_t, kria_state.cue_steps),
					}),
				},
				{
					.name = "meta",
					.read = json_read_scalar,
					.write = json_write_number,
					.params = &((json_read_scalar_params_t) {
						.dst_size = sizeof_field(nvram_data_t, kria_state.meta),
						.dst_offset = offsetof(nvram_data_t, kria_state.meta),
					}),
				},
				{
					.name = "presets",
					.read = json_read_array,
					.write = json_write_array,
					.state = &ansible_json_read_array_state[0],
					.params = &((json_read_array_params_t) {
						.array_len = sizeof_field(nvram_data_t, kria_state.k) / sizeof_field(nvram_data_t, kria_state.k[0]),
						.item_size = sizeof_field(nvram_data_t, kria_state.k[0]),
						.item_docdef = &((json_docdef_t) {
							.read = json_read_object_cached,
							.write = json_write_object,
							.state = &ansible_app_object_state[1],
							.params = &((json_read_object_params_t) {
								.docdef_ct = 9,
								.dst_size = sizeof(kria_data_t),
								.dst_offset = offsetof(nvram_data_t, kria_state.k[0]),
								.alloc = STATIC_ALLOC(kria_data_t, k),
								.free = nop_free,
								.docdefs = ((json_docdef_t[]) {
									{
										.name = "patterns",
										.read = json_read_array,
										.write = json_write_array,
										.state = &ansible_json_read_array_state[1],
										.params = &((json_read_array_params_t) {
											.array_len = sizeof_field(nvram_data_t, kria_state.k[0].p) / sizeof_field(nvram_data_t, kria_state.k[0].p[0]),
											.item_size = sizeof_field(nvram_data_t, kria_state.k[0].p[0]),
											.item_docdef = &((json_docdef_t) {
												.read = json_read_object,
												.write = json_write_object,
												.state = &ansible_app_object_state[2],
												.params = &((json_read_object_params_t) {
													.docdef_ct = 2,
													.docdefs = ((json_docdef_t[]) {
														{
															.name = "tracks",
															.read = json_read_array,
															.write = json_write_array,
															.state = &ansible_json_read_array_state[2],
															.params = &((json_read_array_params_t) {
																.array_len = sizeof_field(nvram_data_t, kria_state.k[0].p[0].t) / sizeof_field(nvram_data_t, kria_state.k[0].p[0].t[0]),
																.item_size = sizeof_field(nvram_data_t, kria_state.k[0].p[0].t[0]),
																.item_docdef = &((json_docdef_t) {
																	.read = json_read_object,
																	.write = json_write_object,
																	.state = &ansible_app_object_state[3],
																	.params = &((json_read_object_params_t) {
																		.docdef_ct = 20,
																		.docdefs = ((json_docdef_t[]) {
																			{
																				.name = "tr",
																				.read = json_read_buffer,
																				.write = json_write_buffer,
																				.state = &ansible_json_read_buffer_state,
																				.params = &((json_read_buffer_params_t) {
																					.dst_size = sizeof_field(nvram_data_t, kria_state.k[0].p[0].t[0].tr),
																					.dst_offset = offsetof(nvram_data_t, kria_state.k[0].p[0].t[0].tr),
																				}),
																			},
																			{
																				.name = "oct",
																				.read = json_read_buffer,
																				.write = json_write_buffer,
																				.state = &ansible_json_read_buffer_state,
																				.params = &((json_read_buffer_params_t) {
																					.dst_size = sizeof_field(nvram_data_t, kria_state.k[0].p[0].t[0].oct),
																					.dst_offset = offsetof(nvram_data_t, kria_state.k[0].p[0].t[0].oct),
																				}),
																			},
																			{
																				.name = "note",
																				.read = json_read_buffer,
																				.write = json_write_buffer,
																				.state = &ansible_json_read_buffer_state,
																				.params = &((json_read_buffer_params_t) {
																					.dst_size = sizeof_field(nvram_data_t, kria_state.k[0].p[0].t[0].note),
																					.dst_offset = offsetof(nvram_data_t, kria_state.k[0].p[0].t[0].note),
																				}),
																			},
																			{
																				.name = "dur",
																				.read = json_read_buffer,
																				.write = json_write_buffer,
																				.state = &ansible_json_read_buffer_state,
																				.params = &((json_read_buffer_params_t) {
																					.dst_size = sizeof_field(nvram_data_t, kria_state.k[0].p[0].t[0].dur),
																					.dst_offset = offsetof(nvram_data_t, kria_state.k[0].p[0].t[0].dur),
																				}),
																			},
																			{
																				.name = "rpt",
																				.read = json_read_buffer,
																				.write = json_write_buffer,
																				.state = &ansible_json_read_buffer_state,
																				.params = &((json_read_buffer_params_t) {
																					.dst_size = sizeof_field(nvram_data_t, kria_state.k[0].p[0].t[0].rpt),
																					.dst_offset = offsetof(nvram_data_t, kria_state.k[0].p[0].t[0].rpt),
																				}),
																			},
																			{
																				.name = "rptBits",
																				.read = json_read_buffer,
																				.write = json_write_buffer,
																				.state = &ansible_json_read_buffer_state,
																				.params = &((json_read_buffer_params_t) {
																					.dst_size = sizeof_field(nvram_data_t, kria_state.k[0].p[0].t[0].rptBits),
																					.dst_offset = offsetof(nvram_data_t, kria_state.k[0].p[0].t[0].rptBits),
																				}),
																			},
																			{
																				.name = "alt_note",
																				.read = json_read_buffer,
																				.write = json_write_buffer,
																				.state = &ansible_json_read_buffer_state,
																				.params = &((json_read_buffer_params_t) {
																					.dst_size = sizeof_field(nvram_data_t, kria_state.k[0].p[0].t[0].alt_note),
																					.dst_offset = offsetof(nvram_data_t, kria_state.k[0].p[0].t[0].alt_note),
																				}),
																			},
																			{
																				.name = "glide",
																				.read = json_read_buffer,
																				.write = json_write_buffer,
																				.state = &ansible_json_read_buffer_state,
																				.params = &((json_read_buffer_params_t) {
																					.dst_size = sizeof_field(nvram_data_t, kria_state.k[0].p[0].t[0].glide),
																					.dst_offset = offsetof(nvram_data_t, kria_state.k[0].p[0].t[0].glide),
																				}),
																			},
																			{
																				.name = "p",
																				.read = json_read_array,
																				.write = json_write_array,
																				.state = &ansible_json_read_array_state[3],
																				.params = &((json_read_array_params_t) {
																					.array_len = sizeof_field(nvram_data_t, kria_state.k[0].p[0].t[0].p) / sizeof_field(nvram_data_t, kria_state.k[0].p[0].t[0].p[0]),
																					.item_size = sizeof_field(nvram_data_t, kria_state.k[0].p[0].t[0].p[0]),
																					.item_docdef = &((json_docdef_t) {
																						.read = json_read_buffer,
																						.write = json_write_buffer,
																						.state = &ansible_json_read_buffer_state,
																						.params = &((json_read_buffer_params_t) {
																							.dst_size = sizeof_field(nvram_data_t, kria_state.k[0].p[0].t[0].p[0]),
																							.dst_offset = offsetof(nvram_data_t, kria_state.k[0].p[0].t[0].p[0]),
																						}),
																					}),
																				}),
																			},
																			{
																				.name = "dur_mul",
																				.read = json_read_scalar,
																				.write = json_write_number,
																				.params = &((json_read_scalar_params_t) {
																					.dst_size = sizeof_field(nvram_data_t, kria_state.k[0].p[0].t[0].dur_mul),
																					.dst_offset = offsetof(nvram_data_t, kria_state.k[0].p[0].t[0].dur_mul),
																				}),
																			},
																			{
																				.name = "direction",
																				.read = json_read_scalar,
																				.write = json_write_number,
																				.params = &((json_read_scalar_params_t) {
																					.dst_size = sizeof_field(nvram_data_t, kria_state.k[0].p[0].t[0].direction),
																					.dst_offset = offsetof(nvram_data_t, kria_state.k[0].p[0].t[0].direction),
																				}),
																			},
																			{
																				.name = "advancing",
																				.read = json_read_buffer,
																				.write = json_write_buffer,
																				.state = &ansible_json_read_buffer_state,
																				.params = &((json_read_buffer_params_t) {
																					.dst_size = sizeof_field(nvram_data_t, kria_state.k[0].p[0].t[0].advancing),
																					.dst_offset = offsetof(nvram_data_t, kria_state.k[0].p[0].t[0].advancing),
																				}),
																			},
																			{
																				.name = "octshift",
																				.read = json_read_scalar,
																				.write = json_write_number,
																				.params = &((json_read_scalar_params_t) {
																					.dst_offset = offsetof(nvram_data_t, kria_state.k[0].p[0].t[0].octshift),
																					.dst_size = sizeof_field(nvram_data_t, kria_state.k[0].p[0].t[0].octshift),
																					.signed_val = true,
																				}),
																			},
																			{
																				.name = "lstart",
																				.read = json_read_buffer,
																				.write = json_write_buffer,
																				.state = &ansible_json_read_buffer_state,
																				.params = &((json_read_buffer_params_t) {
																					.dst_size = sizeof_field(nvram_data_t, kria_state.k[0].p[0].t[0].lstart),
																					.dst_offset = offsetof(nvram_data_t, kria_state.k[0].p[0].t[0].lstart),
																				}),
																			},
																			{
																				.name = "lend",
																				.read = json_read_buffer,
																				.write = json_write_buffer,
																				.state = &ansible_json_read_buffer_state,
																				.params = &((json_read_buffer_params_t) {
																					.dst_size = sizeof_field(nvram_data_t, kria_state.k[0].p[0].t[0].lend),
																					.dst_offset = offsetof(nvram_data_t, kria_state.k[0].p[0].t[0].lend),
																				}),
																			},
																			{
																				.name = "llen",
																				.read = json_read_buffer,
																				.write = json_write_buffer,
																				.state = &ansible_json_read_buffer_state,
																				.params = &((json_read_buffer_params_t) {
																					.dst_size = sizeof_field(nvram_data_t, kria_state.k[0].p[0].t[0].llen),
																					.dst_offset = offsetof(nvram_data_t, kria_state.k[0].p[0].t[0].llen),
																				}),
																			},
																			{
																				.name = "lswap",
																				.read = json_read_buffer,
																				.write = json_write_buffer,
																				.state = &ansible_json_read_buffer_state,
																				.params = &((json_read_buffer_params_t) {
																					.dst_size = sizeof_field(nvram_data_t, kria_state.k[0].p[0].t[0].lswap),
																					.dst_offset = offsetof(nvram_data_t, kria_state.k[0].p[0].t[0].lswap),
																				}),
																			},
																			{
																				.name = "tmul",
																				.read = json_read_buffer,
																				.write = json_write_buffer,
																				.state = &ansible_json_read_buffer_state,
																				.params = &((json_read_buffer_params_t) {
																					.dst_size = sizeof_field(nvram_data_t, kria_state.k[0].p[0].t[0].tmul),
																					.dst_offset = offsetof(nvram_data_t, kria_state.k[0].p[0].t[0].tmul),
																				}),
																			},
																			{
																				.name = "tt_clocked",
																				.read = json_read_scalar,
																				.write = json_write_bool,
																				.params = &((json_read_scalar_params_t) {
																					.dst_size = sizeof_field(nvram_data_t, kria_state.k[0].p[0].t[0].tt_clocked),
																					.dst_offset = offsetof(nvram_data_t, kria_state.k[0].p[0].t[0].tt_clocked),
																				}),
																			},
																			{
																				.name = "trigger_clocked",
																				.read = json_read_scalar,
																				.write = json_write_bool,
																				.params = &((json_read_scalar_params_t) {
																					.dst_size = sizeof_field(nvram_data_t, kria_state.k[0].p[0].t[0].trigger_clocked),
																					.dst_offset = offsetof(nvram_data_t, kria_state.k[0].p[0].t[0].trigger_clocked),
																				}),
																			},
																		}),
																	}),
																}),
															}),
														},
														{
															.name = "scale",
															.read = json_read_scalar,
															.write = json_write_number,
															.params = &((json_read_scalar_params_t) {
																.dst_size = sizeof_field(nvram_data_t, kria_state.k[0].p[0].scale),
																.dst_offset = offsetof(nvram_data_t, kria_state.k[0].p[0].scale),
															}),
														},
													}),
												}),
											}),
										}),
									},
									{
										.name = "curr_pattern",
										.read = json_read_scalar,
										.write = json_write_number,
										.params = &((json_read_scalar_params_t) {
											.dst_size = sizeof_field(nvram_data_t, kria_state.k[0].pattern),
											.dst_offset = offsetof(nvram_data_t, kria_state.k[0].pattern),
										}),
									},
									{
										.name = "meta_pat",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_size = sizeof_field(nvram_data_t, kria_state.k[0].meta_pat),
											.dst_offset = offsetof(nvram_data_t, kria_state.k[0].meta_pat),
										}),
									},
									{
										.name = "meta_steps",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_size = sizeof_field(nvram_data_t, kria_state.k[0].meta_steps),
											.dst_offset = offsetof(nvram_data_t, kria_state.k[0].meta_steps),
										}),
									},
									{
										.name = "meta_start",
										.read = json_read_scalar,
										.write = json_write_number,
										.params = &((json_read_scalar_params_t) {
											.dst_size = sizeof_field(nvram_data_t, kria_state.k[0].meta_start),
											.dst_offset = offsetof(nvram_data_t, kria_state.k[0].meta_start),
										}),
									},
									{
										.name = "meta_end",
										.read = json_read_scalar,
										.write = json_write_number,
										.params = &((json_read_scalar_params_t) {
											.dst_size = sizeof_field(nvram_data_t, kria_state.k[0].meta_end),
											.dst_offset = offsetof(nvram_data_t, kria_state.k[0].meta_end),
										}),
									},
									{
										.name = "meta_len",
										.read = json_read_scalar,
										.write = json_write_number,
										.params = &((json_read_scalar_params_t) {
											.dst_size = sizeof_field(nvram_data_t, kria_state.k[0].meta_len),
											.dst_offset = offsetof(nvram_data_t, kria_state.k[0].meta_len),
										}),
									},
									{
										.name = "meta_lswap",
										.read = json_read_scalar,
										.write = json_write_number,
										.params = &((json_read_scalar_params_t) {
											.dst_size = sizeof_field(nvram_data_t, kria_state.k[0].meta_lswap),
											.dst_offset = offsetof(nvram_data_t, kria_state.k[0].meta_lswap),
										}),
									},
									{
										.name = "glyph",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_size = sizeof_field(nvram_data_t, kria_state.k[0].glyph),
											.dst_offset = offsetof(nvram_data_t, kria_state.k[0].glyph),
										}),
									},
								}),
							}),
						}),
					}),
				},
			}),
		}),
	},
	{
		.name = "mp",
		.read = json_read_object,
		.write = json_write_object,
		.state = &ansible_app_object_state[0],
		.params = &((json_read_object_params_t) {
			.docdef_ct = 4,
			.docdefs = ((json_docdef_t[]) {
				{
					.name = "curr_preset",
					.read = json_read_scalar,
					.write = json_write_number,
					.params = &((json_read_scalar_params_t) {
						.dst_offset = offsetof(nvram_data_t, mp_state.preset),
						.dst_size = sizeof_field(nvram_data_t, mp_state.preset),
					}),
				},
				{
					.name = "sound",
					.read = json_read_scalar,
					.write = json_write_number,
					.params = &((json_read_scalar_params_t) {
						.dst_offset = offsetof(nvram_data_t, mp_state.sound),
						.dst_size = sizeof_field(nvram_data_t, mp_state.sound),
					}),
				},
				{
					.name = "voice_mode",
					.read = json_read_scalar,
					.write = json_write_number,
					.params = &((json_read_scalar_params_t) {
						.dst_offset = offsetof(nvram_data_t, mp_state.voice_mode),
						.dst_size = sizeof_field(nvram_data_t, mp_state.voice_mode),
					}),
				},
				{
					.name = "presets",
					.read = json_read_array,
					.write = json_write_array,
					.state = &ansible_json_read_array_state[0],
					.params = &((json_read_array_params_t) {
						.array_len = sizeof_field(nvram_data_t, mp_state.m) / sizeof_field(nvram_data_t, mp_state.m[0]),
						.item_size = sizeof_field(nvram_data_t, mp_state.m[0]),
						.item_docdef = &((json_docdef_t) {
							.read = json_read_object_cached,
							.write = json_write_object,
							.state = &ansible_app_object_state[1],
							.params = &((json_read_object_params_t) {
								.docdef_ct = 14,
								.dst_size = sizeof(mp_data_t),
								.dst_offset = offsetof(nvram_data_t, mp_state.m[0]),
								.alloc = STATIC_ALLOC(mp_data_t, m),
								.free = nop_free,
								.docdefs = ((json_docdef_t[]) {
									{
										.name = "count",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_offset = offsetof(nvram_data_t, mp_state.m[0].count),
											.dst_size = sizeof_field(nvram_data_t, mp_state.m[0].count),
										}),
									},
									{
										.name = "speed",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_offset = offsetof(nvram_data_t, mp_state.m[0].speed),
											.dst_size = sizeof_field(nvram_data_t, mp_state.m[0].speed) / sizeof_field(nvram_data_t, mp_state.m[0].speed[0]),
										}),
									},
									{
										.name = "min",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_offset = offsetof(nvram_data_t, mp_state.m[0].min),
											.dst_size = sizeof_field(nvram_data_t, mp_state.m[0].min),
										}),
									},
									{
										.name = "max",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_offset = offsetof(nvram_data_t, mp_state.m[0].max),
											.dst_size = sizeof_field(nvram_data_t, mp_state.m[0].max),
										}),
									},
									{
										.name = "trigger",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_offset = offsetof(nvram_data_t, mp_state.m[0].trigger),
											.dst_size = sizeof_field(nvram_data_t, mp_state.m[0].trigger),
										}),
									},
									{
										.name = "toggle",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_offset = offsetof(nvram_data_t, mp_state.m[0].toggle),
											.dst_size = sizeof_field(nvram_data_t, mp_state.m[0].toggle),
										}),
									},
									{
										.name = "rules",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_offset = offsetof(nvram_data_t, mp_state.m[0].rules),
											.dst_size = sizeof_field(nvram_data_t, mp_state.m[0].rules),
										}),
									},
									{
										.name = "rule_dests",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_offset = offsetof(nvram_data_t, mp_state.m[0].rule_dests),
											.dst_size = sizeof_field(nvram_data_t, mp_state.m[0].rule_dests),
										}),
									},
									{
										.name = "sync",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_offset = offsetof(nvram_data_t, mp_state.m[0].sync),
											.dst_size = sizeof_field(nvram_data_t, mp_state.m[0].sync),
										}),
									},
									{
										.name = "rule_dest_targets",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_offset = offsetof(nvram_data_t, mp_state.m[0].rule_dest_targets),
											.dst_size = sizeof_field(nvram_data_t, mp_state.m[0].rule_dest_targets),
										}),
									},
									{
										.name = "smin",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_offset = offsetof(nvram_data_t, mp_state.m[0].smin),
											.dst_size = sizeof_field(nvram_data_t, mp_state.m[0].smin),
										}),
									},
									{
										.name = "smax",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_offset = offsetof(nvram_data_t, mp_state.m[0].smax),
											.dst_size = sizeof_field(nvram_data_t, mp_state.m[0].smax),
										}),
									},
									{
										.name = "scale",
										.read = json_read_scalar,
										.write = json_write_number,
										.params = &((json_read_scalar_params_t) {
											.dst_offset = offsetof(nvram_data_t, mp_state.m[0].scale),
											.dst_size = sizeof_field(nvram_data_t, mp_state.m[0].scale),
										}),
									},
									{
										.name = "glyph",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_offset = offsetof(nvram_data_t, mp_state.m[0].glyph),
											.dst_size = sizeof_field(nvram_data_t, mp_state.m[0].glyph),
										}),
									},
								}),
							}),
						}),
					}),
				},
			}),
		}),
	},
	{
		.name = "es",
		.read = json_read_object,
		.write = json_write_object,
		.fresh = true,
		.state = &ansible_app_object_state[0],
		.params = &((json_read_object_params_t) {
			.docdef_ct = 2,
			.docdefs = ((json_docdef_t[]) {
				{
					.name = "curr_preset",
					.read = json_read_scalar,
					.write = json_write_number,
					.params = &((json_read_scalar_params_t) {
						.dst_offset = offsetof(nvram_data_t, es_state.preset),
						.dst_size = sizeof_field(nvram_data_t, es_state.preset),
					}),
				},
				{
					.name = "presets",
					.read = json_read_array,
					.write = json_write_array,
					.fresh = true,
					.state = &ansible_json_read_array_state[0],
					.params = &((json_read_array_params_t) {
						.array_len = sizeof_field(nvram_data_t, es_state.e) / sizeof_field(nvram_data_t, es_state.e[0]),
						.item_size = sizeof_field(nvram_data_t, es_state.e[0]),
						.item_docdef = &((json_docdef_t) {
							.read = json_read_object_cached,
							.write = json_write_object,
							.fresh = true,
							.state = &ansible_app_object_state[1],
							.params = &((json_read_object_params_t) {
								.docdef_ct = 8,
								.dst_size = sizeof(es_data_t),
								.dst_offset = offsetof(nvram_data_t, es_state.e[0]),
								.alloc = STATIC_ALLOC(es_data_t, e),
								.free = nop_free,
								.docdefs = ((json_docdef_t[]) {
									{
										.name = "arp",
										.read = json_read_scalar,
										.write = json_write_number,
										.params = &((json_read_scalar_params_t) {
											.dst_offset = offsetof(nvram_data_t, es_state.e[0].arp),
											.dst_size = sizeof_field(nvram_data_t, es_state.e[0].arp),
										}),
									},
									{
										.name = "p_select",
										.read = json_read_scalar,
										.write = json_write_number,
										.params = &((json_read_scalar_params_t) {
											.dst_offset = offsetof(nvram_data_t, es_state.e[0].p_select),
											.dst_size = sizeof_field(nvram_data_t, es_state.e[0].p_select),
										}),
									},
									{
										.name = "voices",
										.read = json_read_scalar,
										.write = json_write_number,
										.params = &((json_read_scalar_params_t) {
											.dst_offset = offsetof(nvram_data_t, es_state.e[0].voices),
											.dst_size = sizeof_field(nvram_data_t, es_state.e[0].voices),
										}),
									},
									{
										.name = "octave",
										.read = json_read_scalar,
										.write = json_write_number,
										.params = &((json_read_scalar_params_t) {
											.dst_offset = offsetof(nvram_data_t, es_state.e[0].octave),
											.dst_size = sizeof_field(nvram_data_t, es_state.e[0].octave),
										}),
									},
									{
										.name = "scale",
										.read = json_read_scalar,
										.write = json_write_number,
										.params = &((json_read_scalar_params_t) {
											.dst_offset = offsetof(nvram_data_t, es_state.e[0].scale),
											.dst_size = sizeof_field(nvram_data_t, es_state.e[0].scale),
										}),
									},
									{
										.name = "keymap",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.fresh = true,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_offset = offsetof(nvram_data_t, es_state.e[0].keymap),
											.dst_size = sizeof_field(nvram_data_t, es_state.e[0].keymap),
										}),
									},
									{
										.name = "patterns",
										.read = json_read_array,
										.write = json_write_array,
										.fresh = true,
										.state = &ansible_json_read_array_state[1],
										.params = &((json_read_array_params_t) {
											.array_len = sizeof_field(nvram_data_t, es_state.e[0].p) / sizeof_field(nvram_data_t, es_state.e[0].p[0]),
											.item_size = sizeof_field(nvram_data_t, es_state.e[0].p[0]),
											.item_docdef = &((json_docdef_t) {
												.read = json_read_object,
												.write = json_write_object,
												.fresh = true,
												.state = &ansible_app_object_state[2],
												.params = &((json_read_object_params_t) {
													.docdef_ct = 13,
													.docdefs = ((json_docdef_t[]) {
														{
															.name = "events",
															.read = json_read_array,
															.write = json_write_array,
															.fresh = true,
															.state = &ansible_json_read_array_state[2],
															.params = &((json_read_array_params_t) {
																.array_len = sizeof_field(nvram_data_t, es_state.e[0].p[0].e) / sizeof_field(nvram_data_t, es_state.e[0].p[0].e[0]),
																.item_size = sizeof_field(nvram_data_t, es_state.e[0].p[0].e[0]),
																.item_docdef = &((json_docdef_t) {
																	.read = json_read_object,
																	.write = json_write_object,
																	.fresh = true,
																	.state = &ansible_app_object_state[3],
																	.params = &((json_read_object_params_t) {
																		.docdef_ct = 3,
																		.docdefs = ((json_docdef_t[]) {
																			{
																				.name = "on",
																				.read = json_read_scalar,
																				.write = json_write_number,
																				.params = &((json_read_scalar_params_t) {
																					.dst_offset = offsetof(nvram_data_t, es_state.e[0].p[0].e[0].on),
																					.dst_size = sizeof_field(nvram_data_t, es_state.e[0].p[0].e[0].on),
																				}),
																			},
																			{
																				.name = "index",
																				.read = json_read_scalar,
																				.write = json_write_number,
																				.params = &((json_read_scalar_params_t) {
																					.dst_offset = offsetof(nvram_data_t, es_state.e[0].p[0].e[0].index),
																					.dst_size = sizeof_field(nvram_data_t, es_state.e[0].p[0].e[0].index),
																				}),
																			},
																			{
																				.name = "interval",
																				.read = json_read_scalar,
																				.write = json_write_number,
																				.params = &((json_read_scalar_params_t) {
																					.dst_offset = offsetof(nvram_data_t, es_state.e[0].p[0].e[0].interval),
																					.dst_size = sizeof_field(nvram_data_t, es_state.e[0].p[0].e[0].interval),
																				}),
																			},
																		}),
																	}),
																}),
															}),
														},
														{
															.name = "interval_ind",
															.read = json_read_scalar,
															.write = json_write_number,
															.params = &((json_read_scalar_params_t) {
																.dst_offset = offsetof(nvram_data_t, es_state.e[0].p[0].interval_ind),
																.dst_size = sizeof_field(nvram_data_t, es_state.e[0].p[0].interval_ind),
															}),
														},
														{
															.name = "length",
															.read = json_read_scalar,
															.write = json_write_number,
															.params = &((json_read_scalar_params_t) {
																.dst_offset = offsetof(nvram_data_t, es_state.e[0].p[0].length),
																.dst_size = sizeof_field(nvram_data_t, es_state.e[0].p[0].length),
															}),
														},
														{
															.name = "loop",
															.read = json_read_scalar,
															.write = json_write_number,
															.params = &((json_read_scalar_params_t) {
																.dst_offset = offsetof(nvram_data_t, es_state.e[0].p[0].loop),
																.dst_size = sizeof_field(nvram_data_t, es_state.e[0].p[0].loop),
															}),
														},
														{
															.name = "root_x",
															.read = json_read_scalar,
															.write = json_write_number,
															.params = &((json_read_scalar_params_t) {
																.dst_offset = offsetof(nvram_data_t, es_state.e[0].p[0].root_x),
																.dst_size = sizeof_field(nvram_data_t, es_state.e[0].p[0].root_x),
															}),
														},
														{
															.name = "root_y",
															.read = json_read_scalar,
															.write = json_write_number,
															.params = &((json_read_scalar_params_t) {
																.dst_offset = offsetof(nvram_data_t, es_state.e[0].p[0].root_y),
																.dst_size = sizeof_field(nvram_data_t, es_state.e[0].p[0].root_y),
															}),
														},
														{
															.name = "edge",
															.read = json_read_scalar,
															.write = json_write_number,
															.params = &((json_read_scalar_params_t) {
																.dst_offset = offsetof(nvram_data_t, es_state.e[0].p[0].edge),
																.dst_size = sizeof_field(nvram_data_t, es_state.e[0].p[0].edge),
															}),
														},
														{
															.name = "edge_time",
															.read = json_read_scalar,
															.write = json_write_number,
															.params = &((json_read_scalar_params_t) {
																.dst_offset = offsetof(nvram_data_t, es_state.e[0].p[0].edge_time),
																.dst_size = sizeof_field(nvram_data_t, es_state.e[0].p[0].edge_time),
															}),
														},
														{
															.name = "voices",
															.read = json_read_scalar,
															.write = json_write_number,
															.params = &((json_read_scalar_params_t) {
																.dst_offset = offsetof(nvram_data_t, es_state.e[0].p[0].voices),
																.dst_size = sizeof_field(nvram_data_t, es_state.e[0].p[0].voices),
															}),
														},
														{
															.name = "dir",
															.read = json_read_scalar,
															.write = json_write_number,
															.params = &((json_read_scalar_params_t) {
																.dst_offset = offsetof(nvram_data_t, es_state.e[0].p[0].dir),
																.dst_size = sizeof_field(nvram_data_t, es_state.e[0].p[0].dir),
															}),
														},
														{
															.name = "linearize",
															.read = json_read_scalar,
															.write = json_write_number,
															.params = &((json_read_scalar_params_t) {
																.dst_offset = offsetof(nvram_data_t, es_state.e[0].p[0].linearize),
																.dst_size = sizeof_field(nvram_data_t, es_state.e[0].p[0].linearize),
															}),
														},
														{
															.name = "start",
															.read = json_read_scalar,
															.write = json_write_number,
															.params = &((json_read_scalar_params_t) {
																.dst_offset = offsetof(nvram_data_t, es_state.e[0].p[0].start),
																.dst_size = sizeof_field(nvram_data_t, es_state.e[0].p[0].start),
															}),
														},
														{
															.name = "end",
															.read = json_read_scalar,
															.write = json_write_number,
															.params = &((json_read_scalar_params_t) {
																.dst_offset = offsetof(nvram_data_t, es_state.e[0].p[0].end),
																.dst_size = sizeof_field(nvram_data_t, es_state.e[0].p[0].end),
															}),
														},
													}),
												}),
											}),
										}),
									},
									{
										.name = "glyph",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.fresh = true,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_size = sizeof_field(nvram_data_t, es_state.e[0].glyph),
											.dst_offset = offsetof(nvram_data_t, es_state.e[0].glyph),
										}),
									},
								}),
							}),
						}),
					}),
				},
			}),
		}),
	},
	{
		.name = "levels",
		.read = json_read_object,
		.write = json_write_object,
		.state = &ansible_app_object_state[0],
		.params = &((json_read_object_params_t) {
			.docdef_ct = 2,
			.docdefs = ((json_docdef_t[]) {
				{
					.name = "curr_preset",
					.read = json_read_scalar,
					.write = json_write_number,
					.params = &((json_read_scalar_params_t) {
						.dst_offset = offsetof(nvram_data_t, levels_state.preset),
						.dst_size = sizeof_field(nvram_data_t, levels_state.preset),
					}),
				},
				{
					.name = "presets",
					.read = json_read_array,
					.write = json_write_array,
					.state = &ansible_json_read_array_state[0],
					.params = &((json_read_array_params_t) {
						.array_len = sizeof_field(nvram_data_t, levels_state.l) / sizeof_field(nvram_data_t, levels_state.l[0]),
						.item_size = sizeof_field(nvram_data_t, levels_state.l[0]),
						.item_docdef = &((json_docdef_t) {
							.read = json_read_object_cached,
							.write = json_write_object,
							.state = &ansible_app_object_state[1],
							.params = &((json_read_object_params_t) {
								.docdef_ct = 13,
								.dst_size = sizeof(levels_data_t),
								.dst_offset = offsetof(nvram_data_t, levels_state.l[0]),
								.alloc = STATIC_ALLOC(levels_data_t, l),
								.free = nop_free,
								.docdefs = ((json_docdef_t[]) {
									{
										.name = "pattern",
										.read = json_read_array,
										.write = json_write_array,
										.state = &ansible_json_read_array_state[1],
										.params = &((json_read_array_params_t) {
											.array_len = sizeof_field(nvram_data_t, levels_state.l[0].pattern) / sizeof_field(nvram_data_t, levels_state.l[0].pattern[0]),
											.item_size = sizeof_field(nvram_data_t, levels_state.l[0].pattern[0]),
											.item_docdef = &((json_docdef_t) {
												.read = json_read_buffer,
												.write = json_write_buffer,
												.state = &ansible_json_read_buffer_state,
												.params = &((json_read_buffer_params_t) {
													.dst_offset = offsetof(nvram_data_t, levels_state.l[0].pattern[0]),
													.dst_size = sizeof_field(nvram_data_t, levels_state.l[0].pattern[0]),
												}),
											}),
										}),
									},
									{
										.name = "note",
										.read = json_read_array,
										.write = json_write_array,
										.state = &ansible_json_read_array_state[1],
										.params = &((json_read_array_params_t) {
											.array_len = sizeof_field(nvram_data_t, levels_state.l[0].note) / sizeof_field(nvram_data_t, levels_state.l[0].note[0]),
											.item_size = sizeof_field(nvram_data_t, levels_state.l[0].note[0]),
											.item_docdef = &((json_docdef_t) {
												.read = json_read_buffer,
												.write = json_write_buffer,
												.state = &ansible_json_read_buffer_state,
												.params = &((json_read_buffer_params_t) {
													.dst_offset = offsetof(nvram_data_t, levels_state.l[0].note[0]),
													.dst_size = sizeof_field(nvram_data_t, levels_state.l[0].note[0]),
												}),
											}),
										}),
									},
									{
										.name = "mode",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_offset = offsetof(nvram_data_t, levels_state.l[0].mode),
											.dst_size = sizeof_field(nvram_data_t, levels_state.l[0].mode),
										}),
									},
									{
										.name = "all",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_offset = offsetof(nvram_data_t, levels_state.l[0].all),
											.dst_size = sizeof_field(nvram_data_t, levels_state.l[0].all),
										}),
									},
									{
										.name = "now",
										.read = json_read_scalar,
										.write = json_write_number,
										.params = &((json_read_scalar_params_t) {
											.dst_offset = offsetof(nvram_data_t, levels_state.l[0].now),
											.dst_size = sizeof_field(nvram_data_t, levels_state.l[0].now),
										}),
									},
									{
										.name = "start",
										.read = json_read_scalar,
										.write = json_write_number,
										.params = &((json_read_scalar_params_t) {
											.dst_offset = offsetof(nvram_data_t, levels_state.l[0].start),
											.dst_size = sizeof_field(nvram_data_t, levels_state.l[0].start),
										}),
									},
									{
										.name = "len",
										.read = json_read_scalar,
										.write = json_write_number,
										.params = &((json_read_scalar_params_t) {
											.dst_offset = offsetof(nvram_data_t, levels_state.l[0].len),
											.dst_size = sizeof_field(nvram_data_t, levels_state.l[0].len),
											.signed_val = true,
										}),
									},
									{
										.name = "dir",
										.read = json_read_scalar,
										.write = json_write_number,
										.params = &((json_read_scalar_params_t) {
											.dst_offset = offsetof(nvram_data_t, levels_state.l[0].dir),
											.dst_size = sizeof_field(nvram_data_t, levels_state.l[0].dir),
										}),
									},
									{
										.name = "scale",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_offset = offsetof(nvram_data_t, levels_state.l[0].scale),
											.dst_size = sizeof_field(nvram_data_t, levels_state.l[0].scale),
										}),
									},
									{
										.name = "octave",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_offset = offsetof(nvram_data_t, levels_state.l[0].octave),
											.dst_size = sizeof_field(nvram_data_t, levels_state.l[0].octave),
										}),
									},
									{
										.name = "offset",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_offset = offsetof(nvram_data_t, levels_state.l[0].offset),
											.dst_size = sizeof_field(nvram_data_t, levels_state.l[0].offset),
										}),
									},
									{
										.name = "range",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_offset = offsetof(nvram_data_t, levels_state.l[0].range),
											.dst_size = sizeof_field(nvram_data_t, levels_state.l[0].range),
										}),
									},
									{
										.name = "slew",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_offset = offsetof(nvram_data_t, levels_state.l[0].slew),
											.dst_size = sizeof_field(nvram_data_t, levels_state.l[0].slew),
										}),
									},
								}),
							}),
						}),
					}),
				},
			}),
		}),
	},

	{
		.name = "cycles",
		.read = json_read_object,
		.write = json_write_object,
		.state = &ansible_app_object_state[0],
		.params = &((json_read_object_params_t) {
			.docdef_ct = 2,
			.docdefs = ((json_docdef_t[]) {
				{
					.name = "curr_preset",
					.read = json_read_scalar,
					.write = json_write_number,
					.params = &((json_read_scalar_params_t) {
						.signed_val = false,
						.dst_offset = offsetof(nvram_data_t, cycles_state.preset),
						.dst_size = sizeof_field(nvram_data_t, cycles_state.preset),
					}),
				},
				{
					.name = "presets",
					.read = json_read_array,
					.write = json_write_array,
					.state = &ansible_json_read_array_state,
					.params = &((json_read_array_params_t) {
						.array_len = sizeof_field(nvram_data_t, cycles_state.c) / sizeof_field(nvram_data_t, cycles_state.c[0]),
						.item_size = sizeof_field(nvram_data_t, cycles_state.c[0]),
						.item_docdef = &((json_docdef_t) {
							.read = json_read_object_cached,
							.write = json_write_object,
							.state = &ansible_app_object_state[1],
							.params = &((json_read_object_params_t) {
								.docdef_ct = 9,
								.dst_size = sizeof(cycles_data_t),
								.dst_offset = offsetof(nvram_data_t, cycles_state.c[0]),
								.alloc = STATIC_ALLOC(cycles_data_t, c),
								.free = nop_free,
								.docdefs = (json_docdef_t[]) {
									{
										.name = "pos",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_size = sizeof_field(nvram_data_t, cycles_state.c[0].pos),
											.dst_offset = offsetof(nvram_data_t, cycles_state.c[0].pos),
										}),
									},
									{
										.name = "speed",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_size = sizeof_field(nvram_data_t, cycles_state.c[0].speed),
											.dst_offset = offsetof(nvram_data_t, cycles_state.c[0].speed),
										}),
									},
									{
										.name = "mult",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_size = sizeof_field(nvram_data_t, cycles_state.c[0].mult),
											.dst_offset = offsetof(nvram_data_t, cycles_state.c[0].mult),
										}),
									},
									{
										.name = "range",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_size = sizeof_field(nvram_data_t, cycles_state.c[0].range),
											.dst_offset = offsetof(nvram_data_t, cycles_state.c[0].range),
										}),
									},
									{
										.name = "div",
										.read = json_read_buffer,
										.write = json_write_buffer,
										.state = &ansible_json_read_buffer_state,
										.params = &((json_read_buffer_params_t) {
											.dst_size = sizeof_field(nvram_data_t, cycles_state.c[0].div),
											.dst_offset = offsetof(nvram_data_t, cycles_state.c[0].div),
										}),
									},
									{
										.name = "mode",
										.read = json_read_scalar,
										.write = json_write_number,
										.params = &((json_read_scalar_params_t) {
											.signed_val = false,
											.dst_offset = offsetof(nvram_data_t, cycles_state.c[0].mode),
											.dst_size = sizeof_field(nvram_data_t, cycles_state.c[0].mode),
										}),
									},
									{
										.name = "shape",
										.read = json_read_scalar,
										.write = json_write_number,
										.params = &((json_read_scalar_params_t) {
											.signed_val = false,
											.dst_offset = offsetof(nvram_data_t, cycles_state.c[0].shape),
											.dst_size = sizeof_field(nvram_data_t, cycles_state.c[0].shape),
										}),
									},
									{
										.name = "friction",
										.read = json_read_scalar,
										.write = json_write_number,
										.params = &((json_read_scalar_params_t) {
											.signed_val = false,
											.dst_offset = offsetof(nvram_data_t, cycles_state.c[0].friction),
											.dst_size = sizeof_field(nvram_data_t, cycles_state.c[0].friction),
										}),
									},
									{
										.name = "force",
										.read = json_read_scalar,
										.write = json_write_number,
										.params = &((json_read_scalar_params_t) {
											.signed_val = false,
											.dst_offset = offsetof(nvram_data_t, cycles_state.c[0].force),
											.dst_size = sizeof_field(nvram_data_t, cycles_state.c[0].force),
										}),
									},
								},
							}),
						}),
					}),
				},
			}),
		}),
	},
	{
		.name = "midi_standard",
		.read = json_read_object,
		.write = json_write_object,
		.state = &ansible_app_object_state[0],
		.params = &((json_read_object_params_t) {
			.docdef_ct = 5,
			.docdefs = ((json_docdef_t[]) {
				{
					.name = "clock_period",
					.read = json_read_scalar,
					.write = json_write_number,
					.params = &((json_read_scalar_params_t) {
						.signed_val = false,
						.dst_offset = offsetof(nvram_data_t, midi_standard_state.clock_period),
						.dst_size = sizeof_field(nvram_data_t, midi_standard_state.clock_period),
					}),
				},
				{
					.name = "voicing",
					.read = json_read_scalar,
					.write = json_write_number,
					.params = &((json_read_scalar_params_t) {
						.signed_val = false,
						.dst_offset = offsetof(nvram_data_t, midi_standard_state.voicing),
						.dst_size = sizeof_field(nvram_data_t, midi_standard_state.voicing),
					}),
				},
				{
					.name = "fixed",
					.read = json_read_object,
					.write = json_write_object,
					.state = &ansible_app_object_state[1],
					.params = &((json_read_object_params_t) {
						.docdef_ct = 2,
						.docdefs = ((json_docdef_t[]) {
							{
								.name = "notes",
								.read = json_read_buffer,
								.write = json_write_buffer,
								.state = &ansible_json_read_buffer_state,
								.params = &((json_read_buffer_params_t) {
									.dst_size = sizeof_field(nvram_data_t, midi_standard_state.fixed.notes),
									.dst_offset = offsetof(nvram_data_t, midi_standard_state.fixed.notes),
								}),
							},
							{
								.name = "cc",
								.read = json_read_buffer,
								.write = json_write_buffer,
								.state = &ansible_json_read_buffer_state,
								.params = &((json_read_buffer_params_t) {
									.dst_size = sizeof_field(nvram_data_t, midi_standard_state.fixed.cc),
									.dst_offset = offsetof(nvram_data_t, midi_standard_state.fixed.cc),
								}),
							},
						}),
					}),
				},
				{
					.name = "shift",
					.read = json_read_scalar,
					.write = json_write_number,
					.params = &((json_read_scalar_params_t) {
						.signed_val = true,
						.dst_offset = offsetof(nvram_data_t, midi_standard_state.shift),
						.dst_size = sizeof_field(nvram_data_t, midi_standard_state.shift),
					}),
				},
				{
					.name = "slew",
					.read = json_read_scalar,
					.write = json_write_number,
					.params = &((json_read_scalar_params_t) {
						.signed_val = true,
						.dst_offset = offsetof(nvram_data_t, midi_standard_state.slew),
						.dst_size = sizeof_field(nvram_data_t, midi_standard_state.slew),
					}),
				},
			}),
		}),
	},

	{
		.name = "midi_arp",
		.read = json_read_object,
		.write = json_write_object,
		.state = &ansible_app_object_state[0],
		.params = &((json_read_object_params_t) {
			.docdef_ct = 4,
			.docdefs = ((json_docdef_t[]) {
				{
					.name = "clock_period",
					.read = json_read_scalar,
					.write = json_write_number,
					.params = &((json_read_scalar_params_t) {
						.signed_val = false,
						.dst_offset = offsetof(nvram_data_t, midi_arp_state.clock_period),
						.dst_size = sizeof_field(nvram_data_t, midi_arp_state.clock_period),
					}),
				},
				{
					.name = "style",
					.read = json_read_scalar,
					.write = json_write_number,
					.params = &((json_read_scalar_params_t) {
						.signed_val = false,
						.dst_offset = offsetof(nvram_data_t, midi_arp_state.style),
						.dst_size = sizeof_field(nvram_data_t, midi_arp_state.style),
					}),
				},
				{
					.name = "hold",
					.read = json_read_scalar,
					.write = json_write_bool,
					.params = &((json_read_scalar_params_t) {
						.dst_offset = offsetof(nvram_data_t, midi_arp_state.hold),
						.dst_size = sizeof_field(nvram_data_t, midi_arp_state.hold),
					}),
				},
				{
					.name = "players",
					.read = json_read_array,
					.write = json_write_array,
					.state = &ansible_json_read_array_state,
					.params = &((json_read_array_params_t) {
						.array_len = sizeof_field(nvram_data_t, midi_arp_state.p) / sizeof_field(nvram_data_t, midi_arp_state.p[0]),
						.item_size = sizeof_field(nvram_data_t, midi_arp_state.p[0]),
						.item_docdef = &((json_docdef_t) {
							.read = json_read_object,
							.write = json_write_object,
							.state = &ansible_app_object_state[1],
							.params = &((json_read_object_params_t) {
								.docdef_ct = 8,
								.docdefs = (json_docdef_t[]) {
									{
										.name = "fill",
										.read = json_read_scalar,
										.write = json_write_number,
										.params = &((json_read_scalar_params_t) {
											.signed_val = false,
											.dst_offset = offsetof(nvram_data_t, midi_arp_state.p[0].fill),
											.dst_size = sizeof_field(nvram_data_t, midi_arp_state.p[0].fill),
										}),
									},
									{
										.name = "division",
										.read = json_read_scalar,
										.write = json_write_number,
										.params = &((json_read_scalar_params_t) {
											.signed_val = false,
											.dst_offset = offsetof(nvram_data_t, midi_arp_state.p[0].division),
											.dst_size = sizeof_field(nvram_data_t, midi_arp_state.p[0].division),
										}),
									},
									{
										.name = "rotation",
										.read = json_read_scalar,
										.write = json_write_number,
										.params = &((json_read_scalar_params_t) {
											.signed_val = false,
											.dst_offset = offsetof(nvram_data_t, midi_arp_state.p[0].rotation),
											.dst_size = sizeof_field(nvram_data_t, midi_arp_state.p[0].rotation),
										}),
									},
									{
										.name = "gate",
										.read = json_read_scalar,
										.write = json_write_number,
										.params = &((json_read_scalar_params_t) {
											.signed_val = false,
											.dst_offset = offsetof(nvram_data_t, midi_arp_state.p[0].gate),
											.dst_size = sizeof_field(nvram_data_t, midi_arp_state.p[0].gate),
										}),
									},
									{
										.name = "steps",
										.read = json_read_scalar,
										.write = json_write_number,
										.params = &((json_read_scalar_params_t) {
											.signed_val = false,
											.dst_offset = offsetof(nvram_data_t, midi_arp_state.p[0].steps),
											.dst_size = sizeof_field(nvram_data_t, midi_arp_state.p[0].steps),
										}),
									},
									{
										.name = "offset",
										.read = json_read_scalar,
										.write = json_write_number,
										.params = &((json_read_scalar_params_t) {
											.signed_val = false,
											.dst_offset = offsetof(nvram_data_t, midi_arp_state.p[0].offset),
											.dst_size = sizeof_field(nvram_data_t, midi_arp_state.p[0].offset),
										}),
									},
									{
										.name = "slew",
										.read = json_read_scalar,
										.write = json_write_number,
										.params = &((json_read_scalar_params_t) {
											.signed_val = true,
											.dst_offset = offsetof(nvram_data_t, midi_arp_state.p[0].slew),
											.dst_size = sizeof_field(nvram_data_t, midi_arp_state.p[0].slew),
										}),
									},
									{
										.name = "shift",
										.read = json_read_scalar,
										.write = json_write_number,
										.params = &((json_read_scalar_params_t) {
											.signed_val = true,
											.dst_offset = offsetof(nvram_data_t, midi_arp_state.p[0].shift),
											.dst_size = sizeof_field(nvram_data_t, midi_arp_state.p[0].shift),
										}),
									},
								},
							}),
						}),
					}),
				},
			}),
		}),
	},

	{
		.name = "tt",
		.read = json_read_object,
		.write = json_write_object,
		.state = &ansible_app_object_state[0],
		.params = &((json_read_object_params_t) {
			.docdef_ct = 3,
			.docdefs = ((json_docdef_t[]) {
				{
					.name = "clock_period",
					.read = json_read_scalar,
					.write = json_write_number,
					.params = &((json_read_scalar_params_t) {
						.signed_val = false,
						.dst_offset = offsetof(nvram_data_t, tt_state.clock_period),
						.dst_size = sizeof_field(nvram_data_t, tt_state.clock_period),
					}),
				},
				{
					.name = "tr_time",
					.read = json_read_buffer,
					.write = json_write_buffer,
					.state = &ansible_json_read_buffer_state,
					.params = &((json_read_buffer_params_t) {
						.dst_size = sizeof_field(nvram_data_t, tt_state.tr_time),
						.dst_offset = offsetof(nvram_data_t, tt_state.tr_time),
					}),
				},
				{
					.name = "cv_slew",
					.read = json_read_buffer,
					.write = json_write_buffer,
					.state = &ansible_json_read_buffer_state,
					.params = &((json_read_buffer_params_t) {
						.dst_size = sizeof_field(nvram_data_t, tt_state.cv_slew),
						.dst_offset = offsetof(nvram_data_t, tt_state.cv_slew),
					}),
				},
			}),
		}),
	},
};

/////////
// document

json_docdef_t ansible_preset_docdef = {
	.read = json_read_object,
	.write = json_write_object,
	.state = &ansible_root_object_state,
	.params = &((json_read_object_params_t) {
		.docdef_ct = 3,
		.docdefs = ((json_docdef_t[]) {
			{
				.name = "meta",
				.read = json_read_object,
				.write = json_write_object,
				.state = &ansible_section_object_state,
				.params = &((json_read_object_params_t) {
					.docdefs = ansible_meta_docdefs,
					.docdef_ct = sizeof(ansible_meta_docdefs) / sizeof(json_docdef_t),
				}),
			},
			{
				.name = "shared",
				.read = json_read_object,
				.write = json_write_object,
				.state = &ansible_section_object_state,
				.params = &((json_read_object_params_t) {
					.docdefs = ansible_shared_docdefs,
					.docdef_ct = sizeof(ansible_shared_docdefs) / sizeof(json_docdef_t),
				}),
			},
			{
				.name = "apps",
				.read = json_read_object,
				.write = json_write_object,
				.state = &ansible_section_object_state,
				.params = &((json_read_object_params_t) {
					.docdefs = ansible_app_docdefs,
					.docdef_ct = sizeof(ansible_app_docdefs) / sizeof(json_docdef_t),
				}),
			},
		}),
	}),
};
