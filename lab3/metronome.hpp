#pragma once

#include "mbed.h"

class metronome
{
public:
	//! Change the samples to 4. Since we are storing absolute times and
	//! calculating deltas at the end, 4 samples are needed for 3 deltas.
    enum { beat_samples = 4 };

public:
    metronome()
    : m_timing(false), m_beat_count(0) {}
    ~metronome() {}

public:
	// Call when entering "learn" mode
    void start_timing();
	// Call when leaving "learn" mode
    void stop_timing();

	// Should only record the current time when timing
	// Insert the time at the next free position of m_beats
    void tap();

    bool is_timing() const { return m_timing; }
	// Calculate the BPM from the deltas between m_beats
	// Return 0 if there are not enough samples
    size_t get_bpm() const;

private:
    bool m_timing;
    Timer m_timer;

	// Insert new samples at the end of the array, removing the oldest
    size_t m_beats[beat_samples];
    size_t m_beat_count;
};

//! When we start timing, the previous information is no longer needed. We clear
//! out the samples, and start the timer anew.
void metronome::start_timing()
{
    m_beat_count = 0;

    m_timing = true;
    m_timer.start();
}

//! Once timing is done, we prepare the timer for the next start_timing by
//! resetting it.
void metronome::stop_timing()
{
    m_timing = false;
    m_timer.stop();

    m_timer.reset();
}

//! This function is only valid when timing is occurring, since it reads the
//! timer value for information. Inserts a new sample into the array
void metronome::tap()
{
    if (!m_timing)
        return;

    //! The array is full; push out the oldest sample to make room.
    if (m_beat_count == beat_samples)
    {
        //! Shift every element down one, starting at the second. This
        //! overwrites the first element and makes the last available.
        for (size_t i = 1; i != m_beat_count; ++i)
            m_beats[i-1] = m_beats[i];

        //! Decrease the count since the normal path increases it when storing
        m_beat_count -= 1;
    }

    //! Milliseconds are high enough resolution for our timing purposes.
    m_beats[m_beat_count++] = m_timer.read_ms();
}

//! The user has requested the BPM, so we can calculate it from our absolute
//! time samples (given there are currently enough). In this case, there is
//! no caching done by the metronome itself, and the value is recalculated on
//! each call.
size_t metronome::get_bpm() const
{
    if (m_beat_count < beat_samples)
        return 0;

    size_t average = 0;
    for (size_t i = 1; i != m_beat_count; ++i)
    {
        size_t delta = m_beats[i] - m_beats[i-1];
        size_t bpm = (60 * 1000) / delta;

        average += bpm;
    }

    return average / (beat_samples - 1);
}
