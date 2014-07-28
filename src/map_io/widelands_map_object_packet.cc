/*
 * Copyright (C) 2007-2008, 2010 by the Widelands Development Team
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

#include "map_io/widelands_map_object_packet.h"

#include "base/wexception.h"
#include "economy/fleet.h"
#include "economy/portdock.h"
#include "io/fileread.h"
#include "io/filewrite.h"
#include "logic/battle.h"
#include "logic/critter_bob.h"
#include "logic/editor_game_base.h"
#include "logic/immovable.h"
#include "logic/map.h"
#include "logic/ship.h"
#include "logic/worker.h"
#include "map_io/widelands_map_map_object_loader.h"
#include "map_io/widelands_map_map_object_saver.h"

namespace Widelands {

#define CURRENT_PACKET_VERSION 1


MapObjectPacket::~MapObjectPacket() {
	while (loaders.size()) {
		delete *loaders.begin();
		loaders.erase(loaders.begin());
	}
}


void MapObjectPacket::Read
	(FileSystem & fs, Editor_Game_Base & egbase, MapMapObjectLoader & mol,
	 const OneWorldLegacyLookupTable& lookup_table)
{
	try {
		FileRead fr;
		fr.Open(fs, "binary/mapobjects");

		const uint8_t packet_version = fr.Unsigned8();
		if (packet_version != CURRENT_PACKET_VERSION)
			throw game_data_error
				("unknown/unhandled version %u", packet_version);

		// Initial loading stage
		for (;;)
			switch (uint8_t const header = fr.Unsigned8()) {
			case 0:
				return;
			case MapObject::HeaderImmovable:
				loaders.insert(Immovable::load(egbase, mol, fr, lookup_table));
				break;

			case MapObject::HeaderBattle:
				loaders.insert(Battle::load(egbase, mol, fr));
				break;

			case MapObject::HeaderCritter:
				loaders.insert(Critter_Bob::load(egbase, mol, fr, lookup_table));
				break;

			case MapObject::HeaderWorker:
				loaders.insert(Worker::load(egbase, mol, fr));
				break;

			case MapObject::HeaderWareInstance:
				loaders.insert(WareInstance::load(egbase, mol, fr));
				break;

			case MapObject::HeaderShip:
				loaders.insert(Ship::load(egbase, mol, fr));
				break;

			case MapObject::HeaderPortDock:
				loaders.insert(PortDock::load(egbase, mol, fr));
				break;

			case MapObject::HeaderFleet:
				loaders.insert(Fleet::load(egbase, mol, fr));
				break;

			default:
				throw game_data_error("unknown object header %u", header);
			}
	} catch (const std::exception & e) {
		throw game_data_error("map objects: %s", e.what());
	}
}


void MapObjectPacket::LoadFinish() {
	// load_pointer stage
	for (MapObject::Loader* temp_loader : loaders) {
		try {
			temp_loader->load_pointers();
		} catch (const std::exception & e) {
			throw wexception("load_pointers for %s: %s",
				to_string(temp_loader->get_object()->descr().type()).c_str(),
				e.what());
		}
	}

	// load_finish stage
	for (MapObject::Loader* temp_loader : loaders) {
		try {
			temp_loader->load_finish();
		} catch (const std::exception & e) {
			throw wexception("load_finish for %s: %s",
			                 to_string(temp_loader->get_object()->descr().type()).c_str(),
			                 e.what());
		}
		temp_loader->mol().mark_object_as_loaded(*temp_loader->get_object());
	}
}


void MapObjectPacket::Write
	(FileSystem & fs, Editor_Game_Base & egbase, MapMapObjectSaver & mos)
{
	FileWrite fw;

	fw.Unsigned8(CURRENT_PACKET_VERSION);

	std::vector<Serial> obj_serials = egbase.objects().all_object_serials_ordered();
	for
		(std::vector<Serial>::iterator cit = obj_serials.begin();
		 cit != obj_serials.end();
		 ++cit)
	{
		MapObject * pobj = egbase.objects().get_object(*cit);
		assert(pobj);
		MapObject & obj = *pobj;

		// These checks can be eliminated and the object saver simplified
		// once all MapObjects are saved using the new system
		if (mos.is_object_known(obj))
			continue;

		if (!obj.has_new_save_support())
			throw game_data_error
				("MO(%u of type %s) without new style save support not saved "
				 "explicitly",
				 obj.serial(), obj.descr().descname().c_str());

		mos.register_object(obj);
		obj.save(egbase, mos, fw);
		mos.mark_object_as_saved(obj);
	}

	fw.Unsigned8(0);

	fw.Write(fs, "binary/mapobjects");
}

}
