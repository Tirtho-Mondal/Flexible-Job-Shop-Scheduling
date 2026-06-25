#pragma once
// ============================================================================
//  GanttChart.h
//  ---------------------------------------------------------------------------
//  Renders a decoded schedule as a Gantt chart in SVG (no external libraries):
//  one row per machine, one coloured bar per operation placed at its start/end
//  time, every JOB drawn in its own unique colour, plus a legend. Written next
//  to the other results, under output/ganchart/.
// ============================================================================

#include "Instance.h"
#include "Schedule.h"
#include <string>

using namespace std;

namespace fjs {

class GanttChart {
public:
    // Write an SVG Gantt chart of `sched` (decoded from `inst`) to `path`.
    static void write(const string& path, const Instance& inst, const Schedule& sched);
};

} // namespace fjs
