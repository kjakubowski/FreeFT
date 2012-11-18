#ifndef UI_WINDOW_H
#define UI_WINDOW_H

#include "base.h"
#include <memory>

namespace gfx { class Tile; }

namespace ui
{

	class Window;

	typedef Ptr<Window> PWindow;

	struct Event {
		enum Type {
			window_closed,		// value: return value		sending down
			button_clicked,		// value: button id			sending up
			element_clicked,	// value: element id		sending up
			element_selected,	// value: element id		sending up
			popup_close_signal,	// 							sending directly to popup from parent
			escape,				//							sending down
			text_modified,		//							sending up
		};

		Event(Window *source, Type type, int value = 0) :source(source), type(type), value(value) { }

		Window *source;
		Type type;
		int value;
	};

	class Window: public RefCounter
	{
	public:
		Window(IRect rect, Color background = Color::transparent);
		virtual ~Window() { }
		virtual const char *typeName() const { return "Window"; }
		Window(const Window&) = delete;
		void operator=(const Window&) = delete;
		
		virtual void process();
		virtual void draw() const;

		void close(int return_value);

		// normally window is focused when the mouse is over, but it can
		// be overriden by calling this function
		// Popups have higher priority though
		void setFocus(bool set);
		void attach(PWindow, bool as_popup = false);

		// it will also reset inner rect
		void setRect(const IRect &rect);
		IRect rect() const { return m_rect; }
		IRect clippedRect() const { return m_clipped_rect; }
		int width() const { return m_rect.width(); }
		int height() const { return m_rect.height(); }
		int2 size() const { return m_rect.size(); }
		int2 center() const { return m_rect.center(); }

		void setBackgroundColor(Color col);
		Window* parent() const { return m_parent; }
		Window *mainWindow() { return m_parent? m_parent->mainWindow() : nullptr; }

		void setVisible(bool is_visible) { m_is_visible = is_visible; }
		bool isVisible() const { return m_is_visible; }
		bool isFocused() const;

		//TODO: should events be sent to unfocused objects?

		// Sends event up (towards main window) or down the hierarchy
		// returns true if event was received
		bool sendEvent(const Event &event);
		bool sendEvent(Window *source, Event::Type type, int value = 0) {
			return sendEvent(Event(source, type, value));
		}

		// Override this method to receive events
		virtual bool onEvent(const Event &event) { return false; }
		
		static void drawWindow(IRect rect, Color color, int outline);
	
	protected:
		virtual void drawContents() const { }
		virtual void onInput(int2 mouse_pos) { }

		// each on*** function should return true if the event was handled
		// TODO: pass key_modifier along with key (so when user presses LMB with CTRL, it will
		// be passed until LMB is released)
		virtual bool onMouseClick(int2 pos, int key, bool up) { return false; }
		virtual bool onMouseDrag(int2 start, int2 current, int key, bool is_final) { return false; }

		// relative to normal rect
		// IRect(0, 0, rect.width(), rect.height()) is default
		void setInnerRect(const IRect &rect);
		IRect innerRect() const { return m_inner_rect; }

		int2 innerOffset() const { return -m_inner_rect.min; }
		void setInnerOffset(const int2&);

	private:
		void updateRects();

		Window *m_parent;
		vector<PWindow> m_children;

		IRect  m_inner_rect;	// if its bigger than m_rect then progress bars will be added
		IRect m_rect;			// coordinates relative to parent window
		IRect m_clipped_rect;	// absolute coordinates, clipped to parent window
		Color m_background_color;

		int2 m_drag_start;
		int m_closing_value;
		int m_dragging_mode : 4;
		bool m_is_dragging : 1;
		bool m_is_visible : 1;
		bool m_has_inner_rect : 1;
		bool m_is_popup : 1;
		bool m_is_closing : 1;
		bool m_is_focused : 1;
		bool m_has_hard_focus : 1;
	};

}

#endif
