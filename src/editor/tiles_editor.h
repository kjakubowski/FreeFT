#ifndef EDITOR_TILES_EDITOR_H
#define EDITOR_TILES_EDITOR_H

#include "gfx/tile.h"
#include "ui/window.h"
#include "tile_map.h"

class TileGroup;

namespace ui {

	//TODO: simplify code by recreating TilesEditor when reloading map and/or doing something drastic?
	class TilesEditor: public ui::Window
	{
	public:
		TilesEditor(IRect rect);

		void onTileMapReload();
		void setTileMap(TileMap*);
		void setTileGroup(const TileGroup *tile_group) { m_tile_group = tile_group; }
		void setNewTile(const gfx::Tile *tile) { m_new_tile = tile; }
			
		virtual void drawContents() const;
		virtual void onInput(int2 mouse_pos);
		virtual bool onMouseDrag(int2 start, int2 current, int key, int is_final);

		static void drawGrid(const IBox &box, int2 nodeSize, int y);

		enum Mode {
			mode_selecting_normal,
			mode_selecting_union,
			mode_selecting_intersection,
			mode_selecting_difference,

			mode_placing,
			mode_replacing,

			mode_placing_random,
			mode_replacing_random,

			mode_filling,

			mode_occluders,

			mode_count,
		};

		bool isSelecting() const
			{ return m_mode >= mode_selecting_normal && m_mode <= mode_selecting_difference; }
		bool isReplacing() const
			{ return m_mode == mode_replacing || m_mode == mode_replacing_random; }
		bool isPlacing() const
			{ return m_mode >= mode_placing && m_mode <= mode_replacing; }
		bool isPlacingRandom() const
			{ return m_mode >= mode_placing_random && m_mode <= mode_replacing_random; }
		bool isFilling() const
			{ return m_mode == mode_filling; }
		bool isChangingOccluders() const
			{ return m_mode == mode_occluders; }

		void setMode(Mode);
		Mode mode() const { return m_mode; }

		static const char **modeStrings();
		
		float m_dirty_percent;

	private:
		Mode m_mode;

		TileMap *m_tile_map;
		const TileGroup *m_tile_group;
		const gfx::Tile *m_new_tile;
		vector<int> m_selected_ids;
		
		void fill(const IBox &fill_box, bool is_randomized = false, int group_id = -1);
		void fillHoles(int main_group_id, const IBox &fill_box);
		void removeAll(const IBox &box);

		int findAt(const int3 &pos) const;

		void drawBoxHelpers(const IBox &box) const;
		IBox computeCursor(int2 start, int2 end) const;
		void clampViewPos();
		
		int m_cursor_height, m_grid_height;
		IBox m_selection;
		int2 m_view_pos;

		int2 m_grid_size;
		bool m_show_grid, m_is_selecting;

		//TODO: make sure that this occluder wont be saved
		int m_current_occluder;
		int m_mouseover_tile_id;
	};

	typedef Ptr<TilesEditor> PTilesEditor;

}


#endif

