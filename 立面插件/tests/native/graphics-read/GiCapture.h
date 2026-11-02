#pragma once
#include "NativeEvidence.h"
#include "GraphicsPolicy.h"
#include "GraphicsControlPolicy.h"
#include <dbmain.h>
#include <acgi.h>
#include <memory>
#include <functional>
namespace bce::graphics {
struct Result {
    Coverage coverage;
    std::vector<std::string> callbacks, textRequests, limits, metrics;
    std::size_t textCount{};
    ControlObservation observation;
    std::string json() const;
};
// Source entity is never passed here. Entity and all dependencies must belong to db.
Result capture(AcGiDrawable& entity, AcDbDatabase& db,
               const std::function<void(const std::string&)>& log);
Result controlProtocol(AcDbDatabase& db, const std::function<void(const std::string&)>& log);
Result controlViewportProtocol(AcDbDatabase& db, const std::function<void(const std::string&)>& log,bool nested);
std::string number(double value);
std::string handle(AcDbObjectId id);
}
