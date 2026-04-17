#include "aoi.hpp"
#include <cmath>

namespace gs {
namespace realtime {

GridSpatialIndex::GridSpatialIndex(float map_w, float map_h, float cell_size)
    : map_w_(map_w), map_h_(map_h), cell_size_(cell_size) {
    cols_ = static_cast<int>(std::ceil(map_w / cell_size));
    rows_ = static_cast<int>(std::ceil(map_h / cell_size));
    if (cols_ < 1) cols_ = 1;
    if (rows_ < 1) rows_ = 1;
    cells_.resize(cols_ * rows_);
}

void GridSpatialIndex::Clear() {
    for (auto& cell : cells_) {
        cell.clear();
    }
}

void GridSpatialIndex::Insert(uint64_t player_id, float x, float z) {
    int cx = CoordToCell(x);
    int cz = CoordToCell(z);
    int idx = CellIndex(cx, cz);
    if (idx >= 0 && idx < static_cast<int>(cells_.size())) {
        cells_[idx].push_back(player_id);
    }
}

std::vector<uint64_t> GridSpatialIndex::QueryRange(float x, float z, float radius) const {
    std::vector<uint64_t> out;
    // 计算半径覆盖的网格范围
    int min_cx = CoordToCell(x - radius);
    int max_cx = CoordToCell(x + radius);
    int min_cz = CoordToCell(z - radius);
    int max_cz = CoordToCell(z + radius);

    // 限制在有效范围内
    if (min_cx < 0) min_cx = 0;
    if (max_cx >= cols_) max_cx = cols_ - 1;
    if (min_cz < 0) min_cz = 0;
    if (max_cz >= rows_) max_cz = rows_ - 1;

    float r2 = radius * radius;
    for (int cz = min_cz; cz <= max_cz; ++cz) {
        for (int cx = min_cx; cx <= max_cx; ++cx) {
            int idx = CellIndex(cx, cz);
            for (uint64_t pid : cells_[idx]) {
                // 这里不做精确距离过滤，因为 Room 里会再次过滤
                // 简化：网格内的全部返回，MVP 阶段精度足够
                out.push_back(pid);
            }
        }
    }
    return out;
}

int GridSpatialIndex::CoordToCell(float v) const {
    return static_cast<int>(v / cell_size_);
}

int GridSpatialIndex::CellIndex(int cx, int cz) const {
    if (cx < 0 || cx >= cols_ || cz < 0 || cz >= rows_) return -1;
    return cz * cols_ + cx;
}

} // namespace realtime
} // namespace gs
