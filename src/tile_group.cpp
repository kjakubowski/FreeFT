#include "tile_group.h"


void TileGroup::addTile(const gfx::Tile* tile) {
	DAssert(tile);

	Entry new_entry;
	new_entry.m_tiles.push_back(tile);
	m_entries.push_back(new_entry);
}

void TileGroup::removeTile(const gfx::Tile* tile) {
	for(int e = 0; e < size(); e++) {
		Entry &entry = m_entries[e];
		for(int t = 0; t < (int)entry.m_tiles.size(); t++) 
			if(entry.m_tiles[t] == tile) {
				entry.m_tiles[t] = entry.m_tiles.back();
				entry.m_tiles.pop_back();
				if(entry.m_tiles.empty())
					removeEntry(e);
				return;
			}
	}
}

int TileGroup::findEntry(const gfx::Tile *tile) const {
	for(int e = 0; e < size(); e++) {
		const Entry &entry = m_entries[e];
		for(int t = 0; t < (int)entry.m_tiles.size(); t++) 
			if(entry.m_tiles[t] == tile)
				return e;
	}

	return -1;
}

void TileGroup::removeEntry(int entry_id) {
	DAssert(entry_id < size() && entry_id >= 0);

	for(int n = 0; n < size(); n++) {
		Entry &entry = m_entries[n];
		if(n == entry_id)
			continue;

		for(int m = 0; m < (int)entry.m_matches.size(); m++) {
			Match &match = entry.m_matches[m];
			if(match.m_entry_id == entry_id) {
				entry.m_matches[m--] = entry.m_matches.back();
				entry.m_matches.pop_back();
			}
			else if(match.m_entry_id > entry_id)
				match.m_entry_id--;
		}
	}

	m_entries.erase(m_entries.begin() + entry_id);
}

void TileGroup::mergeEntries(int src_entry_id, int target_entry_id) {
	DAssert(src_entry_id >= 0 && target_entry_id >= 0 && src_entry_id < size() && target_entry_id < size());
	if(src_entry_id == target_entry_id)
		return;

	Entry &target = m_entries[target_entry_id];
	Entry &source = m_entries[src_entry_id];

	target.m_tiles.insert(target.m_tiles.end(), source.m_tiles.begin(), source.m_tiles.end());
	target.m_matches.insert(target.m_matches.end(), source.m_matches.begin(), source.m_matches.end());

	removeEntry(src_entry_id);
}

void TileGroup::addMatch(int entry_id, int matched_entry_id, int3 offset) {
	DAssert(entry_id < size() && matched_entry_id < size() && entry_id >= 0 && matched_entry_id >= 0);	
	m_entries[entry_id].m_matches.push_back(Match{matched_entry_id, offset});
}
	
void TileGroup::removeMatch(int entry_id, int match_id) {
	DAssert(entry_id < size() && entry_id >= 0);

	vector<Match> &matches = m_entries[entry_id].m_matches;
	DAssert(match_id >= 0 && match_id < matches.size());

	matches[match_id] = matches.back();
	matches.pop_back();
}

void TileGroup::clearMatches(int entry_id) {
	DAssert(entry_id < size() && entry_id >= 0);
	m_entries[entry_id].m_matches.clear();
}

void TileGroup::clear() {
	m_entries.clear();
}

using namespace rapidxml;

static void addAttribute(XMLNode *node, const char *name, int value) {
	XMLDocument *doc = node->document();
	char str_value[32];
	sprintf(str_value, "%d", value);
	XMLAttribute *attrib = doc->allocate_attribute(name, doc->allocate_string(str_value));
	node->append_attribute(attrib);
}

static int getIntAttribute(XMLNode *node, const char *name) {
	XMLAttribute *attrib = node->first_attribute(name);
	return attrib? atoi(attrib->value()) : 0;
}

void TileGroup::saveToXML(XMLDocument &doc) const {
	for(int n = 0; n < size(); n++) {
		const Entry &entry = m_entries[n];
		XMLNode *entry_node = doc.allocate_node(node_element, "entry");
		doc.append_node(entry_node);

		for(int t = 0; t < (int)entry.m_tiles.size(); t++) {
			XMLNode *tile_node = doc.allocate_node(node_element, "tile",
					doc.allocate_string(entry.m_tiles[t]->name.c_str()));
			entry_node->append_node(tile_node);
		}

		for(int m = 0; m < (int)entry.m_matches.size(); m++) {
			XMLNode *match_node = doc.allocate_node(node_element, "match");
			entry_node->append_node(match_node);

			addAttribute(match_node, "entry_id", entry.m_matches[m].m_entry_id);
			addAttribute(match_node, "offset_x", entry.m_matches[m].m_offset.x);
			addAttribute(match_node, "offset_y", entry.m_matches[m].m_offset.y);
			addAttribute(match_node, "offset_z", entry.m_matches[m].m_offset.z);
		}
	}
}

void TileGroup::loadFromXML(const XMLDocument &doc, const vector<gfx::Tile> &tiles) {
	clear();

	XMLNode *node = doc.first_node("entry");
	std::map<string, int> tile_map;
	for(int n = 0; n < (int)tiles.size(); n++)
		tile_map[tiles[n].name] = n;

	while(node) {
		Entry entry;
		XMLNode *tile_node = node->first_node("tile");
		while(tile_node) {
			const char *tile_name = tile_node->value();
			auto it = tile_map.find(tile_name);
			if(it != tile_map.end())
				entry.m_tiles.push_back(&tiles[it->second]);
			tile_node = tile_node->next_sibling("tile");
		}

		XMLNode *match_node = node->first_node("match");
		while(match_node) {
			Match match;
			match.m_entry_id = getIntAttribute(match_node, "entry_id");
			match.m_offset.x = getIntAttribute(match_node, "offset_x");
			match.m_offset.y = getIntAttribute(match_node, "offset_y");
			match.m_offset.z = getIntAttribute(match_node, "offset_z");
			entry.m_matches.push_back(match);
			match_node = match_node->next_sibling("match");
		}

		if(!entry.m_tiles.empty())
			m_entries.push_back(entry);

		node = node->next_sibling("entry");
	}

	for(int n = 0; n < size(); n++) {
		Entry &entry = m_entries[n];
		for(int m = 0; m < (int)entry.m_matches.size(); m++)
			if(entry.m_matches[m].m_entry_id < 0 || entry.m_matches[m].m_entry_id >= size()) {
				entry.m_matches[m--] = entry.m_matches.back();
				entry.m_matches.pop_back();
			}
	}
}
