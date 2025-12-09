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

	enum Item : uint8_t {
		ITEM_ZOOM = 0,
		ITEM_DELAY_TIME,
		ITEM_DELAY_DEPTH,
		ITEM_DELAY_FEEDBACK,
		ITEM_FILTER_CUTOFF,
		ITEM_FILTER_Q,
		ITEM_FILTER_SLEW,
		ITEM_DRY_MIX,
		ITEM_WET_MIX,
		ITEM_COMP_ATTACK,
		ITEM_COMP_RELEASE,
		ITEM_COMP_HOLD,
		ITEM_COMP_THRESHOLD,
		ITEM_COMP_RATIO,
		ITEM_COUNT
	};

	explicit SettingsScreenU8g2(U8G2 &display)
		: u8g2(display) {}

	void setZoomCallback(std::function<void(float)> cb) { zoomCallback = cb; }
	void setFilterCutoffCallback(std::function<void(float)> cb) { filterCutoffCallback = cb; }
	void setFilterQCallback(std::function<void(float)> cb) { filterQCallback = cb; }
	void setFilterSlewCallback(std::function<void(float)> cb) { filterSlewCallback = cb; }
	void setDelayTimeCallback(std::function<void(float)> cb) { delayTimeCallback = cb; }
	void setDelayDepthCallback(std::function<void(float)> cb) { delayDepthCallback = cb; }
	void setDelayFeedbackCallback(std::function<void(float)> cb) { delayFeedbackCallback = cb; }
	void setDryMixCallback(std::function<void(float)> cb) { dryMixCallback = cb; }
	void setWetMixCallback(std::function<void(float)> cb) { wetMixCallback = cb; }
	void setCompressorAttackCallback(std::function<void(float)> cb) { compAttackCallback = cb; }
	void setCompressorReleaseCallback(std::function<void(float)> cb) { compReleaseCallback = cb; }
	void setCompressorHoldCallback(std::function<void(float)> cb) { compHoldCallback = cb; }
	void setCompressorThresholdCallback(std::function<void(float)> cb) { compThresholdCallback = cb; }
	void setCompressorRatioCallback(std::function<void(float)> cb) { compRatioCallback = cb; }

	void begin() {}

	void enter() { active = true; markDirty(); }
	void exit()  { active = false; }
	bool isActive() const { return active; }

	void draw() {
		if (!active) return;
		unsigned long now = millis();
		if (!dirty && (now - lastDrawMs) < 33) return;
		lastDrawMs = now;
		u8g2.clearBuffer();
		drawMenu();
		u8g2.sendBuffer();
		dirty = false;
	}

	void update() { draw(); }

	bool onButton(Button b) {
		if (!active) return false;
		switch(b) {
			case BTN_OK:
				editing = !editing;
				markDirty();
				return true;
			case BTN_BACK:
				if (editing) {
					editing = false;
					markDirty();
				}
				return true;
			case BTN_UP:
				if (editing) {
					adjustCurrentItem(+1);
				} else {
					if (selection == 0) selection = ITEM_COUNT - 1; else --selection;
					markDirty();
				}
				return true;
			case BTN_DOWN:
				if (editing) {
					adjustCurrentItem(-1);
				} else {
					selection = (selection + 1) % ITEM_COUNT;
					markDirty();
				}
				return true;
			case BTN_LEFT:
				if (editing) adjustCurrentItem(-10);
				return true;
			case BTN_RIGHT:
				if (editing) adjustCurrentItem(+10);
				return true;
		}
		return false;
	}

	float getZoom() const { return zoom; }
	void setZoom(float z) { zoom = clampValue(z, ZOOM_MIN, ZOOM_MAX); markDirty(); notifyZoomChanged(); }

	float getDelayTimeMs() const { return delayTimeMs; }
	float getDelayDepth() const { return delayDepth; }
	float getDelayFeedback() const { return delayFeedback; }
	float getFilterCutoffHz() const { return filterCutoffHz; }
	float getFilterQ() const { return filterQ; }
	float getFilterSlewHzPerSec() const { return filterSlewHzPerSec; }
	float getDryMix() const { return dryMix; }
	float getWetMix() const { return wetMix; }
	float getCompressorAttackMs() const { return compAttackMs; }
	float getCompressorReleaseMs() const { return compReleaseMs; }
	float getCompressorHoldMs() const { return compHoldMs; }
	float getCompressorThresholdPercent() const { return compThresholdPercent; }
	float getCompressorRatio() const { return compRatio; }

	void setDelayTimeMs(float ms) { delayTimeMs = clampValue(ms, DELAY_TIME_MIN_MS, DELAY_TIME_MAX_MS); markDirty(); notifyDelayTimeChanged(); }
	void setDelayDepth(float d) { delayDepth = clampValue(d, DELAY_DEPTH_MIN, DELAY_DEPTH_MAX); markDirty(); notifyDelayDepthChanged(); }
	void setDelayFeedback(float fb) { delayFeedback = clampValue(fb, DELAY_FEEDBACK_MIN, DELAY_FEEDBACK_MAX); markDirty(); notifyDelayFeedbackChanged(); }
	void setFilterCutoffHz(float hz) { filterCutoffHz = clampValue(hz, LOW_PASS_MIN_HZ, LOW_PASS_MAX_HZ); markDirty(); notifyFilterCutoffChanged(); }
	void setFilterQ(float q) { filterQ = clampValue(q, LOW_PASS_Q_MIN, LOW_PASS_Q_MAX); markDirty(); notifyFilterQChanged(); }
	void setFilterSlewHzPerSec(float hz) { filterSlewHzPerSec = clampValue(hz, FILTER_SLEW_MIN_HZ_PER_SEC, FILTER_SLEW_MAX_HZ_PER_SEC); markDirty(); notifyFilterSlewChanged(); }
	void setDryMix(float mix) { dryMix = clampValue(mix, MIXER_DRY_MIN, MIXER_DRY_MAX); markDirty(); notifyDryMixChanged(); }
	void setWetMix(float mix) { wetMix = clampValue(mix, MIXER_WET_MIN, MIXER_WET_MAX); markDirty(); notifyWetMixChanged(); }
	void setCompressorAttackMs(float ms) { compAttackMs = clampValue(ms, MASTER_COMPRESSOR_ATTACK_MIN_MS, MASTER_COMPRESSOR_ATTACK_MAX_MS); markDirty(); notifyCompressorAttackChanged(); }
	void setCompressorReleaseMs(float ms) { compReleaseMs = clampValue(ms, MASTER_COMPRESSOR_RELEASE_MIN_MS, MASTER_COMPRESSOR_RELEASE_MAX_MS); markDirty(); notifyCompressorReleaseChanged(); }
	void setCompressorHoldMs(float ms) { compHoldMs = clampValue(ms, MASTER_COMPRESSOR_HOLD_MIN_MS, MASTER_COMPRESSOR_HOLD_MAX_MS); markDirty(); notifyCompressorHoldChanged(); }
	void setCompressorThresholdPercent(float pct) { compThresholdPercent = clampValue(pct, MASTER_COMPRESSOR_THRESHOLD_MIN, MASTER_COMPRESSOR_THRESHOLD_MAX); markDirty(); notifyCompressorThresholdChanged(); }
	void setCompressorRatio(float ratio) { compRatio = clampValue(ratio, MASTER_COMPRESSOR_RATIO_MIN, MASTER_COMPRESSOR_RATIO_MAX); markDirty(); notifyCompressorRatioChanged(); }

