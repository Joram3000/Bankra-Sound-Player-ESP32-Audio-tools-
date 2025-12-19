#pragma once

#include <Arduino.h>
#include <functional>

// Interface implemented by concrete settings screen backends. Provides
// a common surface so the rest of the firmware can remain agnostic
// of the display library that renders the UI.
class ISettingsScreen {
public:
	// Logical button roles shared by the physical button mapper.
	enum class Button : uint8_t { Back = 0, Up = 1, Ok = 2, Left = 3, Down = 4, Right = 5 };

	// Aggregated callbacks to reduce interface surface and keep backends simpler.
	struct Callbacks {
		std::function<void(float)> onZoom;
		std::function<void(float)> onFilterCutoff;
		std::function<void(float)> onFilterQ;
		std::function<void(float)> onFilterSlew;
		std::function<void(float)> onDelayTime;
		std::function<void(float)> onDelayDepth;
		std::function<void(float)> onDelayFeedback;
		std::function<void(float)> onDryMix;
		std::function<void(float)> onWetMix;
		std::function<void(float)> onCompressorAttack;
		std::function<void(float)> onCompressorRelease;
		std::function<void(float)> onCompressorHold;
		std::function<void(float)> onCompressorThreshold;
		std::function<void(float)> onCompressorRatio;
		std::function<void(bool)> onCompressorEnabled;
	};

	virtual ~ISettingsScreen() = default;

	virtual void begin() = 0;
	virtual void enter() = 0;
	virtual void exit() = 0;
	virtual bool isActive() const = 0;
	virtual void update() = 0;
	virtual bool onButton(Button button) = 0;

	virtual void setCallbacks(const Callbacks& callbacks) = 0;

	virtual float getZoom() const = 0;
	virtual float getDelayTimeMs() const = 0;
	virtual float getDelayDepth() const = 0;
	virtual float getDelayFeedback() const = 0;
	virtual float getFilterCutoffHz() const = 0;
	virtual float getFilterQ() const = 0;
	virtual float getFilterSlewHzPerSec() const = 0;
	virtual float getDryMix() const = 0;
	virtual float getWetMix() const = 0;
	virtual bool getCompressorEnabled() const = 0;
	virtual float getCompressorAttackMs() const = 0; 
	virtual float getCompressorReleaseMs() const = 0;
	virtual float getCompressorHoldMs() const = 0;
	virtual float getCompressorThresholdPercent() const = 0;
	virtual float getCompressorRatio() const = 0;

	virtual void setZoom(float zoom) = 0;
	virtual void setDelayTimeMs(float ms) = 0;
	virtual void setDelayDepth(float depth) = 0;
	virtual void setDelayFeedback(float feedback) = 0;
	virtual void setFilterCutoffHz(float hz) = 0;
	virtual void setFilterQ(float q) = 0;
	virtual void setFilterSlewHzPerSec(float hz) = 0;
	virtual void setDryMix(float mix) = 0;
	virtual void setWetMix(float mix) = 0;
	virtual void setCompressorEnabled(bool enabled) = 0;
	virtual void setCompressorAttackMs(float ms) = 0;
	virtual void setCompressorReleaseMs(float ms) = 0;
	virtual void setCompressorHoldMs(float ms) = 0;
	virtual void setCompressorThresholdPercent(float pct) = 0;
	virtual void setCompressorRatio(float ratio) = 0;
};
