#pragma once

#include "GalleryPreviewMetrics.h"

#include <QString>

QString GalleryPerformanceDiagnosticText(
    const GalleryPreviewMetricsSnapshot& metrics);
