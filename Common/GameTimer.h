#pragma once

#include <Windows.h>


class GameTimer
{
public:
	GameTimer();
	void Reset(); // Resets the timer to zero
	void Start(); // Starts the timer
	void Stop();  // Stops the timer
	void Tick();  // Updates the timer
	float TotalTime() const; // Total time since reset
	float DeltaTime() const; // Time between the last two ticks
	float GetElapsedSeconds() const; // Returns the elapsed time in seconds since the timer was last reset
	float GetElapsedMilliseconds() const; // Returns the elapsed time in milliseconds since the timer was last reset
	float GetElapsedMicroseconds() const; // Returns the elapsed time in microseconds since the timer was last reset
	float GetElapsedNanoseconds() const; // Returns the elapsed time in nanoseconds since the timer was last reset
	double GetElapsedSecondsDouble() const; // Returns the elapsed time in seconds since the timer was last reset, as a double

private:
	double m_secondsPerCount; // Seconds per count
	double m_deltaTime;       // Time between the last two ticks

	__int64 m_baseTime;       // Time when the timer was reset
	__int64 m_pausedTime;     // Total time the timer has been paused
	__int64 m_stopTime;       // Time when the timer was stopped
	__int64 m_prevTime;       // Time of the previous tick
	__int64 m_currTime;       // Current time

	bool m_stopped;           // Is the timer currently stopped?
};
