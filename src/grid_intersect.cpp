#include "grid.h"
#include "sys/profiler.h"

int Grid::findAny(const FBox &box, int ignored_id, int flags) const {
	IRect grid_box = nodeCoords(box);

	for(int y = grid_box.min.y; y <= grid_box.max.y; y++)
		for(int x = grid_box.min.x; x <= grid_box.max.x; x++) {
			int node_id = nodeAt(int2(x, y));
			const Node &node = m_nodes[node_id];

			if(!node.size || !(node.obj_flags & flags) || !areOverlapping(box, node.bbox))
				continue;

			const Object *objects[node.size];
			int count = extractObjects(node_id, objects, ignored_id, flags);

			for(int n = 0; n < count; n++)
				if(areOverlapping(box, objects[n]->bbox))
					return objects[n] - &m_objects[0];

			if(node.is_dirty)
				updateNode(node_id);	
		}

	return -1;
}
	
void Grid::findAll(vector<int> &out, const FBox &box, int ignored_id, int flags) const {
	IRect grid_box = nodeCoords(box);

	for(int y = grid_box.min.y; y <= grid_box.max.y; y++)
		for(int x = grid_box.min.x; x <= grid_box.max.x; x++) {
			int node_id = nodeAt(int2(x, y));
			const Node &node = m_nodes[node_id];

			if(!(node.obj_flags & flags) || !areOverlapping(box, node.bbox))
				continue;
			bool anything_found = false;

			const Object *objects[node.size];
			int count = extractObjects(node_id, objects, ignored_id, flags);

			for(int n = 0; n < count; n++)
				if(areOverlapping(box, objects[n]->bbox)) {
					out.push_back(objects[n] - &m_objects[0]);
					anything_found = true;
				}

			if(!anything_found && node.is_dirty)
				updateNode(node_id);	
		}
}

pair<int, float> Grid::trace(const Segment &segment, int ignored_id, int flags) const {
	float tmin = max(segment.min, intersection(segment, m_bounding_box));
	float tmax = min(segment.max, -intersection(-segment, m_bounding_box));
	
	float3 p1 = segment.at(tmin), p2 = segment.at(tmax);
	int2 pos = worldToGrid((int2)p1.xz()), end = worldToGrid((int2)p2.xz());
	
	if(!isInsideGrid(pos) || !isInsideGrid(end))
		return make_pair(-1, constant::inf);

	// Algorithm idea from: RTCD by Christer Ericson
	int dx = end.x > pos.x? 1 : end.x < pos.x? -1 : 0;
	int dz = end.y > pos.y? 1 : end.y < pos.y? -1 : 0;

	float cell_size = (float)node_size;
	float inv_cell_size = 1.0f / cell_size;
	float lenx = fabs(p2.x - p1.x);
	float lenz = fabs(p2.z - p1.z);

	float minx = float(node_size) * floorf(p1.x * inv_cell_size), maxx = minx + cell_size;
	float minz = float(node_size) * floorf(p1.z * inv_cell_size), maxz = minz + cell_size;
	float tx = (p1.x > p2.x? p1.x - minx : maxx - p1.x) / lenx;
	float tz = (p1.z > p2.z? p1.z - minz : maxz - p1.z) / lenz;

	float deltax = cell_size / lenx;
	float deltaz = cell_size / lenz;

	int out = -1;
	float out_dist = tmax;

	while(true) {
		int node_id = nodeAt(pos);
		const Node &node = m_nodes[node_id];

		if((node.obj_flags & flags) && intersection(segment, node.bbox) < out_dist) {
			const Object *objects[node.size];
			int count = extractObjects(node_id, objects, ignored_id, flags);

			for(int n = 0; n < count; n++) {
				float dist = intersection(segment, objects[n]->bbox);
				if(dist < out_dist) {
					out_dist = dist;
					out = objects[n] - &m_objects[0];
				}
			}	
			
			if(node.is_dirty)
				updateNode(node_id);
		}

		if(tx <= tz || dz == 0) {
			if(pos.x == end.x)
				break;
			tx += deltax;
			pos.x += dx;
		}
		else {
			if(pos.y == end.y)
				break;
			tz += deltaz;
			pos.y += dz;
		}
		float ray_pos = tmin + max((tx - deltax) * lenx, (tz - deltaz) * lenz);
		if(ray_pos >= out_dist)
			break;
	}

	return make_pair(out, out_dist);
}

void Grid::findAll(vector<int> &out, const IRect &view_rect) const {
	IRect grid_box(0, 0, m_size.x, m_size.y);

	for(int y = grid_box.min.y; y < grid_box.max.y; y++) {
		const int2 &row_rect = m_row_rects[y];
		if(row_rect.x >= view_rect.max.y || row_rect.y <= view_rect.min.y)
			continue;
		
		for(int x = grid_box.min.x; x < grid_box.max.x; x++) {
			int node_id = nodeAt(int2(x, y));
			const Node &node = m_nodes[node_id];

			if(!areOverlapping(view_rect, node.rect))
				continue;
			bool anything_found = false;
			const Object *objects[node.size];
			int count = extractObjects(node_id, objects);

			for(int n = 0; n < count; n++)
				if(areOverlapping(view_rect, objects[n]->rect)) {
					out.push_back(objects[n] - &m_objects[0]);
					anything_found = true;
				}

			if(!anything_found && node.is_dirty)
				updateNode(node_id);	
		}
	}
}

int Grid::pixelIntersect(const int2 &screen_pos, bool (*pixelTest)(const ObjectDef&, const int2&)) const {
	IRect grid_box(0, 0, m_size.x, m_size.y);
	
	int best = -1;
	FBox best_box;

	for(int y = grid_box.min.y; y < grid_box.max.y; y++) {
		const int2 &row_rect = m_row_rects[y];
		if(row_rect.x >= screen_pos.y || row_rect.y <= screen_pos.y)
			continue;

		for(int x = grid_box.min.x; x < grid_box.max.x; x++) {
			int node_id = nodeAt(int2(x, y));
			const Node &node = m_nodes[node_id];

			if(!node.rect.isInside(screen_pos))
				continue;
			if(node.is_dirty)
				updateNode(node_id);	

			const Object *objects[node.size];
			int count = extractObjects(node_id, objects);

			for(int n = 0; n < count; n++)
				if(objects[n]->rect.isInside(screen_pos) && pixelTest(*objects[n], screen_pos)) {
					if(best == -1 || drawingOrder(objects[n]->bbox, best_box) == 1) {
						best = objects[n] - &m_objects[0];
						best_box = objects[n]->bbox;
					}
				}
		}
	}

	return best;
}
