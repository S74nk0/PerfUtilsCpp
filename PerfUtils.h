#pragma once

#include <array>
#include <vector>
#include <ratio>
#include <chrono>
#include <numeric>
#include <algorithm>

// POZOR
// <chrono> daje slabe rezultate, kjer so meritve premalo natancne
// ref (mar. 2015): https://connect.microsoft.com/VisualStudio/feedback/details/719443/
// na podlagi zgornjega linka, ceprav je BUG closed in popravljen, BUG se vedno obstaja
// resitev je uporaba drugega prevajalnika (MINGW)

enum class MetricPrefixType : size_t {
	milli = 0,
	micro,
	nano
};
#define MetricPrefixTypeCount 3

template<typename FUN, typename FUN_RET_TYPE>
class PerfUtil
{
	// is best suitable for measuring intervals , ref (mar. 2015): http://en.cppreference.com/w/cpp/chrono/steady_clock
	typedef std::chrono::steady_clock MyClock;
	//typedef std::chrono::high_resolution_clock MyClock; // ta je lahko sistemski

	using MyDurationCast = std::chrono::duration<double, std::nano>;
	//using MyDurationCast = std::chrono::nanoseconds;
public:
	PerfUtil(size_t maxIters) : _MAX_ITERATIONS(maxIters) {
		reset();
	}
	~PerfUtil() {}
	void reset() { _timesNs.clear(); _timesNs.reserve(_MAX_ITERATIONS); /*_currentIteration = 0;*/ }
	/// time stuff => nano = 10^-9; micro = 10^-6; milli = 10^-3
	// Last measured time
	const double getLastMeasuredNanoSecond() const { return _timesNs.back(); }
	const double getLastMeasuredMicroSecond() const { return getLastMeasuredNanoSecond() / 1000; }
	const double getLastMeasuredMilliSecond() const { return getLastMeasuredMicroSecond() / 1000; }
	// Avarage measured time, make sure to call fullProfileMeasure
	//const double getAvarageTimeNanoSecond() const { return static_cast<double>(std::accumulate(_timesNs.begin(), _timesNs.end(), 0)) / static_cast<double>(_timesNs.size()); }
	// zgornja funkcija lahko privede do overflow-a
	const double getAvarageTimeNanoSecond() const {
		double sum = 0;
		auto div = static_cast<double>(_timesNs.size());
		for (const auto time : _timesNs) {
			sum += time / div;
		}
		return sum;
	}

	const double getAvarageTimeMicroSecond() const { return getAvarageTimeNanoSecond() / 1000; }
	const double getAvarageTimeMilliSecond() const { return getAvarageTimeMicroSecond() / 1000; }

	const size_t getMaxIters() const { return _MAX_ITERATIONS; }
	const size_t getCurIters() const { return _timesNs.size(); }

	template<typename ...Args>
	FUN_RET_TYPE measureOnceAndGetResault(FUN function, Args&&... args) {
		FUN_RET_TYPE ret;
		start();
		ret = function(std::forward<Args>(args)...);
		end();
		return ret;
	}
	template<typename ...Args>
	void fullProfileMeasure(FUN function, Args&&... args) {
		while (_timesNs.size() < _MAX_ITERATIONS) {
			start();
			function(std::forward<Args>(args)...);
			end();
		}
	}

private:
	std::vector<double> _timesNs;
	const size_t _MAX_ITERATIONS;
	//size_t _currentIteration = 0;
	MyClock::time_point _start;
	MyClock::time_point _end;
	// methods
	void start() { _start = MyClock::now(); }
	void end(bool addToTimes = true) { _end = MyClock::now(); if (addToTimes) _timesNs.push_back(getElapsedTime()); }
	double getElapsedTime() { return std::chrono::duration_cast<MyDurationCast>(_end - _start).count(); }
};

//PerfUtils
template<typename FUN, typename FUN_RET_TYPE, typename ...Args>
FUN_RET_TYPE execFunction(bool profile, size_t NUM_OF_ITERS, const MetricPrefixType mpType, FUN function, Args&&... args) {
	const auto mpTypeIndex = static_cast<size_t>(mpType);
	const std::array<const char *, MetricPrefixTypeCount> prefixTypeString = { "milli", "micro", "nano" };
	std::array<double, MetricPrefixTypeCount> prefixTypeTimes;
	auto perfUtil = PerfUtil<FUN, FUN_RET_TYPE>(NUM_OF_ITERS);
	auto calcPrefixTypeTimes = [&]() {
		prefixTypeTimes[static_cast<size_t>(MetricPrefixType::milli)] = perfUtil.getAvarageTimeMilliSecond();
		prefixTypeTimes[static_cast<size_t>(MetricPrefixType::micro)] = perfUtil.getAvarageTimeMicroSecond();
		prefixTypeTimes[static_cast<size_t>(MetricPrefixType::nano)] = perfUtil.getLastMeasuredNanoSecond();
	};

	FUN_RET_TYPE resultat = perfUtil.measureOnceAndGetResault(function, std::forward<Args>(args)...);

	if (profile) {
		perfUtil.fullProfileMeasure(function, std::forward<Args>(args)...);
		calcPrefixTypeTimes();
		printf("Avarage time in %s seconds (%d iterations): %e \n", prefixTypeString[mpTypeIndex], perfUtil.getMaxIters(), prefixTypeTimes[mpTypeIndex]);
		//printf("iteratons %d \n", perfUtil.getCurIters());
	}
	else {
		calcPrefixTypeTimes();
		printf("Exec time in %s seconds: %e \n", prefixTypeString[mpTypeIndex], prefixTypeTimes[mpTypeIndex]);
	}

	return resultat;
}
