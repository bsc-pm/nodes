/*
	This file is part of NODES and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2021 Barcelona Supercomputing Center (BSC)
*/

#ifndef TAP_DRIVER_HPP
#define TAP_DRIVER_HPP

#include <cstdarg>
#include <cstdlib>
#include <iostream>
#include <string>
#include <sstream>
#include <sys/time.h>

#if __cplusplus >= 201103L
#include <mutex>
#else
#include <pthread.h>
namespace std {
	struct mutex {
		pthread_mutex_t _mutex;

		inline mutex()
		{
			pthread_mutex_init(&_mutex, 0);
		}

		void lock()
		{
			pthread_mutex_lock(&_mutex);
		}

		void unlock()
		{
			pthread_mutex_unlock(&_mutex);
		}
	};

	template <typename T>
	struct lock_guard {
		mutex &_mutex;

		inline lock_guard(mutex &mutex) :
			_mutex(mutex)
		{
			_mutex.lock();
		}

		inline ~lock_guard()
		{
			_mutex.unlock();
		}
	};
}
#endif


//! \brief A class that generates output recognizable by autotools' tesing protocol
class TAPDriver {

private:

	//! Current test ID
	size_t _currentTest;

	//! Whether any test has failed up until now
	bool _hasFailed;

	// A mutex to access the previous fields
	std::mutex _mutex;

private:

	void emitOutcome(
		std::string const &outcome,
		std::string const &detail,
		std::string const &special = ""
	) {
		std::cout << outcome << " " << _currentTest;
		if (detail != "") {
			std::cout << " " << detail;
		}

		if (special != "") {
			std::cout << " # " << special;
		}

		std::cout << std::endl;
	}

public:

	inline TAPDriver() :
		_currentTest(1),
		_hasFailed(false),
		_mutex()
	{
	}

	//! \brief Finish the set of tests
	void end()
	{
		// Autotools searches for this string at the start or end
		std::cout << "1.." << _currentTest - 1 << std::endl;
	}

	//! \brief Indicate that the current test was successful
	//! \param[in] detail (Optional) A comment explaining the outcome
	void success(std::string const &detail = "")
	{
		std::lock_guard<std::mutex> guard(_mutex);
		{
			emitOutcome("ok", detail);

			_currentTest++;
		}
	}

	//! \brief Indicate that the current test failed
	//! \param[in] detail (Optional) A comment explaining the outcome
	void failure(std::string const &detail = "")
	{
		std::lock_guard<std::mutex> guard(_mutex);
		{
			emitOutcome("not ok", detail);

			_currentTest++;
			_hasFailed = true;
		}
	}

	//! \brief Indicate that the current test failed but that it was expected
	//! \param[in] detail A comment explaining the outcome
	//! \param[in] weakDetail A comment explaining why the test is weak
	void weakFailure(std::string const &detail, std::string const &weakDetail)
	{
		std::lock_guard<std::mutex> guard(_mutex);
		{
			std::ostringstream special;
			special << "TODO " << weakDetail;
			emitOutcome("not ok", detail, special.str());

			_currentTest++;
		}
	}

	//! \brief Indicate that the current test has been skipped
	//! \param[in] detail (Optional) A comment explaining the outcome
	void skip(std::string const &detail = "")
	{
		std::lock_guard<std::mutex> guard(_mutex);
		{
			emitOutcome("ok", detail, "SKIP");

			_currentTest++;
		}
	}

	//! \brief Indicate that the set of tests will stop here (even if not all of them have been run)
	//! \param[in] detail (Optional) A comment explaining the reason for the bail
	void bailOut(std::string const &detail = "")
	{
		std::lock_guard<std::mutex> guard(_mutex);
		{
			std::cout << "Bail out!";
			if (detail != "") {
				std::cout << " " << detail;
			}
			std::cout << std::endl;
		}
	}


	//! \brief Evaluate a condition linked to the current test
	//! \param[in] condition True if the test was successful, false otherwise
	//! \param[in] detail (Optional) Information about the test
	void evaluate(bool condition, std::string const &detail = "")
	{
		if (condition) {
			success(detail);
		} else {
			failure(detail);
		}
	}

	//! \brief (Weakly) Evaluate a condition linked to the current test
	//! \param[in] condition True if the test was successful, false otherwise
	//! \param[in] detail Information about the test
	//! \param[in] weakDetail Information about why the test is weak
	void evaluateWeak(bool condition, std::string const &detail, std::string const &weakDetail)
	{
		if (condition) {
			success(detail);
		} else {
			weakFailure(detail, weakDetail);
		}
	}

