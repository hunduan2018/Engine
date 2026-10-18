
//*********************************************************
// GameTimer.cpp
//*********************************************************
#include "GameTimer.h"

// Initializes the timer
GameTimer::GameTimer()
	: m_secondsPerCount(0.0), m_deltaTime(-1.0), m_baseTime(0),
	  m_pausedTime(0), m_stopTime(0), m_prevTime(0), m_currTime(0),
	  m_stopped(false)
{
	__int64 countsPerSec;
	QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&countsPerSec));
	m_secondsPerCount = 1.0 / static_cast<double>(countsPerSec);
}

// Resets the timer to zero
void GameTimer::Reset()
{
	__int64 currTime;
	QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currTime));
	m_baseTime = currTime;
	m_prevTime = currTime;
	m_stopTime = 0;
	m_stopped = false;
}

// Starts the timer
void GameTimer::Start()
{
	// 
	__int64 startTime;
	QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&startTime));

	// 
	if (m_stopped)
	{
		m_pausedTime += (startTime - m_stopTime);
		m_prevTime = startTime;
		m_stopTime = 0;
		m_stopped = false;
	}
}

// Stops the timer
void GameTimer::Stop()
{
	if (!m_stopped)
	{
		__int64 currTime;
		QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currTime));
		m_stopTime = currTime;
		m_stopped = true;
	}
}

// Updates the timer
void GameTimer::Tick()
{
	if (m_stopped)
	{
		m_deltaTime = 0.0;
		return;
	}

	// Get the current time
	__int64 currTime;
	QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currTime));
	m_currTime = currTime;

	// Time difference between this frame and the previous frame
	m_deltaTime = (m_currTime - m_prevTime) * m_secondsPerCount;

	// Prepare for next frame
	m_prevTime = m_currTime;

	// Force nonnegative. 
	// The DXSDK's CDXUTTimer mentions that if the processor goes 
	// into a power save mode or we get shuffled to another processor,
	// then m_deltaTime can be negative.
	if (m_deltaTime < 0.0)
	{
		m_deltaTime = 0.0;
	}
}

// Total time since reset
float GameTimer::TotalTime() const
{
	if (m_stopped)
	{
		return static_cast<float>(((m_stopTime - m_pausedTime) - m_baseTime) * m_secondsPerCount);
	}
	else
	{
		return static_cast<float>(((m_currTime - m_pausedTime) - m_baseTime) * m_secondsPerCount);
	}
}

// Time between the last two ticks
float GameTimer::DeltaTime() const
{
	return static_cast<float>(m_deltaTime);
}

// Returns the elapsed time in seconds since the timer was last reset
float GameTimer::GetElapsedSeconds() const
{
	return TotalTime();
}

// Returns the elapsed time in milliseconds since the timer was last reset
float GameTimer::GetElapsedMilliseconds() const
{
	return TotalTime() * 1000.0f;
}

// Returns the elapsed time in microseconds since the timer was last reset
float GameTimer::GetElapsedMicroseconds() const
{
	return TotalTime() * 1000000.0f;
}

// Returns the elapsed time in nanoseconds since the timer was last reset
float GameTimer::GetElapsedNanoseconds() const
{
	return TotalTime() * 1000000000.0f;
}

// Returns the elapsed time in seconds since the timer was last reset, as a double
double GameTimer::GetElapsedSecondsDouble() const
{
	return TotalTime();
}
