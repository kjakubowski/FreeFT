/* Copyright (C) 2013-2014 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of FreeFT.
 */

#include <memory.h>
#include <cstdio>

#include "gfx/device.h"
#include "gfx/font.h"
#include "gfx/scene_renderer.h"

#include "navi_map.h"
#include "sys/profiler.h"
#include "sys/platform.h"
#include "game/actor.h"
#include "game/world.h"
#include "game/container.h"
#include "game/door.h"
#include "game/item.h"
#include "sys/config.h"
#include "sys/xml.h"
#include "sys/network.h"
#include "sys/host.h"
#include <list>
#include <algorithm>

using namespace gfx;
using namespace game;

float frand() {
	return float(rand()) / float(RAND_MAX);
}
	
using namespace net;


class Server: public net::LocalHost {
public:
	Server(int port) :LocalHost(Address(port)), m_world(0), m_timestamp(0), m_client_count(0) { }

	enum {
		max_clients = 32,
		client_timeout = 10,
	};

	struct ClientInfo {
		BitVector update_map;
		int actor_id;
	};


	int spawnActor(const float3 &pos) {
		DASSERT(m_world);
		return m_world->addEntity(new Actor(ActorTypeId::male, pos));
	}

/*	void disconnectClient(int client_id) {
		Client &client = m_clients[client_id];
		m_world->removeEntity(client.actor_id);
		printf("Client disconnected: %s\n", client.address.toString().c_str());

		client.clear();
		m_client_count--;
	}*/

	void handleUnverifiedHost(RemoteHost &host, ClientInfo &info) {
		DASSERT(!host.isVerified());

		const Chunk *chunk = nullptr;
		EntityMap &emap = m_world->entityMap();

		while( (chunk = host.getIChunk()) ) {
			if(chunk->type == ChunkType::join) {
				info.actor_id = spawnActor(float3(245 + frand() * 10.0f, 128, 335 + frand() * 10.0f));
//				printf("Client connected (cid:%d): %s\n", (int)r, host.address().toString().c_str());

				info.update_map.resize(emap.size() * 2);
				for(int n = 0; n < emap.size(); n++)
					if(emap[n].ptr)
						info.update_map[n] = true;

				TempPacket temp;
				temp << JoinAcceptPacket{string(m_world->mapName()), info.actor_id};
				host.enqueChunk(temp, ChunkType::join_accept, 0);
			}
			if(chunk->type == ChunkType::join_complete) {
				m_client_count++;
				host.verify(true);
				break;
			}
		}
	}

	void handleHost(RemoteHost &host, ClientInfo &info) {
		DASSERT(host.isVerified());
		const Chunk *chunk = nullptr;

		while( (chunk = host.getIChunk()) ) {
			if(chunk->type == ChunkType::leave) {
				//TODO: removeHost
				break;
			}
			else if(chunk->type == ChunkType::actor_order) {
				MemoryLoader ldr(chunk->data.data(), chunk->data.size());

				Actor *actor = dynamic_cast<Actor*>(m_world->getEntity(info.actor_id));
				Order order;
				ldr >> order;
				if(order.isValid())
					actor->setNextOrder(order);
				else
					printf("Invalid order!\n");
			}
		}
		
		EntityMap &emap = m_world->entityMap();
		const vector<int> &new_updates = m_world->replicationList();
			
		for(int n = 0; n < (int)new_updates.size(); n++)
			info.update_map[new_updates[n]] = true;
		vector<int> &lost = host.lostUChunks();
		for(int n = 0; n < (int)lost.size(); n++)
			info.update_map[lost[n]] = true;
		lost.clear();

		//TODO: apply priorities to entities
		//TODO: priority for objects visible by client
			
		int idx = 0;
		BitVector &map = info.update_map;
		while(idx < emap.size()) {
			if(!map.any(idx >> BitVector::base_shift)) {
				idx = ((idx >> BitVector::base_shift) + 1) << BitVector::base_shift;
				continue;
			}

			if(map[idx]) {
				const Entity *entity = emap[idx].ptr;

				TempPacket temp;

				if(entity) {
					temp << entity->entityType() << *entity;
				}

				if(host.enqueUChunk(temp, entity? ChunkType::entity_full : ChunkType::entity_delete, idx, 1))
					map[idx] = false;
				else
					break;
			}

			idx++;
		}
	}

	void action() {
		InPacket packet;
		Address source;

		EntityMap &emap = m_world->entityMap();

		double time = getTime();
		m_current_time = time;

		LocalHost::beginFrame();

		for(int r = 0; r < numRemoteHosts(); r++) {
			RemoteHost *host = getRemoteHost(r);
			const Chunk *chunk = nullptr;

			if(!host)
				continue;
				
			if((int)m_client_infos.size() <= r)
				m_client_infos.resize(r + 1);
			ClientInfo &info = m_client_infos[r];
			if(emap.size() > info.update_map.size())
				info.update_map.resize(emap.size() * 2);

			beginSending(r);

			if(host->isVerified())
				handleHost(*host, info);
			else
				handleUnverifiedHost(*host, info);

			finishSending();
		}
		
		LocalHost::finishFrame();
		
		m_world->replicationList().clear();

		//TODO: check timeouts
		m_timestamp++;
	}

