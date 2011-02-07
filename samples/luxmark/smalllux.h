/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#ifndef _SMALLLUX_H
#define	_SMALLLUX_H

#include <cmath>
#include <sstream>
#include <fstream>
#include <iostream>

#if defined(__linux__) || defined(__APPLE__)
#include <stddef.h>
#include <sys/time.h>
#elif defined (WIN32)
#include <windows.h>
#else
	Unsupported Platform !!!
#endif

#include "luxrays/luxrays.h"
#include "luxrays/core/utils.h"
#include "luxrays/utils/sdl/scene.h"
#include "luxrays/utils/film/film.h"
#include "luxrays/utils/core/atomic.h"

#include "slgcfg.h"

#include "mainwindow.h"

using namespace std;
using namespace luxrays;
using namespace luxrays::sdl;
using namespace luxrays::utils;

extern void DebugHandler(const char *msg);

extern MainWindow *LogWindow;

class LuxLogEvent: public QEvent {
public:
	LuxLogEvent(QString mesg);
 
	QString getMessage() { return message; }

private:
	QString message;
};

#define LM_LOG(a) { if (LogWindow) { std::stringstream _LM_LOG_LOCAL_SS; _LM_LOG_LOCAL_SS << a; qApp->postEvent(LogWindow, new LuxLogEvent(QString(_LM_LOG_LOCAL_SS.str().c_str()))); }}
#define LM_LOG_LUXRAYS(a) { LM_LOG("<FONT COLOR=\"#000000\"><B>[LuxRays]</B></FONT> " << a); }
#define LM_LOG_ENGINE(a) { LM_LOG("<FONT COLOR=\"#00ff00\"><B>[RenderEngine]</B></FONT> " << a); }
#define LM_LOG_CFG(a) { LM_LOG("<FONT COLOR=\"#005500\"><B>[RenderConfig]</B></FONT> " << a); }

enum LuxMarkAppMode {
	BENCHMARK_OCL_GPU, BENCHMARK_OCL_CPUGPU, BENCHMARK_NATIVE, INTERACTIVE
};

#endif	/* _SMALLLUX_H */
