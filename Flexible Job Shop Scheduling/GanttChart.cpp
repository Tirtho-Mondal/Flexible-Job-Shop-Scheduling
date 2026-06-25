// ============================================================================
//  GanttChart.cpp - SVG Gantt chart of a schedule, one colour per job.
// ============================================================================
#include "GanttChart.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace std;

namespace fjs {

// A distinct colour per job, evenly spread around the hue circle (HSV -> RGB).
static string jobColor(int job, int numJobs) {
    const double h = (numJobs > 0) ? (360.0 * job / numJobs) : 0.0;
    const double s = 0.62, v = 0.85;
    const double c = v * s;
    const double x = c * (1.0 - fabs(fmod(h / 60.0, 2.0) - 1.0));
    const double m = v - c;
    double r = 0, g = 0, b = 0;
    if      (h <  60) { r = c; g = x; }
    else if (h < 120) { r = x; g = c; }
    else if (h < 180) { g = c; b = x; }
    else if (h < 240) { g = x; b = c; }
    else if (h < 300) { r = x; b = c; }
    else              { r = c; b = x; }
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X",
             (int)((r + m) * 255), (int)((g + m) * 255), (int)((b + m) * 255));
    return string(buf);
}

static string esc(const string& s) {     // minimal XML escaping for labels
    string o;
    for (char ch : s) {
        if      (ch == '&') o += "&amp;";
        else if (ch == '<') o += "&lt;";
        else if (ch == '>') o += "&gt;";
        else                o += ch;
    }
    return o;
}

void GanttChart::write(const string& path, const Instance& inst, const Schedule& sched) {
    const int   m        = inst.numMachines();
    const int   nJobs    = inst.numJobs();
    const int   makespan = max(1, sched.makespan());

    // ---- layout constants ---------------------------------------------
    const int    marginL = 70, marginT = 56, marginR = 30;
    const int    rowH = 28, rowGap = 8;
    const double scale = max(2.0, min(24.0, 1200.0 / makespan));   // px per time unit
    const int    chartW = (int)(makespan * scale);
    const int    plotH  = m * (rowH + rowGap);

    // legend: jobs laid out in columns under the chart
    const int    legendSwatch = 14, legendColW = 90, legendRowH = 20;
    const int    legendCols = max(1, (marginL + chartW + marginR) / legendColW);
    const int    legendRows = (nJobs + legendCols - 1) / legendCols;

    const int    width  = marginL + chartW + marginR;
    const int    height = marginT + plotH + 40 + legendRows * legendRowH + 20;

    ofstream f(path);
    if (!f) return;

    f << "<svg xmlns='http://www.w3.org/2000/svg' width='" << width << "' height='" << height
      << "' font-family='Segoe UI, Arial, sans-serif'>\n";
    f << "<rect width='100%' height='100%' fill='white'/>\n";

    // ---- title ---------------------------------------------------------
    f << "<text x='" << marginL << "' y='28' font-size='18' font-weight='bold'>"
      << esc(inst.name) << "  -  Gantt chart  (Cmax = " << sched.makespan() << ")</text>\n";
    f << "<text x='" << marginL << "' y='46' font-size='12' fill='#555'>group: "
      << esc(inst.group) << "   jobs: " << nJobs << "   machines: " << m
      << "   (each job = one colour)</text>\n";

    // ---- time grid + axis labels --------------------------------------
    int tick = 1;
    while (makespan / tick > 12) tick *= (tick == 1 ? 5 : 2);   // ~ <=12 gridlines
    for (int t = 0; t <= makespan; t += tick) {
        const int x = marginL + (int)(t * scale);
        f << "<line x1='" << x << "' y1='" << marginT << "' x2='" << x << "' y2='" << (marginT + plotH)
          << "' stroke='#e6e6e6'/>\n";
        f << "<text x='" << x << "' y='" << (marginT + plotH + 16)
          << "' font-size='11' fill='#666' text-anchor='middle'>" << t << "</text>\n";
    }

    // ---- machine rows + labels ----------------------------------------
    for (int k = 0; k < m; ++k) {
        const int y = marginT + k * (rowH + rowGap);
        f << "<text x='" << (marginL - 10) << "' y='" << (y + rowH * 0.66)
          << "' font-size='12' text-anchor='end'>M" << (k + 1) << "</text>\n";
        f << "<line x1='" << marginL << "' y1='" << (y + rowH) << "' x2='" << (marginL + chartW)
          << "' y2='" << (y + rowH) << "' stroke='#f0f0f0'/>\n";
    }

    // ---- operation bars (coloured by job) -----------------------------
    for (int j = 0; j < nJobs; ++j) {
        const string col = jobColor(j, nJobs);
        for (const Operation& op : inst.job(j).operations()) {
            const int gid = op.globalId;
            const int mc  = sched.machineOf(gid);
            const int st  = sched.startOf(gid);
            const int en  = sched.endOf(gid);
            const int y   = marginT + mc * (rowH + rowGap);
            const int x   = marginL + (int)(st * scale);
            const int w   = max(1, (int)((en - st) * scale));
            f << "<rect x='" << x << "' y='" << y << "' width='" << w << "' height='" << rowH
              << "' rx='3' fill='" << col << "' stroke='#333' stroke-width='0.6'>"
              << "<title>J" << (j + 1) << "  " << esc(op.label())
              << "  M" << (mc + 1) << "  [" << st << "," << en << "]</title></rect>\n";
            if (w >= 22)
                f << "<text x='" << (x + w / 2) << "' y='" << (y + rowH * 0.66)
                  << "' font-size='10' fill='#111' text-anchor='middle'>J" << (j + 1)
                  << "." << (op.positionInJob + 1) << "</text>\n";
        }
    }

    // ---- legend (job -> colour) ---------------------------------------
    const int legendY = marginT + plotH + 34;
    f << "<text x='" << marginL << "' y='" << legendY << "' font-size='12' font-weight='bold'>Jobs</text>\n";
    for (int j = 0; j < nJobs; ++j) {
        const int cx = marginL + (j % legendCols) * legendColW;
        const int cy = legendY + 12 + (j / legendCols) * legendRowH;
        f << "<rect x='" << cx << "' y='" << cy << "' width='" << legendSwatch << "' height='" << legendSwatch
          << "' rx='2' fill='" << jobColor(j, nJobs) << "' stroke='#333' stroke-width='0.5'/>\n";
        f << "<text x='" << (cx + legendSwatch + 5) << "' y='" << (cy + legendSwatch - 2)
          << "' font-size='11'>J" << (j + 1) << "</text>\n";
    }

    f << "</svg>\n";
}

} // namespace fjs
