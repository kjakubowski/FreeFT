/* Copyright (C) 2013-2014 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of FreeFT.
 */

#ifndef GAME_VISIBILITY_H
#define GAME_VISIBILITY_H

#include "game/world.h"
#include "game/actor.h"
#include "game/entity_map.h"
#include "occluder_map.h"

namespace gfx { class SceneRenderer; }

namespace game {

	class WorldViewer {
	public:
		WorldViewer(PWorld, EntityRef spectator = EntityRef());
		~WorldViewer();
		WorldViewer(const WorldViewer&) = delete;
		void operator=(const WorldViewer&) = delete;

		struct VisEntity {
			VisEntity() :mode(invisible), occluder_id(-1) { }

			unique_ptr<Entity> shadow;
			EntityRef ref;

			enum Mode {
				blending_out,
				blending_in,
				pre_blending_out,
				visible,
				invisible,
				shadowed,
			} mode;

			float blend_value;
			int occluder_id;
		};

		// WorldViewer has to be updated after each world simulation
		void update(double time_diff);
		void addToRender(gfx::SceneRenderer&);

		const FBox refBBox(ObjectRef) const;
		const Tile *refTile(ObjectRef) const;
		const Entity *refEntity(EntityRef) const;

		//TODO: option to ignore entities
		Intersection pixelIntersect(const int2 &screen_pos, const Entity *ignore = nullptr, Flags::Type flags = Flags::all) const;
		Intersection trace(const Segment &segment, const Entity *ignore = nullptr, Flags::Type flags = Flags::all) const;

	protected:
		bool isMovable(const Entity&) const;
		bool isVisible(const FBox &box, int index, bool is_movable) const;

		vector<VisEntity> m_entities;
		OccluderConfig m_occluder_config;

		PWorld m_world;

		const EntityRef m_spectator;
		float3 m_cur_pos, m_eye_pos;
		float3 m_cur_dir;
		float m_cur_fov;
	};

}

#endif
