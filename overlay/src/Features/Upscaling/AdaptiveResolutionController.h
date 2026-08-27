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
		Paused
	};

	enum class BoundState : std::uint8_t
	{
		Unknown,
		Gpu,
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
		bool cpuGuard = true;
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
		float scale = 1.0f;
		float smoothedFrameTimeMs = 0.0f;
		float smoothedGpuTimeMs = 0.0f;
		float gpuBusyRatio = 0.0f;
		State state = State::Disabled;
		BoundState boundState = BoundState::Unknown;
		bool cpuGuardActive = false;
	};

	/**
	 * @brief Lightweight dynamic-resolution controller for the Adaptive 80 mode.
	 *
	 * The controller uses asymmetric frame-time smoothing, fast scale reduction,
	 * slow recovery, a dead band around the target, and a non-blocking GPU timing
	 * signal. When the GPU signal is unavailable or ambiguous it performs a small
	 * resolution-response probe before deciding whether further reductions help.
	 */
	class Controller
	{
	public:
		void Reset(float initialScale);
		Output Update(const Settings& settings, const Sample& sample);
		const Output& GetOutput() const { return output; }

		static const char* GetStateName(State state);
		static const char* GetBoundStateName(BoundState state);

	private:
		Output output{};
		bool initialized = false;
		std::uint32_t gpuBoundSamples = 0;
		std::uint32_t cpuBoundSamples = 0;
		std::uint32_t recoverySamples = 0;

		bool probeActive = false;
		std::uint32_t probeSamples = 0;
		float probeStartScale = 1.0f;
		float probeStartFrameTimeMs = 0.0f;
		float probeFrameTimeSumMs = 0.0f;
		float cpuGuardRestoreScale = 1.0f;
		float cpuGuardSecondsRemaining = 0.0f;

		void UpdateBoundState(const Settings& settings, const Sample& sample, float upperDeadBandMs);
		void StartProbe(float startScale, float startFrameTimeMs);
		void UpdateProbe(const Settings& settings, float upperDeadBandMs, float currentFrameTimeMs);
		void ActivateCpuGuard(float restoreScale);
	};
}
