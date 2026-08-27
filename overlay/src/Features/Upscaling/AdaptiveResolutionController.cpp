#include "AdaptiveResolutionController.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float kMinimumFrameTimeMs = 1.0f;
	constexpr float kMaximumFrameTimeMs = 250.0f;
	constexpr float kMinimumDeltaSeconds = 1.0f / 500.0f;
	constexpr float kMaximumDeltaSeconds = 0.10f;
	constexpr float kGpuBoundEnterRatio = 0.82f;
	constexpr float kGpuBoundImmediateRatio = 0.72f;
	constexpr float kCpuBoundEnterRatio = 0.68f;
	constexpr std::uint32_t kGpuBoundConfirmationSamples = 3;
	constexpr std::uint32_t kCpuBoundConfirmationSamples = 8;
	constexpr std::uint32_t kProbeEvaluationSamples = 6;
	constexpr float kCpuGuardHoldSeconds = 2.0f;

	float ClampScale(float value)
	{
		return std::clamp(value, 0.33f, 1.0f);
	}
}

namespace Adaptive80
{
	void Controller::Reset(float initialScale)
	{
		output = {};
		output.scale = ClampScale(initialScale);
		output.state = State::Target;
		output.boundState = BoundState::Unknown;
		initialized = true;
		gpuBoundSamples = 0;
		cpuBoundSamples = 0;
		recoverySamples = 0;
		probeActive = false;
		probeSamples = 0;
		probeStartScale = output.scale;
		probeStartFrameTimeMs = 0.0f;
		probeFrameTimeSumMs = 0.0f;
		cpuGuardRestoreScale = output.scale;
		cpuGuardSecondsRemaining = 0.0f;
	}

