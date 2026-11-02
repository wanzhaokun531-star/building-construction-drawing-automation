#pragma once
#include "WriteContracts.h"
#include <memory>
class AcDbDatabase;
namespace bce {
struct PreparedLine {
    Point2 start;
    Point2 end;
    std::string layerName;
    short layerColor{3};
};
class PreparedBatch final {
public:
    static Result<std::unique_ptr<PreparedBatch>> create(const PreparedLine& line);
    ~PreparedBatch();
    PreparedBatch(const PreparedBatch&) = delete;
    PreparedBatch& operator=(const PreparedBatch&) = delete;
    PreparedBatch(PreparedBatch&&) = delete;
    PreparedBatch& operator=(PreparedBatch&&) = delete;
    AcDbDatabase* database() noexcept;
    const PreparedLine& line() const noexcept;
    Result<ResourceDefinition> layerDefinition() const;
    bool validateOrdinaryLine() const;
private:
    struct Impl;
    explicit PreparedBatch(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};
}
