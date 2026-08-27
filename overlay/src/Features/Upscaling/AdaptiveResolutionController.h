#pragma once

#include <cstdint>

namespace Adaptive80
{
	enum class State : std::uint8_t
	{
		Disabled,
		Quality,
		Target,
		Rescue,
		Emergency,
		CpuLimited,
		Stabilizing,
		Paused
	};

	enum class BoundState : std::uint8_t
	{
		Unknown,
		Gpu,
		Mixed,
		Cpu
	};

	struct Settings
	{
		float targetFrameTimeMs = 25.0f;
		float minScale = 0.52f;
		float maxScale = 0.70f;
		float emergencyMinScale = 0.44f;
		float attackScalePerSecond = 0.80f;
		float recoveryScalePerSecond = 0.04f;
		float gpuHeadroom = 0.90f;
		float resolutionStep = 0.04f;
		float holdSeconds = 0.28f;
		float targetHoldSeconds = 0.80f;
		bool cpuGuard = true;
	};

	struct ScaleBounds
	{
		float minScale = 1.0f;
		float maxScale = 1.0f;
		float emergencyMinScale = 1.0f;
		bool clampedByProvider = false;
	};

	struct Sample
	{
		float frameTimeMs = 0.0f;
		float gpuTimeMs = 0.0f;
		float gpuReferenceFrameTimeMs = 0.0f;
		float deltaSeconds = 0.0f;
		bool gpuTimeValid = false;
		bool paused = false;
	};

	struct Output
	{
		// scale is the scale actually applied to the renderer. requestedScale is
		// the controller's current target while the stability hold is active.
		float scale = 1.0f;
		float requestedScale = 1.0f;
		float smoothedFrameTimeMs = 0.0f;
		float smoothedGpuTimeMs = 0.0f;
		float gpuBusyRatio = 0.0f;
		float holdRemainingSeconds = 0.0f;
		float targetStableSeconds = 0.0f;
		float lastScaleDelta = 0.0f;
		State state = State::Disabled;
		BoundState boundState = BoundState::Unknown;
		bool cpuGuardActive = false;
		bool scaleChanged = false;
	};

	/**
	 * @brief Stable dynamic-resolution controller for TN Adaptive 80 v0.4.
	 *
	 * v0.4 keeps the fast-attack/slow-recovery philosophy and accepts only
	 * upscaler-safe render-scale bounds. Scale requests are quantized, held for
	 * a settling interval, and re-evaluated before another change. GPU timing is
	 * classified as GPU, Mixed, or CPU/engine so a mixed bottleneck can still
	 * shed some GPU load without collapsing image quality.
	 */
	class Controller
	{
	public:
		void Reset(float initialScale);
		Output Update(const Settings& settings, const Sample& sample);
		const Output& GetOutput() const { return output; }

		static const char* GetStateName(State state);
		static const char* GetBoundStateName(BoundState state);

		/** Clamp user-requested floors/ceiling to an upscaler-reported safe range. */
		static ScaleBounds ConstrainScaleBounds(
			float requestedMinScale, float requestedMaxScale, float requestedEmergencyMinScale,
			float providerMinScale, float providerMaxScale);

	private:
		Output output{};
		bool initialized = false;
		std::uint32_t gpuBoundSamples = 0;
		std::uint32_t mixedBoundSamples = 0;
		std::uint32_t cpuBoundSamples = 0;
		std::uint32_t recoverySamples = 0;

		bool probeActive = false;
		std::uint32_t probeSamples = 0;
		float probeStartScale = 1.0f;
		float probeStartFrameTimeMs = 0.0f;
		float probeFrameTimeSumMs = 0.0f;
		float probeSettleSecondsRemaining = 0.0f;
		float cpuGuardRestoreScale = 1.0f;
		float cpuGuardSecondsRemaining = 0.0f;

		void UpdateBoundState(const Settings& settings, const Sample& sample, float upperDeadBandMs);
		void StartProbe(float startScale, float startFrameTimeMs, float settleSeconds);
		void UpdateProbe(const Settings& settings, float upperDeadBandMs, float currentFrameTimeMs, float deltaSeconds);
		void ActivateCpuGuard(float restoreScale);
		float NextLowerScale(const Settings& settings, float floorScale, std::uint32_t steps) const;
		float NextHigherScale(const Settings& settings, float ceilingScale, std::uint32_t steps) const;
		bool ApplyScaleEvent(float requestedScale, const Settings& settings, float holdMultiplier = 1.0f);
	};
}
