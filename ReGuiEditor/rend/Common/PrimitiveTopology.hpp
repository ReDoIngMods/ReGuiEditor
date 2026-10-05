#pragma once
#include <cstdint>

namespace rend {
	enum class PrimitiveTopology : uint8_t {
		PointList = 1,          // D3D11_PRIMITIVE_TOPOLOGY_POINTLIST
		LineList = 2,           // D3D11_PRIMITIVE_TOPOLOGY_LINELIST
		LineStrip = 3,          // D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP
		TriangleList = 4,       // D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
		TriangleStrip = 5,      // D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
		LineListAdj = 10,       // D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ
		LineStripAdj = 11,      // D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ
		TriangleListAdj = 12,   // D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ
		TriangleStripAdj = 13,  // D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ
	};

	inline static const char* primitiveTopologyToString(rend::PrimitiveTopology topology) {
		switch (topology) {
		case rend::PrimitiveTopology::PointList:        return "PointList";
		case rend::PrimitiveTopology::LineList:         return "LineList";
		case rend::PrimitiveTopology::LineStrip:        return "LineStrip";
		case rend::PrimitiveTopology::TriangleList:     return "TriangleList";
		case rend::PrimitiveTopology::TriangleStrip:    return "TriangleStrip";
		case rend::PrimitiveTopology::LineListAdj:      return "LineListAdj";
		case rend::PrimitiveTopology::LineStripAdj:     return "LineStripAdj";
		case rend::PrimitiveTopology::TriangleListAdj:  return "TriangleListAdj";
		case rend::PrimitiveTopology::TriangleStripAdj: return "TriangleStripAdj";
		default: return "Unknown";
		}
	}
}