	Output Controller::Update(const Settings& rawSettings, const Sample& sample)
	{
		Settings settings = rawSettings;
		settings.targetFrameTimeMs = std::clamp(settings.targetFrameTimeMs, 8.0f, 66.67f);
		settings.maxScale = ClampScale(settings.maxScale);
		settings.minScale = std::clamp(ClampScale(settings.minScale), 0.33f, settings.maxScale);
		settings.emergencyMinScale = std::clamp(ClampScale(settings.emergencyMinScale), 0.33f, settings.minScale);
		settings.attackScalePerSecond = std::clamp(settings.attackScalePerSecond, 0.05f, 3.0f);
		settings.recoveryScalePerSecond = std::clamp(settings.recoveryScalePerSecond, 0.005f, 0.50f);
		settings.gpuHeadroom = std::clamp(settings.gpuHeadroom, 0.80f, 0.98f);

		if (!initialized)
			Reset(settings.maxScale);

		output.scale = std::clamp(output.scale, settings.emergencyMinScale, settings.maxScale);
		if (sample.paused) {
			output.state = State::Paused;
			return output;
		}

		if (!std::isfinite(sample.frameTimeMs) || sample.frameTimeMs < kMinimumFrameTimeMs || sample.frameTimeMs > kMaximumFrameTimeMs)
			return output;

		const float deltaSeconds = std::clamp(
			std::isfinite(sample.deltaSeconds) ? sample.deltaSeconds : 0.0f,
			kMinimumDeltaSeconds,
			kMaximumDeltaSeconds);

		if (output.smoothedFrameTimeMs <= 0.0f) {
			output.smoothedFrameTimeMs = sample.frameTimeMs;
		} else {
			// React to performance losses quickly and restore quality only after a
			// sustained recovery. The time constants keep behaviour refresh-rate independent.
			const float timeConstant = sample.frameTimeMs > output.smoothedFrameTimeMs ? 0.12f : 0.55f;
			const float alpha = 1.0f - std::exp(-deltaSeconds / timeConstant);
			output.smoothedFrameTimeMs += (sample.frameTimeMs - output.smoothedFrameTimeMs) * alpha;
		}

		if (sample.frameTimeMs > 65.0f)
			output.smoothedFrameTimeMs = std::max(output.smoothedFrameTimeMs, sample.frameTimeMs * 0.80f);

		const float lowerDeadBandMs = settings.targetFrameTimeMs * 0.88f;
		const float upperDeadBandMs = settings.targetFrameTimeMs * 1.08f;
		const float rescueBoundaryMs = std::max(40.0f, settings.targetFrameTimeMs * 1.60f);
		const float emergencyBoundaryMs = std::max(65.0f, settings.targetFrameTimeMs * 2.60f);

		UpdateBoundState(settings, sample, upperDeadBandMs);
		UpdateProbe(settings, upperDeadBandMs, sample.frameTimeMs);

		if (output.cpuGuardActive) {
			cpuGuardSecondsRemaining = std::max(0.0f, cpuGuardSecondsRemaining - deltaSeconds);
			const float restoreRate = std::max(settings.recoveryScalePerSecond * 2.0f, 0.10f);
			output.scale = std::min(cpuGuardRestoreScale, output.scale + restoreRate * deltaSeconds);
			output.scale = std::min(output.scale, settings.maxScale);
			output.state = State::CpuLimited;

			const bool recovered = output.smoothedFrameTimeMs <= upperDeadBandMs;
			if (recovered && ++recoverySamples >= 4) {
				output.cpuGuardActive = false;
				output.boundState = BoundState::Unknown;
				cpuGuardSecondsRemaining = 0.0f;
				recoverySamples = 0;
			} else if (!recovered) {
				recoverySamples = 0;
			}

			if (cpuGuardSecondsRemaining <= 0.0f && !recovered) {
				output.cpuGuardActive = false;
				output.boundState = BoundState::Unknown;
				gpuBoundSamples = 0;
				cpuBoundSamples = 0;
			}
			return output;
		}

		const float effectiveFrameTimeMs = output.smoothedFrameTimeMs;
		if (effectiveFrameTimeMs <= lowerDeadBandMs) {
			output.state = State::Quality;
			output.scale = std::min(settings.maxScale, output.scale + settings.recoveryScalePerSecond * deltaSeconds);
			return output;
		}

		if (effectiveFrameTimeMs <= upperDeadBandMs) {
			output.state = State::Target;
			return output;
		}

		output.state = effectiveFrameTimeMs > emergencyBoundaryMs ? State::Emergency : State::Rescue;

		bool reductionAllowed = !settings.cpuGuard || output.boundState != BoundState::Cpu;
		if (settings.cpuGuard && sample.gpuTimeValid && output.gpuBusyRatio < kGpuBoundImmediateRatio)
			reductionAllowed = false;
		if (!reductionAllowed) {
			ActivateCpuGuard(std::max(output.scale, probeStartScale));
			return output;
		}

		// When the GPU signal is unavailable or ambiguous, make one small probe
		// and wait for its effect. This prevents a CPU bottleneck from collapsing image quality.
		if (settings.cpuGuard && probeActive)
			return output;

		const float floorScale = effectiveFrameTimeMs > emergencyBoundaryMs ? settings.emergencyMinScale : settings.minScale;
		const float desiredFrameTimeMs = settings.targetFrameTimeMs * settings.gpuHeadroom;
		const float idealRatio = std::clamp(std::sqrt(desiredFrameTimeMs / effectiveFrameTimeMs), 0.60f, 0.995f);
		const float idealScale = output.scale * idealRatio;

		float attackMultiplier = 1.0f;
		if (effectiveFrameTimeMs > rescueBoundaryMs)
			attackMultiplier = 1.35f;
		if (effectiveFrameTimeMs > emergencyBoundaryMs)
			attackMultiplier = 1.75f;

		float maximumDrop = settings.attackScalePerSecond * attackMultiplier * deltaSeconds;
		if (settings.cpuGuard && output.boundState == BoundState::Unknown)
			maximumDrop = std::min(maximumDrop, effectiveFrameTimeMs > emergencyBoundaryMs ? 0.06f : 0.035f);

		const float previousScale = output.scale;
		output.scale = std::max(floorScale, std::max(idealScale, output.scale - maximumDrop));
		output.scale = std::clamp(output.scale, settings.emergencyMinScale, settings.maxScale);

		if (settings.cpuGuard && output.boundState == BoundState::Unknown && !probeActive && previousScale - output.scale >= 0.01f)
			StartProbe(previousScale, effectiveFrameTimeMs);

		return output;
	}

