#pragma once

#include <AudioTools.h>
#include <algorithm>
#include <U8g2lib.h>
#include <Arduino.h>
#include <functional>

#include "config.h"



class SettingsScreenU8g2 {
public:
		enum Button : uint8_t { BTN_UP = 0, BTN_DOWN = 1, BTN_LEFT = 2, BTN_RIGHT = 3, BTN_OK = 4, BTN_BACK = 5 };

	// Menu item identifiers
	enum Item : uint8_t { ITEM_ZOOM = 0, ITEM_DELAY_TIME = 1, ITEM_DELAY_DEPTH = 2, ITEM_DELAY_FEEDBACK = 3, ITEM_FILTER_CUTOFF = 4, ITEM_COUNT = 5 };

	explicit SettingsScreenU8g2(U8G2 &display)
		: u8g2(display), active(false), editing(false), zoom(1.0f), dirty(true), lastDrawMs(0) {}

	// Set callbacks that get called whenever a setting value changes.
	void setZoomCallback(std::function<void(float)> cb) { zoomCallback = cb; }
	void setFilterCutoffCallback(std::function<void(float)> cb) { filterCutoffCallback = cb; } // we have to change this to the Q factor 
	void setDelayTimeCallback(std::function<void(float)> cb) { delayTimeCallback = cb; }
	void setDelayDepthCallback(std::function<void(float)> cb) { delayDepthCallback = cb; }
	void setDelayFeedbackCallback(std::function<void(float)> cb) { delayFeedbackCallback = cb; }

	// Call once at startup if you need to initialize anything (kept for API symmetry)
	void begin() {}

	// Called when the settings screen becomes active
	void enter() { active = true; markDirty(); }
	void exit()  { active = false; }
	bool isActive() const { return active; }

	// Draw immediately. Safe to call from the main loop; the class will
	// throttle / debounce itself if necessary using internal dirty flag.
	void draw() {
		if (!active) return;
		// Small throttle: don't redraw faster than ~30fps
		unsigned long now = millis();
		if (!dirty && (now - lastDrawMs) < 33) return;
		lastDrawMs = now;

		u8g2.clearBuffer();
		drawZoom();
		u8g2.sendBuffer();
		dirty = false;
	}

	// Convenience: call from loop() to redraw when active
	void update() { draw(); }

	// Handle a button press. Returns true if the event was consumed.
	bool onButton(Button b) {
		if (!active) return false;
		// Debug: print incoming button events to serial so we can trace input
		Serial.print("SettingsScreen::onButton received: ");
		switch(b) {
			case BTN_UP: Serial.println("BTN_UP"); break;
			case BTN_DOWN: Serial.println("BTN_DOWN"); break;
			case BTN_LEFT: Serial.println("BTN_LEFT"); break;
			case BTN_RIGHT: Serial.println("BTN_RIGHT"); break;
			case BTN_OK: Serial.println("BTN_OK"); break;
			case BTN_BACK: Serial.println("BTN_BACK"); break;
			default: Serial.println("BTN_UNKNOWN"); break;
		}

		switch (b) {
			case BTN_OK:
				// Toggle edit mode
				editing = !editing;
				markDirty();
				return true;

			case BTN_BACK:
				// Exit edit mode (or exit screen if not editing)
				if (editing) {
					editing = false;
					markDirty();
				} else {
					// consumer may call exit()
				}
				return true;

			case BTN_UP:
				if (editing) {
					adjustCurrentItem(+1);
				} else {
					// navigate menu up
					if (selection == 0) selection = ITEM_COUNT - 1; else --selection;
					markDirty();
				}
				return true;

			case BTN_DOWN:
				if (editing) {
					adjustCurrentItem(-1);
				} else {
					// navigate menu down
					selection = (selection + 1) % ITEM_COUNT;
					markDirty();
				}
				return true;

			case BTN_LEFT:
				if (editing) { adjustCurrentItem(-10); }
				return true;

			case BTN_RIGHT:
				if (editing) { adjustCurrentItem(+10); }
				return true;
		}
		return false;
	}

	// Accessors
	float getZoom() const { return zoom; }
	void setZoom(float z) { zoom = z; clampZoom(); markDirty(); notifyZoomChanged(); }

	// Additional accessors for persistence / external sync
	float getDelayTimeMs() const { return delayTimeMs; }
	float getDelayDepth() const { return delayDepth; }
	float getDelayFeedback() const { return delayFeedback; }
	float getFilterCutoffHz() const { return filterCutoffHz; }

	void setDelayTimeMs(float ms) { delayTimeMs = constrain(ms, delayTimeMin, delayTimeMax); markDirty(); notifyDelayTimeChanged(); }
	void setDelayDepth(float d) { delayDepth = constrain(d, delayDepthMin, delayDepthMax); markDirty(); notifyDelayDepthChanged(); }
	void setDelayFeedback(float fb) { delayFeedback = constrain(fb, delayFeedbackMin, delayFeedbackMax); markDirty(); notifyDelayFeedbackChanged(); }
	void setFilterCutoffHz(float hz) { filterCutoffHz = constrain(hz, filterCutoffMin, filterCutoffMax); markDirty(); notifyFilterCutoffChanged(); }

private:
	U8G2 &u8g2;
	bool active;
	bool editing;
	float zoom; // zoom factor
	bool dirty;
	unsigned long lastDrawMs;
	std::function<void(float)> zoomCallback;

	// zoom configuration
	static constexpr float zoomMin = 0.5f;
	static constexpr float zoomMax = 40.0f;
	static constexpr float zoomStep = 0.1f;
	static constexpr float zoomBigStep = 0.5f;
	// delay config steps
	static constexpr float delayTimeMin = 50.0f;
	static constexpr float delayTimeMax = 2000.0f;
	static constexpr float delayTimeStep = 10.0f;

