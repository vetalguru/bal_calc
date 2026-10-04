#ifndef BALLISTICS_STANDARD_DRAG_TABLES_H
#define BALLISTICS_STANDARD_DRAG_TABLES_H

#include <cstddef>

#include <ballistics/drag.h>

namespace ballistics::detail {

template <typename T>
struct Span {
    const T* data;
    std::size_t size;
};

Span<DragPoint> StandardTable(DragTableId id);

} // namespace ballistics::detail

#endif // BALLISTICS_STANDARD_DRAG_TABLES_H