	void setWorld(World *world) { m_world = world; }

private:
	vector<ClientInfo> m_client_infos;
	World *m_world;
	int m_timestamp;
	int m_client_count;
	double m_current_time;
};

int safe_main(int argc, char **argv)
{
	int port = 0;
	string map_name = "data/maps/mission05.mod";
	
	srand((int)getTime());

	for(int n = 1; n < argc; n++) {
		if(strcmp(argv[n], "-p") == 0) {
			ASSERT(n + 1 < argc);
			port = atoi(argv[++n]);
		}
		else if(strcmp(argv[n], "-m") == 0) {
			ASSERT(n + 1 < argc);
			map_name = string("data/maps/") + argv[++n];
		}
	}
	if(!port) {
		printf("Port unspecified\n");
		return 0;
	}

	unique_ptr<Server> host(new Server(port));
	
	Config config = loadConfig("server");
	ItemDesc::loadItems();

	createWindow(config.resolution, config.fullscreen);
	setWindowTitle("FreeFT::game; built " __DATE__ " " __TIME__);
	setWindowPos(config.window_pos);
	pollEvents();

	printDeviceInfo();
	grabMouse(false);

	setBlendingMode(bmNormal);

	int2 view_pos(-1000, 500);

	PFont font = Font::mgr["liberation_32"];

	World world(World::Mode::server, map_name.c_str());

	world.updateNaviMap(true);
	host->setWorld(&world);

	double last_time = getTime();
	vector<int3> path;
	int3 last_pos(0, 0, 0);
	float3 target_pos(0, 0, 0);

	int inventory_sel = -1, container_sel = -1;
	string prof_stats;
	double stat_update_time = getTime();

	while(pollEvents()) {
		double loop_start = profiler::getTime();
		if(isKeyDown(Key_esc))
			break;

		if((isKeyPressed(Key_lctrl) && isMouseKeyPressed(0)) || isMouseKeyPressed(2))
			view_pos -= getMouseMove();
		
		Ray ray = screenRay(getMousePos() + view_pos);
		Intersection isect = world.pixelIntersect(getMousePos() + view_pos, collider_all|visibility_flag);
		if(isect.isEmpty() || isect.isTile())
			isect = world.trace(ray, nullptr, collider_all|visibility_flag);

		double time = getTime();

		world.updateNaviMap(false);
		world.simulate((time - last_time) * config.time_multiplier);
		last_time = time;

		static int counter = 0;
		if(host)// && counter % 30 == 0)
			host->action();
		counter++;
		

		clear(Color(128, 64, 0));
		SceneRenderer renderer(IRect(int2(0, 0), config.resolution), view_pos);
		
		if(!isect.isEmpty()) {		
			FBox box = isect.boundingBox();
			renderer.addBox(box, Color(255, 255, 255, 100));
		}

	//	world.updateVisibility(actor->boundingBox());
		world.addToRender(renderer);

		renderer.render();
		lookAt(view_pos);
			
		lookAt({0, 0});
		drawLine(getMousePos() - int2(5, 0), getMousePos() + int2(5, 0));
		drawLine(getMousePos() - int2(0, 5), getMousePos() + int2(0, 5));

		DTexture::bind0();
		drawQuad(0, 0, 250, config.profiler_enabled? 300 : 50, Color(0, 0, 0, 80));
		
		gfx::PFont font = gfx::Font::mgr["liberation_16"];
		float3 isect_pos = ray.at(isect.distance());
		font->drawShadowed(int2(0, 0), Color::white, Color::black,
				"View:(%d %d)\nRay:(%.2f %.2f %.2f)\n",
				view_pos.x, view_pos.y, isect_pos.x, isect_pos.y, isect_pos.z);
		if(config.profiler_enabled)
			font->drawShadowed(int2(0, 60), Color::white, Color::black, "%s", prof_stats.c_str());

		swapBuffers();
		TextureCache::main_cache.nextFrame();

		profiler::updateTimer("main_loop", profiler::getTime() - loop_start);
		if(getTime() - stat_update_time > 0.25) {
			prof_stats = profiler::getStats();
			stat_update_time = getTime();
		}
		profiler::nextFrame();
	}

	delete host.release();
	destroyWindow();

	return 0;
}

int main(int argc, char **argv) {
	try {
		return safe_main(argc, argv);
	}
	catch(const Exception &ex) {
		destroyWindow();
		printf("%s\n\nBacktrace:\n%s\n", ex.what(), cppFilterBacktrace(ex.backtrace()).c_str());
		return 1;
	}
}