	void Controller::UpdateBoundState(const Settings& settings, const Sample& sample, float upperDeadBandMs)
	{
		if (output.cpuGuardActive)
			return;

		if (!sample.gpuTimeValid || !std::isfinite(sample.gpuTimeMs) || sample.gpuTimeMs <= 0.0f || sample.gpuTimeMs > 250.0f) {
			output.gpuBusyRatio = 0.0f;
			return;
		}

		if (output.smoothedGpuTimeMs <= 0.0f)
			output.smoothedGpuTimeMs = sample.gpuTimeMs;
		else
			output.smoothedGpuTimeMs += (sample.gpuTimeMs - output.smoothedGpuTimeMs) * 0.25f;

		const float referenceFrameTimeMs =
			std::isfinite(sample.gpuReferenceFrameTimeMs) && sample.gpuReferenceFrameTimeMs > 0.0f ?
				sample.gpuReferenceFrameTimeMs :
				sample.frameTimeMs;
		const float measuredBusyRatio = std::clamp(sample.gpuTimeMs / std::max(referenceFrameTimeMs, 0.1f), 0.0f, 1.25f);
		if (output.gpuBusyRatio <= 0.0f)
			output.gpuBusyRatio = measuredBusyRatio;
		else
			output.gpuBusyRatio += (measuredBusyRatio - output.gpuBusyRatio) * 0.25f;

		if (output.smoothedFrameTimeMs <= upperDeadBandMs) {
			gpuBoundSamples = 0;
			cpuBoundSamples = 0;
			if (output.boundState == BoundState::Cpu)
				output.boundState = BoundState::Unknown;
			return;
		}

		if (output.gpuBusyRatio >= kGpuBoundEnterRatio) {
			gpuBoundSamples++;
			cpuBoundSamples = 0;
		} else if (output.gpuBusyRatio <= kCpuBoundEnterRatio) {
			cpuBoundSamples++;
			gpuBoundSamples = 0;
		} else {
			gpuBoundSamples = gpuBoundSamples > 0 ? gpuBoundSamples - 1 : 0;
			cpuBoundSamples = cpuBoundSamples > 0 ? cpuBoundSamples - 1 : 0;
		}

		if (gpuBoundSamples >= kGpuBoundConfirmationSamples) {
			output.boundState = BoundState::Gpu;
			output.cpuGuardActive = false;
		} else if (settings.cpuGuard && cpuBoundSamples >= kCpuBoundConfirmationSamples) {
			output.boundState = BoundState::Cpu;
			ActivateCpuGuard(std::max(output.scale, probeStartScale));
		}
	}

	void Controller::StartProbe(float startScale, float startFrameTimeMs)
	{
		probeActive = true;
		probeSamples = 0;
		probeStartScale = startScale;
		probeStartFrameTimeMs = startFrameTimeMs;
		probeFrameTimeSumMs = 0.0f;
	}

	void Controller::UpdateProbe(const Settings& settings, float upperDeadBandMs, float currentFrameTimeMs)
	{
		if (!probeActive)
			return;

		probeFrameTimeSumMs += currentFrameTimeMs;
		if (++probeSamples < kProbeEvaluationSamples)
			return;

		probeActive = false;
		const float observedFrameTimeMs = probeFrameTimeSumMs / static_cast<float>(probeSamples);
		const float pixelReduction = 1.0f - (output.scale * output.scale) /
			std::max(probeStartScale * probeStartScale, 0.001f);
		const float frameImprovement = (probeStartFrameTimeMs - observedFrameTimeMs) /
			std::max(probeStartFrameTimeMs, 0.1f);
		const float requiredImprovement = std::max(0.025f, pixelReduction * 0.20f);

		if (settings.cpuGuard && output.smoothedFrameTimeMs > upperDeadBandMs &&
			pixelReduction > 0.025f && frameImprovement < requiredImprovement) {
			output.boundState = BoundState::Cpu;
			ActivateCpuGuard(probeStartScale);
		} else if (pixelReduction > 0.025f && frameImprovement >= requiredImprovement) {
			output.boundState = BoundState::Gpu;
		}
	}

	void Controller::ActivateCpuGuard(float restoreScale)
	{
		output.cpuGuardActive = true;
		output.boundState = BoundState::Cpu;
		output.state = State::CpuLimited;
		cpuGuardRestoreScale = std::max(output.scale, ClampScale(restoreScale));
		cpuGuardSecondsRemaining = kCpuGuardHoldSeconds;
		probeActive = false;
		probeSamples = 0;
		probeFrameTimeSumMs = 0.0f;
		gpuBoundSamples = 0;
		cpuBoundSamples = 0;
		recoverySamples = 0;
	}

	const char* Controller::GetStateName(State state)
	{
		switch (state) {
		case State::Disabled:
			return "Disabled";
		case State::Quality:
			return "Quality";
		case State::Target:
			return "Target";
		case State::Rescue:
			return "Rescue";
		case State::Emergency:
			return "Emergency";
		case State::CpuLimited:
			return "CPU Guard";
		case State::Paused:
			return "Paused";
		}
		return "Unknown";
	}

	const char* Controller::GetBoundStateName(BoundState state)
	{
		switch (state) {
		case BoundState::Unknown:
			return "Learning";
		case BoundState::Gpu:
			return "GPU";
		case BoundState::Cpu:
			return "CPU";
		}
		return "Unknown";
	}
}