	static constexpr float delayDepthMin = 0.0f;
	static constexpr float delayDepthMax = 1.0f;
	static constexpr float delayDepthStep = 0.02f;

	static constexpr float delayFeedbackMin = 0.0f;
	static constexpr float delayFeedbackMax = 0.95f;
	static constexpr float delayFeedbackStep = 0.02f;

	static constexpr float filterCutoffMin = 20.0f;
	static constexpr float filterCutoffMax = 20000.0f;
	static constexpr float filterCutoffStep = 50.0f;

	uint8_t selection = 0; // which menu item is selected
	// other setting values
	float delayTimeMs = 420.0f;
	float delayDepth = 0.40f;
	float delayFeedback = 0.45f;
	float filterCutoffHz = 1000.0f;

	std::function<void(float)> filterCutoffCallback;
	std::function<void(float)> delayTimeCallback;
	std::function<void(float)> delayDepthCallback;
	std::function<void(float)> delayFeedbackCallback;

	void clampZoom() {
		if (zoom < zoomMin) zoom = zoomMin;
		if (zoom > zoomMax) zoom = zoomMax;
	}

	void markDirty() { dirty = true; }

	void notifyZoomChanged() {
		if (zoomCallback) zoomCallback(zoom);
	}

	void notifyDelayTimeChanged() { if (delayTimeCallback) delayTimeCallback(delayTimeMs); }
	void notifyDelayDepthChanged() { if (delayDepthCallback) delayDepthCallback(delayDepth); }
	void notifyDelayFeedbackChanged() { if (delayFeedbackCallback) delayFeedbackCallback(delayFeedback); }
	void notifyFilterCutoffChanged() { if (filterCutoffCallback) filterCutoffCallback(filterCutoffHz); }

	void adjustCurrentItem(int delta) {
		// delta is in "steps" where +1 is a small step, +10 is coarse
		switch (selection) {
			case ITEM_ZOOM: {
				float step = (delta > 0 ? zoomStep : zoomStep);
				float amount = (delta >= 10 || delta <= -10) ? zoomBigStep * (delta/10) : zoomStep * delta;
				zoom += amount;
				clampZoom();
				markDirty();
				notifyZoomChanged();
				break;
			}
			case ITEM_DELAY_TIME: {
				float amount = (delta >= 10 || delta <= -10) ? delayTimeStep * (delta/10) : delayTimeStep * delta;
				delayTimeMs = constrain(delayTimeMs + amount, delayTimeMin, delayTimeMax);
				markDirty(); notifyDelayTimeChanged(); break;
			}
			case ITEM_DELAY_DEPTH: {
				float amount = (delta >= 10 || delta <= -10) ? delayDepthStep * (delta/10) : delayDepthStep * delta;
				delayDepth = constrain(delayDepth + amount, delayDepthMin, delayDepthMax);
				markDirty(); notifyDelayDepthChanged(); break;
			}
			case ITEM_DELAY_FEEDBACK: {
				float amount = (delta >= 10 || delta <= -10) ? delayFeedbackStep * (delta/10) : delayFeedbackStep * delta;
				delayFeedback = constrain(delayFeedback + amount, delayFeedbackMin, delayFeedbackMax);
				markDirty(); notifyDelayFeedbackChanged(); break;
			}
			case ITEM_FILTER_CUTOFF: {
				float amount = (delta >= 10 || delta <= -10) ? filterCutoffStep * (delta/10) : filterCutoffStep * delta;
				filterCutoffHz = constrain(filterCutoffHz + amount, filterCutoffMin, filterCutoffMax);
				markDirty(); notifyFilterCutoffChanged(); break;
			}
		}
	}


	void drawZoom() {
		// Draw the full menu (label + all items) — this function is now the
		// primary menu renderer.
		u8g2.setFont(u8g2_font_6x12_tr);
		const char* labels[ITEM_COUNT] = {"Zoom","Delay ms","Delay depth","Delay fb","Filter Hz"};
		for (int i = 0; i < ITEM_COUNT; ++i) {
			int y = 22 + i * 10;
			// highlight selected
			if (i == selection) {
				// invert rect for visibility
				u8g2.drawBox(0, y - 10, u8g2.getDisplayWidth(), 12);
				u8g2.setDrawColor(0);
			} else {
				u8g2.setDrawColor(1);
			}
			u8g2.setFont(u8g2_font_6x12_tr);
			char labelBuf[24];
			if (editing && i == selection) {
				snprintf(labelBuf, sizeof(labelBuf), "* %s", labels[i]);
			} else {
				snprintf(labelBuf, sizeof(labelBuf), "  %s", labels[i]);
			}
			u8g2.drawStr(4, y, labelBuf);
			// draw value aligned right
			char valbuf[24];
			switch (i) {
				case ITEM_ZOOM: snprintf(valbuf, sizeof(valbuf), "%.1fx", zoom); break;
				case ITEM_DELAY_TIME: snprintf(valbuf, sizeof(valbuf), "%.0fms", delayTimeMs); break;
				case ITEM_DELAY_DEPTH: snprintf(valbuf, sizeof(valbuf), "%.2f", delayDepth); break;
				case ITEM_DELAY_FEEDBACK: snprintf(valbuf, sizeof(valbuf), "%.2f", delayFeedback); break;
				case ITEM_FILTER_CUTOFF: snprintf(valbuf, sizeof(valbuf), "%.0fHz", filterCutoffHz); break;
			}
			int vx = u8g2.getDisplayWidth() - (int)strlen(valbuf) * 6 - 4;
			u8g2.drawStr(vx, y, valbuf);
			// restore draw color
			u8g2.setDrawColor(1);
		}


	}


};
