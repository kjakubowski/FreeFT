#include "gfx/scene_renderer.h"
#include "gfx/device.h"
#include <algorithm>
#include "sys/profiler.h"

using std::make_pair;
using std::pair;

namespace gfx {


	int SceneRenderer::Element::Compare(const SceneRenderer::Element &rhs) const {
		//DASSERT(!areOverlapping(box, box));

		int y_ret = m_bbox.max.y <= rhs.m_bbox.min.y? -1 : rhs.m_bbox.max.y <= m_bbox.min.y? 1 : 0;
		if(y_ret)
			return y_ret;

		int x_ret = m_bbox.max.x <= rhs.m_bbox.min.x? -1 : rhs.m_bbox.max.x <= m_bbox.min.x? 1 : 0;
		if(x_ret)
			return x_ret;

		int z_ret = m_bbox.max.z <= rhs.m_bbox.min.z? -1 : rhs.m_bbox.max.z <= m_bbox.min.z? 1 : 0;
		return z_ret;
	}

	SceneRenderer::SceneRenderer(IRect viewport, int2 view_pos) :m_viewport(viewport), m_view_pos(view_pos) {
	}

	void SceneRenderer::add(PTexture texture, IRect rect, float3 pos, int3 bbox, Color color) {
		DASSERT(texture);

		rect += (int2)worldToScreen(pos);
		if(!areOverlapping(rect, IRect(m_view_pos, m_view_pos + m_viewport.size())))
			return;

		int3 ipos(pos);
		float eps = 0.0001f;
		int3 frac(pos.x - ipos.x > eps?1 : 0, pos.y - ipos.y > eps? 1 : 0, pos.z - ipos.z > eps? 1 : 0);

		Element new_elem;
		new_elem.m_texture = texture;
		new_elem.m_bbox = IBox(ipos, ipos + bbox + frac);
		new_elem.m_rect = rect;
		new_elem.m_color = color;

		m_elements.push_back(new_elem);
	}

	void SceneRenderer::addBox(IBox bbox, Color color, bool is_filled) {
		if(!is_filled) {
			m_wire_boxes.push_back(BoxElement{bbox, color});
			return;
		}

		Element new_elem;
		new_elem.m_texture = nullptr;
		new_elem.m_bbox = bbox;
		new_elem.m_rect = worldToScreen(bbox);
		new_elem.m_color = color;
		m_elements.push_back(new_elem);
	}


	static int DFS(const vector<char> &graph, vector<pair<int, int> > &gdata, int count, int i, int time) {
		for(int k = 0; k < count; k++) {
			if(k == i || graph[i + k * count] != -1)
				continue;
			if(gdata[k].second)
				continue;
			gdata[k].second = time++;
			time = DFS(graph, gdata, count, k, time);
		}

		gdata[i].first = time++;
		return time;
	}

	void SceneRenderer::render() {
		PROFILE(tRendering)

		setScissorTest(true);
		lookAt(m_view_pos - m_viewport.min);
		IRect view(m_view_pos, m_view_pos + m_viewport.size());

		int xNodes = (m_viewport.width() + node_size - 1) / node_size;
		int yNodes = (m_viewport.height() + node_size - 1) / node_size;

//		std::random_shuffle(m_elements.begin(), m_elements.end());

		// Screen is divided into a set of squares. Rendered elements are assigned to covered
		// squares and sorting is done independently for each of the squares, so that, we can
		// minize problems with rendering order
		vector<std::pair<int, int> > grid;
		grid.reserve(m_elements.size() * 4);

		for(int n = 0; n < (int)m_elements.size(); n++) {
			const Element &elem = m_elements[n];
			IRect rect = elem.m_rect - m_view_pos;

			for(int y = rect.min.y - rect.min.y % node_size; y < rect.max.y; y += node_size)
				for(int x = rect.min.x - rect.min.x % node_size; x < rect.max.x; x += node_size) {
					int grid_x = x / node_size, grid_y = y / node_size;
					if(grid_x >= 0 && grid_y >= 0 && grid_x < xNodes && grid_y < yNodes) {
						int node_id = grid_x + grid_y * xNodes;
						grid.push_back(std::make_pair(node_id, n));
					}
				}
		}

		std::sort(grid.begin(), grid.end());

		// Now we need to do topological sort for a graph in which each edge means
		// than one tile should be drawn before the other; cycles in this graph result
		// in glitches in the end (unavoidable)
		vector<char> graph(1024, 0);
		vector<std::pair<int, int> > gdata(32, make_pair(0, 0));

		for(int g = 0; g < (int)grid.size();) {
			int node_id = grid[g].first;
			int grid_x = node_id % xNodes, grid_y = node_id / xNodes;

			int count = 0;
			while(grid[g + count].first == node_id && g + count < (int)grid.size())
				count++;

			if((int)graph.size() < count * count)
				graph.resize(count * count);
			if((int)gdata.size() < count)
				gdata.resize(count);

			for(int i = 0; i < count; i++) {
				const Element &elem1 = m_elements[grid[g + i].second];

				for(int j = i + 1; j < count; j++) {
					const Element &elem2 = m_elements[grid[g + j].second];
					int result = areOverlapping(elem1.m_rect, elem2.m_rect)? elem1.Compare(elem2): 0;
					graph[i + j * count] = result;
					graph[j + i * count] = -result;
				}
			}

			for(int i = 0; i < count; i++)
				gdata[i] = make_pair(0, 0);

			int time = 1;
			for(int i = 0; i < count; i++) {
				if(gdata[i].second)
					continue;
				gdata[i].second = time++;
				time = DFS(graph, gdata, count, i, time);
			}

			for(int i = 0; i < count; i++)
				gdata[i].second = grid[g + i].second;
			std::sort(&gdata[0], &gdata[0] + count);

			int2 grid_tl = m_viewport.min + int2(grid_x * node_size, grid_y * node_size);
			IRect grid_rect(grid_tl, grid_tl + int2(node_size, node_size));
			grid_rect.max = min(grid_rect.max, m_viewport.max);
			setScissorRect(grid_rect);

			for(int i = count - 1; i >= 0; i--) {
				const Element &elem = m_elements[gdata[i].second];
				if(elem.m_texture) {
					elem.m_texture->bind();
					drawQuad(elem.m_rect.min, elem.m_rect.size(), elem.m_color);
				}
				else {
					DTexture::bind0();
					drawBBoxFilled(elem.m_bbox, elem.m_color);
				}
			}

			g += count;
		}

//		printf("\nGrid overhead: %.2f\n", (double)grid.size() / (double)m_elements.size());

		setScissorRect(m_viewport);
		DTexture::bind0();
		for(int n = 0; n < (int)m_wire_boxes.size(); n++) {
			const BoxElement &elem = m_wire_boxes[n];
			drawBBox(elem.m_bbox, elem.m_color);
		}

		setScissorTest(false);
	}

}