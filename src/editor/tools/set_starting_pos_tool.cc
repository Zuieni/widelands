/*
 * Copyright (C) 2002-2004, 2006-2012 by the Widelands Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include "editor/tools/set_starting_pos_tool.h"

#include "editor/editorinteractive.h"
#include "editor/tools/tool.h"
#include "graphic/graphic.h"
#include "graphic/playercolor.h"
#include "logic/map.h"

// global variable to pass data from callback to class
static int32_t current_player_;

/*
 * static callback function for overlay calculation
 */
int32_t editor_tool_set_starting_pos_callback(const Widelands::TCoords<Widelands::FCoords>& c,
                                              Widelands::Map& map) {
	// Area around already placed players
	Widelands::PlayerNumber const nr_players = map.get_nrplayers();
	for (Widelands::PlayerNumber p = 1, last = current_player_ - 1;; ++p) {
		for (; p <= last; ++p) {
			if (Widelands::Coords const sp = map.get_starting_pos(p)) {
				if (map.calc_distance(sp, c) < MIN_PLACE_AROUND_PLAYERS) {
					return 0;
				}
			}
		}
		if (last == nr_players) {
			break;
		}
		last = nr_players;
	}

	Widelands::NodeCaps const caps = c.field->nodecaps();
	if ((caps & Widelands::BUILDCAPS_SIZEMASK) == Widelands::BUILDCAPS_BIG)
		return caps;

	return 0;
}

EditorSetStartingPosTool::EditorSetStartingPosTool()
   : EditorTool(*this, *this, false), overlay_ids_(kMaxPlayers, 0) {
	current_player_ = 1;
}

int32_t EditorSetStartingPosTool::handle_click_impl(const Widelands::World&,
                                                    const Widelands::NodeAndTriangle<>& center,
                                                    EditorInteractive& eia,
                                                    EditorActionArgs*,
                                                    Widelands::Map* map) {
	assert(0 <= center.node.x);
	assert(center.node.x < map->get_width());
	assert(0 <= center.node.y);
	assert(center.node.y < map->get_height());
	if (current_player_) {
		if (map->get_nrplayers() < current_player_) {
			//  Mmh, my current player is not valid. Maybe the user has loaded a
			//  new map while this tool was active. We set the new player to a
			//  valid one. The sel pointer is the only thing that stays wrong, but
			//  this is not important
			current_player_ = 1;
		}

		//  check if field is valid
		if (editor_tool_set_starting_pos_callback(map->get_fcoords(center.node), *map)) {
			set_starting_pos(eia, current_player_, center.node, map);
		}
	}
	return 1;
}

void EditorSetStartingPosTool::set_starting_pos(EditorInteractive& eia,
                                                Widelands::PlayerNumber plnum,
                                                const Widelands::Coords& c,
                                                Widelands::Map* map) {
	FieldOverlayManager* overlay_manager = eia.mutable_field_overlay_manager();
	//  remove old overlay if any
	overlay_manager->remove_overlay(overlay_ids_.at(plnum - 1));

	//  add new overlay
	FieldOverlayManager::OverlayId overlay_id = overlay_manager->next_overlay_id();
	overlay_ids_[plnum - 1] = overlay_id;

	const Image* player_image =
	   playercolor_image(plnum - 1, g_gr->images().get("images/players/player_position.png"),
	                     g_gr->images().get("images/players/player_position_pc.png"));
	assert(player_image);

	overlay_manager->register_overlay(
	   c, player_image, 8, Vector2i(player_image->width() / 2, STARTING_POS_HOTSPOT_Y), overlay_id);

	//  set new player pos
	map->set_starting_pos(plnum, c);
}

Widelands::PlayerNumber EditorSetStartingPosTool::get_current_player() const {
	return current_player_;
}

void EditorSetStartingPosTool::set_current_player(int32_t const i) {
	current_player_ = i;
}