private:
	U8G2 &u8g2;
	bool active = false;
	bool editing = false;
	float zoom = DEFAULT_HORIZ_ZOOM;
	bool dirty = true;
	unsigned long lastDrawMs = 0;
	uint8_t selection = 0;

	float delayTimeMs = DEFAULT_DELAY_TIME_MS;
	float delayDepth = DEFAULT_DELAY_DEPTH;
	float delayFeedback = DEFAULT_DELAY_FEEDBACK;
	float filterCutoffHz = LOW_PASS_CUTOFF_HZ;
	float filterQ = LOW_PASS_Q;
	float filterSlewHzPerSec = FILTER_SLEW_DEFAULT_HZ_PER_SEC;
	float dryMix = MIXER_DEFAULT_DRY_LEVEL;
	float wetMix = MIXER_DEFAULT_WET_LEVEL;
	float compAttackMs = MASTER_COMPRESSOR_ATTACK_MS;
	float compReleaseMs = MASTER_COMPRESSOR_RELEASE_MS;
	float compHoldMs = MASTER_COMPRESSOR_HOLD_MS;
	float compThresholdPercent = MASTER_COMPRESSOR_THRESHOLD_PERCENT;
	float compRatio = MASTER_COMPRESSOR_RATIO;

	std::function<void(float)> zoomCallback;
	std::function<void(float)> filterCutoffCallback;
	std::function<void(float)> filterQCallback;
	std::function<void(float)> filterSlewCallback;
	std::function<void(float)> delayTimeCallback;
	std::function<void(float)> delayDepthCallback;
	std::function<void(float)> delayFeedbackCallback;
	std::function<void(float)> dryMixCallback;
	std::function<void(float)> wetMixCallback;
	std::function<void(float)> compAttackCallback;
	std::function<void(float)> compReleaseCallback;
	std::function<void(float)> compHoldCallback;
	std::function<void(float)> compThresholdCallback;
	std::function<void(float)> compRatioCallback;

	void markDirty() { dirty = true; }

	void notifyZoomChanged() { if (zoomCallback) zoomCallback(zoom); }
	void notifyDelayTimeChanged() { if (delayTimeCallback) delayTimeCallback(delayTimeMs); }
	void notifyDelayDepthChanged() { if (delayDepthCallback) delayDepthCallback(delayDepth); }
	void notifyDelayFeedbackChanged() { if (delayFeedbackCallback) delayFeedbackCallback(delayFeedback); }
	void notifyFilterCutoffChanged() { if (filterCutoffCallback) filterCutoffCallback(filterCutoffHz); }
	void notifyFilterQChanged() { if (filterQCallback) filterQCallback(filterQ); }
	void notifyFilterSlewChanged() { if (filterSlewCallback) filterSlewCallback(filterSlewHzPerSec); }
	void notifyDryMixChanged() { if (dryMixCallback) dryMixCallback(dryMix); }
	void notifyWetMixChanged() { if (wetMixCallback) wetMixCallback(wetMix); }
	void notifyCompressorAttackChanged() { if (compAttackCallback) compAttackCallback(compAttackMs); }
	void notifyCompressorReleaseChanged() { if (compReleaseCallback) compReleaseCallback(compReleaseMs); }
	void notifyCompressorHoldChanged() { if (compHoldCallback) compHoldCallback(compHoldMs); }
	void notifyCompressorThresholdChanged() { if (compThresholdCallback) compThresholdCallback(compThresholdPercent); }
	void notifyCompressorRatioChanged() { if (compRatioCallback) compRatioCallback(compRatio); }

	void adjustCurrentItem(int delta) {
		auto coarseMult = [](float fine) { return fine * 5.0f; };
		switch (selection) {
			case ITEM_ZOOM:
				applyAdjustment(zoom, delta, ZOOM_MIN, ZOOM_MAX, ZOOM_STEP, ZOOM_BIG_STEP, [this]{ notifyZoomChanged(); });
				break;
			case ITEM_DELAY_TIME:
				applyAdjustment(delayTimeMs, delta, DELAY_TIME_MIN_MS, DELAY_TIME_MAX_MS, DELAY_TIME_STEP_MS, DELAY_TIME_STEP_MS * 10.0f, [this]{ notifyDelayTimeChanged(); });
				break;
			case ITEM_DELAY_DEPTH:
				applyAdjustment(delayDepth, delta, DELAY_DEPTH_MIN, DELAY_DEPTH_MAX, DELAY_DEPTH_STEP, coarseMult(DELAY_DEPTH_STEP), [this]{ notifyDelayDepthChanged(); });
				break;
			case ITEM_DELAY_FEEDBACK:
				applyAdjustment(delayFeedback, delta, DELAY_FEEDBACK_MIN, DELAY_FEEDBACK_MAX, DELAY_FEEDBACK_STEP, coarseMult(DELAY_FEEDBACK_STEP), [this]{ notifyDelayFeedbackChanged(); });
				break;
			case ITEM_FILTER_CUTOFF:
				applyAdjustment(filterCutoffHz, delta, LOW_PASS_MIN_HZ, LOW_PASS_MAX_HZ, LOW_PASS_STEP_HZ, LOW_PASS_STEP_HZ * 10.0f, [this]{ notifyFilterCutoffChanged(); });
				break;
			case ITEM_FILTER_Q:
				applyAdjustment(filterQ, delta, LOW_PASS_Q_MIN, LOW_PASS_Q_MAX, LOW_PASS_Q_STEP, coarseMult(LOW_PASS_Q_STEP), [this]{ notifyFilterQChanged(); });
				break;
			case ITEM_FILTER_SLEW:
				applyAdjustment(filterSlewHzPerSec, delta, FILTER_SLEW_MIN_HZ_PER_SEC, FILTER_SLEW_MAX_HZ_PER_SEC, FILTER_SLEW_STEP_HZ_PER_SEC, FILTER_SLEW_STEP_HZ_PER_SEC * 10.0f, [this]{ notifyFilterSlewChanged(); });
				break;
			case ITEM_DRY_MIX:
				applyAdjustment(dryMix, delta, MIXER_DRY_MIN, MIXER_DRY_MAX, MIXER_DRY_STEP, coarseMult(MIXER_DRY_STEP), [this]{ notifyDryMixChanged(); });
				break;
			case ITEM_WET_MIX:
				applyAdjustment(wetMix, delta, MIXER_WET_MIN, MIXER_WET_MAX, MIXER_WET_STEP, coarseMult(MIXER_WET_STEP), [this]{ notifyWetMixChanged(); });
				break;
			case ITEM_COMP_ATTACK:
				applyAdjustment(compAttackMs, delta, MASTER_COMPRESSOR_ATTACK_MIN_MS, MASTER_COMPRESSOR_ATTACK_MAX_MS, MASTER_COMPRESSOR_ATTACK_STEP_MS, coarseMult(MASTER_COMPRESSOR_ATTACK_STEP_MS), [this]{ notifyCompressorAttackChanged(); });
				break;
			case ITEM_COMP_RELEASE:
				applyAdjustment(compReleaseMs, delta, MASTER_COMPRESSOR_RELEASE_MIN_MS, MASTER_COMPRESSOR_RELEASE_MAX_MS, MASTER_COMPRESSOR_RELEASE_STEP_MS, coarseMult(MASTER_COMPRESSOR_RELEASE_STEP_MS), [this]{ notifyCompressorReleaseChanged(); });
				break;
			case ITEM_COMP_HOLD:
				applyAdjustment(compHoldMs, delta, MASTER_COMPRESSOR_HOLD_MIN_MS, MASTER_COMPRESSOR_HOLD_MAX_MS, MASTER_COMPRESSOR_HOLD_STEP_MS, coarseMult(MASTER_COMPRESSOR_HOLD_STEP_MS), [this]{ notifyCompressorHoldChanged(); });
				break;
			case ITEM_COMP_THRESHOLD:
				applyAdjustment(compThresholdPercent, delta, MASTER_COMPRESSOR_THRESHOLD_MIN, MASTER_COMPRESSOR_THRESHOLD_MAX, MASTER_COMPRESSOR_THRESHOLD_STEP, coarseMult(MASTER_COMPRESSOR_THRESHOLD_STEP), [this]{ notifyCompressorThresholdChanged(); });
				break;
			case ITEM_COMP_RATIO:
				applyAdjustment(compRatio, delta, MASTER_COMPRESSOR_RATIO_MIN, MASTER_COMPRESSOR_RATIO_MAX, MASTER_COMPRESSOR_RATIO_STEP, coarseMult(MASTER_COMPRESSOR_RATIO_STEP), [this]{ notifyCompressorRatioChanged(); });
				break;
		}
	}

	void applyAdjustment(float &value, int delta, float minVal, float maxVal,
					     float fineStep, float coarseStep,
					     const std::function<void(void)> &notifier) {
		if (delta == 0) return;
		bool coarse = (delta >= 10) || (delta <= -10);
		float step = coarse ? coarseStep : fineStep;
		float direction = (delta > 0) ? 1.0f : -1.0f;
		value = clampValue(value + step * direction, minVal, maxVal);
		markDirty();
		if (notifier) notifier();
	}

	void drawMenu() {
		u8g2.setFont(u8g2_font_6x12_tr);
		static const char* const labels[ITEM_COUNT] = {
			"Zoom","Delay ms","Delay depth","Delay fb","Filter Hz",
			"Filter Q","Filter slew","Dry mix","Wet mix",
			"Comp atk","Comp rel","Comp hold","Comp thr","Comp ratio"
		};
		const int rowHeight = 10;
		const int highlightHeight = rowHeight + 2;
		const int menuTop = 12;
		uint8_t visible = SETTINGS_VISIBLE_MENU_ITEMS;
		uint8_t firstIndex = 0;
		if (ITEM_COUNT > visible) {
			if (selection >= visible) {
				firstIndex = selection - visible + 1;
			}
			uint8_t maxFirst = ITEM_COUNT - visible;
			if (firstIndex > maxFirst) firstIndex = maxFirst;
		}
		for (uint8_t row = 0; row < visible; ++row) {
			uint8_t idx = firstIndex + row;
			if (idx >= ITEM_COUNT) break;
			int baseline = menuTop + row * rowHeight;
			if (idx == selection) {
				u8g2.drawBox(0, baseline - rowHeight, u8g2.getDisplayWidth(), highlightHeight);
				u8g2.setDrawColor(0);
			} else {
				u8g2.setDrawColor(1);
			}
			char labelBuf[24];
			if (editing && idx == selection) {
				snprintf(labelBuf, sizeof(labelBuf), "* %s", labels[idx]);
			} else {
				snprintf(labelBuf, sizeof(labelBuf), "  %s", labels[idx]);
			}
			u8g2.drawStr(4, baseline, labelBuf);
			char valbuf[24];
			switch (idx) {
				case ITEM_ZOOM: snprintf(valbuf, sizeof(valbuf), "%.1fx", zoom); break;
				case ITEM_DELAY_TIME: snprintf(valbuf, sizeof(valbuf), "%.0fms", delayTimeMs); break;
				case ITEM_DELAY_DEPTH: snprintf(valbuf, sizeof(valbuf), "%.2f", delayDepth); break;
				case ITEM_DELAY_FEEDBACK: snprintf(valbuf, sizeof(valbuf), "%.2f", delayFeedback); break;
				case ITEM_FILTER_CUTOFF: snprintf(valbuf, sizeof(valbuf), "%.0fHz", filterCutoffHz); break;
				case ITEM_FILTER_Q: snprintf(valbuf, sizeof(valbuf), "%.2f", filterQ); break;
				case ITEM_FILTER_SLEW: snprintf(valbuf, sizeof(valbuf), "%.1fk/s", filterSlewHzPerSec / 1000.0f); break;
				case ITEM_DRY_MIX: snprintf(valbuf, sizeof(valbuf), "%.2f", dryMix); break;
				case ITEM_WET_MIX: snprintf(valbuf, sizeof(valbuf), "%.2f", wetMix); break;
				case ITEM_COMP_ATTACK: snprintf(valbuf, sizeof(valbuf), "%.0fms", compAttackMs); break;
				case ITEM_COMP_RELEASE: snprintf(valbuf, sizeof(valbuf), "%.0fms", compReleaseMs); break;
				case ITEM_COMP_HOLD: snprintf(valbuf, sizeof(valbuf), "%.0fms", compHoldMs); break;
				case ITEM_COMP_THRESHOLD: snprintf(valbuf, sizeof(valbuf), "%.0f%%", compThresholdPercent); break;
				case ITEM_COMP_RATIO: snprintf(valbuf, sizeof(valbuf), "%.2f", compRatio); break;
			}
			int vx = u8g2.getDisplayWidth() - (int)strlen(valbuf) * 6 - 4;
			u8g2.drawStr(vx, baseline, valbuf);
			u8g2.setDrawColor(1);
		}
	}

	static float clampValue(float value, float minValue, float maxValue) {
		if (value < minValue) return minValue;
		if (value > maxValue) return maxValue;
		return value;
	}
};
