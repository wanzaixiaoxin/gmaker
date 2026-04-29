#pragma once

#include <cstdint>
#include <vector>
#include <unordered_set>

namespace gs {
namespace realtime {

// SpatialIndex AOI 空间索引接口
class SpatialIndex {
public:
    virtual ~SpatialIndex() = default;
    virtual void Clear() = 0;
    virtual void Insert(uint64_t player_id, float x, float z) = 0;
    virtual std::vector<uint64_t> QueryRange(float x, float z, float radius) const = 0;
};

// GridSpatialIndex 均匀网格空间索引（九宫格简化版）
class GridSpatialIndex : public SpatialIndex {
public:
    GridSpatialIndex(float map_w, float map_h, float cell_size);

    void Clear() override;
    void Insert(uint64_t player_id, float x, float z) override;
    std::vector<uint64_t> QueryRange(float x, float z, float radius) const override;

private:
    int CoordToCell(float v) const;
    int CellIndex(int cx, int cz) const;

    float map_w_, map_h_, cell_size_;
    int cols_, rows_;
    std::vector<std::vector<uint64_t>> cells_;
};

} // namespace realtime
} // namespace gs
