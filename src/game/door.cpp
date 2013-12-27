/* Copyright (C) 2013-2014 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of FreeFT.
 */

#include "game/door.h"
#include "game/world.h"
#include "game/actor.h"
#include "game/sprite.h"
#include "sys/xml.h"
#include <cstdio>

namespace game {

	static const char *s_seq_names[Door::state_count] = {
		"Closed",
		"OpenedIn",
		"OpeningIn",
		"ClosingIn",
		"OpenedOut",
		"OpeningOut",
		"ClosingOut",
	};

	struct Transition { Door::State current, target, result; };
	static Transition s_transitions[] = {
		{ Door::state_closed, Door::state_opened_in,  Door::state_opening_in },
		{ Door::state_closed, Door::state_opened_out, Door::state_opening_out },
		{ Door::state_opened_in,  Door::state_closed, Door::state_closing_in },
		{ Door::state_opened_out, Door::state_closed, Door::state_closing_out },
	};

	bool Door::testSpriteType(PSprite sprite, Door::Type type) {
		int seq_ids[state_count];
		for(int n = 0; n < state_count; n++)
			seq_ids[n] = sprite->findSequence(s_seq_names[n]);

		if(seq_ids[state_closed] == -1)
			return false;

		bool can_open_in =
			seq_ids[state_opened_in] != -1 &&
			seq_ids[state_opening_in] != -1 &&
			seq_ids[state_closing_in] != -1;
		bool can_open_out =
			seq_ids[state_opened_out] != -1 &&
			seq_ids[state_opening_out] != -1 &&
			seq_ids[state_closing_out] != -1;

		if((type == DoorTypeId::sliding || type == DoorTypeId::rotating || type == DoorTypeId::rotating_in) && !can_open_in)
			return false;
		if((type == DoorTypeId::rotating || type == DoorTypeId::rotating_out) && !can_open_out)
			return false;
		if(type == DoorTypeId::sliding && can_open_out)
			return false;

		return true;
	}

	Door::Door(Stream &sr) :Entity(sr) {
		sr.unpack(m_type_id, m_close_time, m_update_anim, m_state);
	
		for(int n = 0; n < state_count; n++)
			m_seq_ids[n] = m_sprite->findSequence(s_seq_names[n]);
		setBBox(computeBBox(m_state));
	}

	Door::Door(const XMLNode &node) :Entity(node) {
		initialize(DoorTypeId::fromString(node.attrib("door_type")));
	}
		
	Door::Door(const char *sprite_name, Type type_id, const float3 &pos) :Entity(sprite_name) {
		initialize(type_id);
		setPos(pos);
	}
	
	XMLNode Door::save(XMLNode &parent) const {
		XMLNode node = Entity::save(parent);
		node.addAttrib("door_type", DoorTypeId::toString(m_type_id));
		return node;
	}

	void Door::save(Stream &sr) const {
		Entity::save(sr);
		sr.pack(m_type_id, m_close_time, m_update_anim, m_state);
	}

	void Door::initialize(Door::Type type_id) {
		m_type_id = type_id;
		m_close_time = -1.0;
		m_update_anim = false;
		
		for(int n = 0; n < state_count; n++)
			m_seq_ids[n] = m_sprite->findSequence(s_seq_names[n]);
		DASSERT(testSpriteType(m_sprite, type_id));

		m_state = state_closed;
		playSequence(m_seq_ids[m_state]);
		setBBox(computeBBox(m_state));
	}
		
	void Door::setDirAngle(float angle) {
		Entity::setDirAngle(angle);
		setBBox(computeBBox(m_state));
	}
	
	Entity *Door::clone() const {
		return new Door(*this);
	}
	
	void Door::setKey(const Item &key) {
		DASSERT(!key.isValid() || key.typeId() == ItemTypeId::other);
		m_key = key;
	}

	void Door::interact(const Entity *interactor) {
		State target, result = m_state;
		if(isOpened())
			target = state_closed;
		else {
			if(m_key.isValid()) {
				const Actor *actor = dynamic_cast<const Actor*>(interactor);
				if(!actor || actor->inventory().find(m_key) == -1) {
					printf("Key required!\n");
					return;
				}
			}

			target = m_type_id == DoorTypeId::rotating_out? state_opened_out : state_opened_in;
		}

		for(int n = 0; n < COUNTOF(s_transitions); n++)
			if(s_transitions[n].current == m_state && s_transitions[n].target == target) {
				result = s_transitions[n].result;
				break;
			}
		if(result == m_state)
			return;

		//TODO: open direction should depend on interactor's position		
		FBox bbox = computeBBox(result);
		bool is_colliding = m_world->isColliding(bbox + pos(), this, collider_dynamic | collider_dynamic_nv);

		if(is_colliding && m_type_id == DoorTypeId::rotating && m_state == state_closed && target == state_opened_in) {
			target = state_opened_out;
			result = state_opening_out;
			bbox = computeBBox(result);
			is_colliding = m_world->isColliding(bbox + pos(), this, collider_dynamic | collider_dynamic_nv);
		}
		if(!is_colliding) {
			setBBox(bbox);
			m_state = result;
			m_update_anim = true;
		}
	}
	
	FBox Door::computeBBox(State state) const {	
		float3 size = m_sprite->boundingBox();
		float maxs = max(size.x, size.z);
		
		FBox box;
		if(m_type_id == DoorTypeId::sliding || state == state_closed)
			box = FBox(float3(0, 0, 0), state == state_opened_in? float3(0, 0, 0) : size);
		else if(state == state_closing_in || state == state_opening_in)
			box = FBox(-maxs + 1, 0, 0, 1, size.y, maxs);
		else if(state == state_closing_out || state == state_opening_out)
			box = FBox(0, 0, 0, maxs, size.y, maxs);
		else if(state == state_opened_out)
			box = FBox(0, 0, 0, size.z, size.y, size.x);
		else if(state == state_opened_in)
			box = FBox(-size.z + 1, 0, 0, 1, size.y, size.x);

		//TODO: this is still wrong
		FBox out = rotateY(box, size * 0.5f, dirAngle());
		out.min = (int3)out.min;
		out.max = (int3)out.max;
		DASSERT(m_type_id == DoorTypeId::sliding || !out.isEmpty());
		return out;
	}

	void Door::think() {
		const float2 dir = actualDir();
		
		if(m_update_anim) {
			m_world->needUpdate(this);
			playSequence(m_seq_ids[m_state]);
			m_update_anim = false;
		}
		if(m_type_id == DoorTypeId::sliding && m_state == state_opened_in && m_world->currentTime() > m_close_time) {
			FBox bbox = computeBBox(state_closed);
			if(m_world->isColliding(bbox + pos(), this, collider_dynamic | collider_dynamic_nv)) {
				m_close_time = m_world->currentTime() + 1.5;
			}
			else {
				setBBox(bbox);
				m_state = state_closing_in;
				m_update_anim = true;
			}
		}
	}

	void Door::onAnimFinished() {
		for(int n = 0; n < COUNTOF(s_transitions); n++)
			if(m_state == s_transitions[n].result) {
				m_state = s_transitions[n].target;
				setBBox(computeBBox(m_state));
				m_update_anim = true;
				if(m_state == state_opened_in && m_type_id == DoorTypeId::sliding)
					m_close_time = m_world->currentTime() + 3.0;
				break;
			}
	}

}
