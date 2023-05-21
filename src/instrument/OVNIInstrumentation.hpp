/*
	This file is part of NODES and is licensed under the terms contained in the COPYING and COPYING.LESSER files.

	Copyright (C) 2021-2023 Barcelona Supercomputing Center (BSC)
*/

#ifndef OVNI_INSTRUMENTATION_HPP
#define OVNI_INSTRUMENTATION_HPP

#include <cstdlib>

#ifdef ENABLE_OVNI_INSTRUMENTATION
#include <ovni.h>
#endif


class Instrument {

private:
	static inline bool _enabled = false;

private:
#ifdef ENABLE_OVNI_INSTRUMENTATION
	static inline void emitOvniEvent(const char *mcv)
	{
		if (_enabled) {
			struct ovni_ev ev = {};

			ovni_ev_set_clock(&ev, ovni_clock_now());
			ovni_ev_set_mcv(&ev, (char *) mcv);

			ovni_ev_emit(&ev);
		}
	}
#else
	static inline void emitOvniEvent(const char *)
	{
	}
#endif

public:

	static inline void initializeOvni()
	{
		const char *envvar = std::getenv("NODES_OVNI");
		if (envvar != nullptr) {
			_enabled = (atoi(envvar) == 1);
		}
	}

	static inline void enterRegisterAccesses()
	{
		emitOvniEvent("DR[");
	}

	static inline void exitRegisterAccesses()
	{
		emitOvniEvent("DR]");
	}

	static inline void enterUnregisterAccesses()
	{
		emitOvniEvent("DU[");
	}

	static inline void exitUnregisterAccesses()
	{
		emitOvniEvent("DU]");
	}

	static inline void enterWaitIf0()
	{
		emitOvniEvent("DW[");
	}

	static inline void exitWaitIf0()
	{
		emitOvniEvent("DW]");
	}

	static inline void enterInlineIf0()
	{
		emitOvniEvent("DI[");
	}

	static inline void exitInlineIf0()
	{
		emitOvniEvent("DI]");
	}

	static inline void enterTaskWait()
	{
		emitOvniEvent("DT[");
	}

	static inline void exitTaskWait()
	{
		emitOvniEvent("DT]");
	}

	static inline void enterCreateTask()
	{
		emitOvniEvent("DC[");
	}

	static inline void exitCreateTask()
	{
		emitOvniEvent("DC]");
	}

	static inline void enterSubmitTask()
	{
		emitOvniEvent("DS[");
	}

	static inline void exitSubmitTask()
	{
		emitOvniEvent("DS]");
	}

	static inline void enterSpawnFunction()
	{
		emitOvniEvent("DP[");
	}

	static inline void exitSpawnFunction()
	{
		emitOvniEvent("DP]");
	}

};

#endif // OVNI_INSTRUMENTATION_HPP
