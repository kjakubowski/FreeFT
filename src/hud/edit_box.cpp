/* Copyright (C) 2013-2014 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of FreeFT.
 */

#include "hud/edit_box.h"
#include "game/actor.h"
#include "game/world.h"

using namespace gfx;

namespace hud {

	HudEditBox::HudEditBox(const FRect &rect, int max_size, EditMode mode, int id)
		:HudButton(rect, id), m_cursor_pos(0), m_max_size(max_size), m_mode(mode) {
		setLabelStyle(HudLabelStyle::disabled);
	}

	void HudEditBox::setText(const string &text) {
		m_text = text;
		m_old_text = m_text;
		m_cursor_pos = min(m_cursor_pos, (int)m_text.size());
	}

	void HudEditBox::onUpdate(double time_diff) {
		HudButton::onUpdate(time_diff);
		m_show_time = m_is_visible? m_show_time + time_diff : 0.0f;
	}
		
	void HudEditBox::onInputFocus(bool is_focused) {
		setEnabled(is_focused);
	}

	bool HudEditBox::onInput(const InputEvent &event) {
		bool handled = false;

		if(m_mode == mode_console && event.keyDown('`'))
			return false;

		bool mouse_over = isMouseOver(event);
		if(event.mouseOver())
			m_is_highlighted = mouse_over;

		if(!isEnabled() && event.mouseKeyDown(0) && !isGreyed() && mouse_over) {
			setInputFocus(true);
			handled = true;
			m_old_text = m_text;
		}

		if(isEnabled()) {
			if(event.keyDown(InputKey::enter) || event.keyDown(InputKey::esc) || (event.mouseKeyDown(0) && !mouse_over)) {
				if(event.keyDown(InputKey::esc))
					m_text = m_old_text;
				setInputFocus(false);
				handleEvent(this, HudEvent::text_modified, m_id);
				handled = true;
			}
			if(isOneOf(event.type(), InputEvent::key_down_auto, InputEvent::key_down) && isEnabled()) {
				int key = event.key();
				if(event.keyDownAuto(key, 2) || event.keyDown(key))
					return onKey(event.keyChar());
				return true;
			}
			if(event.mouseKeyPressed(0) && mouse_over) {
				setCursorPos(event.mousePos() - rect().min);
			}
		}

		return handled;
	}
		
	bool HudEditBox::isValidChar(int key) {
		if(m_mode == mode_normal || m_mode == mode_console)
		   return key >= 32 && key < 128;
		else if(m_mode == mode_nick || m_mode == mode_locase_nick)
			return (key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9') || key == '-' || key == '_';

		return false;
	}

	bool HudEditBox::onKey(int key) {
		if(key == InputKey::end)
			m_cursor_pos = (int)m_text.size();
		else if(key == InputKey::home)
			m_cursor_pos = 0;
		else if(key == InputKey::right && m_cursor_pos < (int)m_text.size())
			m_cursor_pos++;
		else if(key == InputKey::left && m_cursor_pos > 0)
			m_cursor_pos--;
		else if(key == InputKey::del && (int)m_text.size() > m_cursor_pos)
			m_text.erase(m_cursor_pos, 1);
		else if(key == InputKey::backspace && m_cursor_pos > 0 && !m_text.empty())
			m_text.erase(--m_cursor_pos, 1);
		else if((int)m_text.size() < m_max_size && isValidChar(key))
			m_text.insert(m_cursor_pos++, 1, m_mode == mode_locase_nick? tolower(key) : key);

		return true;
	}

	void HudEditBox::setCursorPos(const float2 &rect_pos) {
		m_cursor_pos = (int)m_text.size();

		//TODO: speed up
		for(int n = 0; n < (int)m_text.size(); n++) {
			FRect ext = evalExtents(m_text.substr(0, n));
			if(ext.max.x + 5.0f > rect_pos.x - layer_spacing) {
				m_cursor_pos = max(0, n);
				break;
			}
		}
	}
		
	const FRect HudEditBox::evalExtents(const string &text) const {
		return FRect(m_font->evalExtents(m_label + text));
	}

	void HudEditBox::onDraw() const {
		HudButton::onDraw();

		int line_height = m_font->lineHeight();

		FRect target_rect = m_font->draw(rect() + float2(layer_spacing, 0.0f), {textColor(), textShadowColor(), HAlign::left, VAlign::center},
				format("%s%s", m_label.c_str(), m_text.c_str()));

		DTexture::unbind();
		
		int tick = ((int)(m_show_time / 0.4)) % 3;
		if(isEnabled() && tick <= 1) {
			FRect ext = evalExtents(m_text.substr(0, m_cursor_pos));
			int2 pos = (int2)target_rect.min + int2(ext.max.x + 2.0f, 0);
			drawLine(pos, pos + int2(0, line_height), Color(255, 255, 255, 180));
		}

	}

}