	//! \brief Check that a condition is satisfied in a given amount of time
	//! \param[in] condition True if the test was successful, false otherwise
	//! \param[in] microseconds Grace period to assert the condition
	//! \param[in] detail (Optional) Information about the test
	//! \param[in] weak True if a timeout does not necessarily mean incorrectness
	template <typename ConditionType>
	void timedEvaluate(
		ConditionType condition,
		long microseconds,
		std::string const &detail = "",
		bool weak = false
	) {
		// Get the current timestamp and compute the maximum timestamp (timeout)
		timeval current, maximum;
		gettimeofday(&current, 0);

		maximum.tv_sec = current.tv_sec;
		maximum.tv_usec = current.tv_usec + microseconds;
		while (maximum.tv_usec >= 1000000L) {
			maximum.tv_sec += 1;
			maximum.tv_usec -= 1000000L;
		}

		// Check the condition until timeout
		while (!condition()) {
			gettimeofday(&current, 0);
			bool timeout = current.tv_sec > maximum.tv_sec || (
				(current.tv_sec == maximum.tv_sec) && (current.tv_usec > maximum.tv_usec)
			);

			if (timeout) {
				if (condition()) {
					success(detail);
				} else {
					if (!weak) {
						failure(detail);
					} else {
						weakFailure(detail, "timed out waiting for the condition to be asserted");
					}
				}

				return;
			}
		}

		// Condition asserted in the given timeframe
		success(detail);
	}

	//! \brief Check that a condition is kept during a given amount of time
	//! \param[in] condition True if the test was successful, false otherwise
	//! \param[in] microseconds Grace period to assert the condition
	//! \param[in] detail (Optional) Information about the test
	template <typename ConditionType>
	void sustainedEvaluate(
		ConditionType condition,
		long microseconds,
		std::string const &detail = ""
	) {
		// Get the current timestamp and compute the maximum timestamp (timeout)
		timeval current, maximum;
		gettimeofday(&current, 0);

		maximum.tv_sec = current.tv_sec;
		maximum.tv_usec = current.tv_usec + microseconds;
		while (maximum.tv_usec >= 1000000L) {
			maximum.tv_sec += 1;
			maximum.tv_usec -= 1000000L;
		}

		// Check that the condition holds until timeout
		while (condition()) {
			gettimeofday(&current, 0);
			bool timeout = current.tv_sec > maximum.tv_sec || (
				(current.tv_sec == maximum.tv_sec) && (current.tv_usec > maximum.tv_usec)
			);

			if (timeout) {
				if (condition()) {
					success(detail);
				} else {
					failure(detail);
				}

				return;
			}
		}

		// Condition not asserted until the given timeframe
		failure(detail);
	}

	//! \brief Exit abruptly if any of the tests up to this point has failed
	void bailOutAndExitIfAnyFailed()
	{
		if (_hasFailed) {
			bailOut("to avoid further errors");
			std::exit(1);
		}
	}

	template <typename T1>
	void emitDiagnostic(T1 v1)
	{
		std::lock_guard<std::mutex> guard(_mutex);
		std::cout << "# " << v1 << std::endl;
	}

	template <typename T1, typename T2>
	void emitDiagnostic(T1 v1, T2 v2)
	{
		std::lock_guard<std::mutex> guard(_mutex);
		std::cout << "# " << v1 << v2 << std::endl;
	}

	template <typename T1, typename T2, typename T3>
	void emitDiagnostic(T1 v1, T2 v2, T3 v3)
	{
		std::lock_guard<std::mutex> guard(_mutex);
		std::cout << "# " << v1 << v2 << v3 << std::endl;
	}

	template <typename T1, typename T2, typename T3, typename T4>
	void emitDiagnostic(T1 v1, T2 v2, T3 v3, T4 v4)
	{
		std::lock_guard<std::mutex> guard(_mutex);
		std::cout << "# " << v1 << v2 << v3 << v4 << std::endl;
	}

	template <typename T1, typename T2, typename T3, typename T4, typename T5>
	void emitDiagnostic(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5)
	{
		std::lock_guard<std::mutex> guard(_mutex);
		std::cout << "# " << v1 << v2 << v3 << v4 << v5 << std::endl;
	}

	template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
	void emitDiagnostic(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6)
	{
		std::lock_guard<std::mutex> guard(_mutex);
		std::cout << "# " << v1 << v2 << v3 << v4 << v5 << v6 << std::endl;
	}

	template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
	void emitDiagnostic(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6, T7 v7)
	{
		std::lock_guard<std::mutex> guard(_mutex);
		std::cout << "# " << v1 << v2 << v3 << v4 << v5 << v6 << v7 << std::endl;
	}

	template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>
	void emitDiagnostic(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6, T7 v7, T8 v8)
	{
		std::lock_guard<std::mutex> guard(_mutex);
		std::cout << "# " << v1 << v2 << v3 << v4 << v5 << v6 << v7 << v8 << std::endl;
	}

	template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>
	void emitDiagnostic(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6, T7 v7, T8 v8, T9 v9)
	{
		std::lock_guard<std::mutex> guard(_mutex);
		std::cout << "# " << v1 << v2 << v3 << v4 << v5 << v6 << v7 << v8 << v9 << std::endl;
	}

	template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>
	void emitDiagnostic(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6, T7 v7, T8 v8, T9 v9, T10 v10)
	{
		std::lock_guard<std::mutex> guard(_mutex);
		std::cout << "# " << v1 << v2 << v3 << v4 << v5 << v6 << v7 << v8 << v9 << v10 << std::endl;
	}

};

#endif // TAP_DRIVER_HPP
