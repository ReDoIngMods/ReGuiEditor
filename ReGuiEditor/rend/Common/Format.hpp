#pragma once
#include <cstdint>

namespace rend {
	enum class Format : uint8_t {
		Unknown = 0,                                   // DXGI_FORMAT_UNKNOWN
		R32G32B32A32_TYPELESS = 1,                     // DXGI_FORMAT_R32G32B32A32_TYPELESS
		R32G32B32A32_FLOAT = 2,                        // DXGI_FORMAT_R32G32B32A32_FLOAT
		R32G32B32A32_UINT = 3,                         // DXGI_FORMAT_R32G32B32A32_UINT
		R32G32B32A32_SINT = 4,                         // DXGI_FORMAT_R32G32B32A32_SINT
		R32G32B32_TYPELESS = 5,                        // DXGI_FORMAT_R32G32B32_TYPELESS
		R32G32B32_FLOAT = 6,                           // DXGI_FORMAT_R32G32B32_FLOAT
		R32G32B32_UINT = 7,                            // DXGI_FORMAT_R32G32B32_UINT
		R32G32B32_SINT = 8,                            // DXGI_FORMAT_R32G32B32_SINT
		R16G16B16A16_TYPELESS = 9,                     // DXGI_FORMAT_R16G16B16A16_TYPELESS
		R16G16B16A16_FLOAT = 10,                       // DXGI_FORMAT_R16G16B16A16_FLOAT
		R16G16B16A16_UNORM = 11,                       // DXGI_FORMAT_R16G16B16A16_UNORM
		R16G16B16A16_UINT = 12,                        // DXGI_FORMAT_R16G16B16A16_UINT
		R16G16B16A16_SNORM = 13,                       // DXGI_FORMAT_R16G16B16A16_SNORM
		R16G16B16A16_SINT = 14,                        // DXGI_FORMAT_R16G16B16A16_SINT
		R32G32_TYPELESS = 15,                          // DXGI_FORMAT_R32G32_TYPELESS
		R32G32_FLOAT = 16,                             // DXGI_FORMAT_R32G32_FLOAT
		R32G32_UINT = 17,                              // DXGI_FORMAT_R32G32_UINT
		R32G32_SINT = 18,                              // DXGI_FORMAT_R32G32_SINT
		R32G8X24_TYPELESS = 19,                        // DXGI_FORMAT_R32G8X24_TYPELESS
		D32_FLOAT_S8X24_UINT = 20,                     // DXGI_FORMAT_D32_FLOAT_S8X24_UINT
		R32_FLOAT_X8X24_TYPELESS = 21,                 // DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS
		X32_TYPELESS_G8X24_UINT = 22,                  // DXGI_FORMAT_X32_TYPELESS_G8X24_UINT
		R10G10B10A2_TYPELESS = 23,                     // DXGI_FORMAT_R10G10B10A2_TYPELESS
		R10G10B10A2_UNORM = 24,                        // DXGI_FORMAT_R10G10B10A2_UNORM
		R10G10B10A2_UINT = 25,                         // DXGI_FORMAT_R10G10B10A2_UINT
		R11G11B10_FLOAT = 26,                          // DXGI_FORMAT_R11G11B10_FLOAT
		R8G8B8A8_TYPELESS = 27,                        // DXGI_FORMAT_R8G8B8A8_TYPELESS
		R8G8B8A8_UNORM = 28,                           // DXGI_FORMAT_R8G8B8A8_UNORM
		R8G8B8A8_UNORM_SRGB = 29,                      // DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
		R8G8B8A8_UINT = 30,                            // DXGI_FORMAT_R8G8B8A8_UINT
		R8G8B8A8_SNORM = 31,                           // DXGI_FORMAT_R8G8B8A8_SNORM
		R8G8B8A8_SINT = 32,                            // DXGI_FORMAT_R8G8B8A8_SINT
		R16G16_TYPELESS = 33,                          // DXGI_FORMAT_R16G16_TYPELESS
		R16G16_FLOAT = 34,                             // DXGI_FORMAT_R16G16_FLOAT
		R16G16_UNORM = 35,                             // DXGI_FORMAT_R16G16_UNORM
		R16G16_UINT = 36,                              // DXGI_FORMAT_R16G16_UINT
		R16G16_SNORM = 37,                             // DXGI_FORMAT_R16G16_SNORM
		R16G16_SINT = 38,                              // DXGI_FORMAT_R16G16_SINT
		R32_TYPELESS = 39,                             // DXGI_FORMAT_R32_TYPELESS
		D32_FLOAT = 40,                                // DXGI_FORMAT_D32_FLOAT
		R32_FLOAT = 41,                                // DXGI_FORMAT_R32_FLOAT
		R32_UINT = 42,                                 // DXGI_FORMAT_R32_UINT
		R32_SINT = 43,                                 // DXGI_FORMAT_R32_SINT
		R24G8_TYPELESS = 44,                           // DXGI_FORMAT_R24G8_TYPELESS
		D24_UNORM_S8_UINT = 45,                        // DXGI_FORMAT_D24_UNORM_S8_UINT
		R24_UNORM_X8_TYPELESS = 46,                    // DXGI_FORMAT_R24_UNORM_X8_TYPELESS
		X24_TYPELESS_G8_UINT = 47,                     // DXGI_FORMAT_X24_TYPELESS_G8_UINT
		R8G8_TYPELESS = 48,                            // DXGI_FORMAT_R8G8_TYPELESS
		R8G8_UNORM = 49,                               // DXGI_FORMAT_R8G8_UNORM
		R8G8_UINT = 50,                                // DXGI_FORMAT_R8G8_UINT
		R8G8_SNORM = 51,                               // DXGI_FORMAT_R8G8_SNORM
		R8G8_SINT = 52,                                // DXGI_FORMAT_R8G8_SINT
		R16_TYPELESS = 53,                             // DXGI_FORMAT_R16_TYPELESS
		R16_FLOAT = 54,                                // DXGI_FORMAT_R16_FLOAT
		D16_UNORM = 55,                                // DXGI_FORMAT_D16_UNORM
		R16_UNORM = 56,                                // DXGI_FORMAT_R16_UNORM
		R16_UINT = 57,                                 // DXGI_FORMAT_R16_UINT
		R16_SNORM = 58,                                // DXGI_FORMAT_R16_SNORM
		R16_SINT = 59,                                 // DXGI_FORMAT_R16_SINT
		R8_TYPELESS = 60,                              // DXGI_FORMAT_R8_TYPELESS
		R8_UNORM = 61,                                 // DXGI_FORMAT_R8_UNORM
		R8_UINT = 62,                                  // DXGI_FORMAT_R8_UINT
		R8_SNORM = 63,                                 // DXGI_FORMAT_R8_SNORM
		R8_SINT = 64,                                  // DXGI_FORMAT_R8_SINT
		A8_UNORM = 65,                                 // DXGI_FORMAT_A8_UNORM
		R1_UNORM = 66,                                 // DXGI_FORMAT_R1_UNORM
		R9G9B9E5_SHAREDEXP = 67,                       // DXGI_FORMAT_R9G9B9E5_SHAREDEXP
		R8G8_B8G8_UNORM = 68,                          // DXGI_FORMAT_R8G8_B8G8_UNORM
		G8R8_G8B8_UNORM = 69,                          // DXGI_FORMAT_G8R8_G8B8_UNORM
		BC1_TYPELESS = 70,                             // DXGI_FORMAT_BC1_TYPELESS
		BC1_UNORM = 71,                                // DXGI_FORMAT_BC1_UNORM
		BC1_UNORM_SRGB = 72,                           // DXGI_FORMAT_BC1_UNORM_SRGB
		BC2_TYPELESS = 73,                             // DXGI_FORMAT_BC2_TYPELESS
		BC2_UNORM = 74,                                // DXGI_FORMAT_BC2_UNORM
		BC2_UNORM_SRGB = 75,                           // DXGI_FORMAT_BC2_UNORM_SRGB
		BC3_TYPELESS = 76,                             // DXGI_FORMAT_BC3_TYPELESS
		BC3_UNORM = 77,                                // DXGI_FORMAT_BC3_UNORM
		BC3_UNORM_SRGB = 78,                           // DXGI_FORMAT_BC3_UNORM_SRGB
		BC4_TYPELESS = 79,                             // DXGI_FORMAT_BC4_TYPELESS
		BC4_UNORM = 80,                                // DXGI_FORMAT_BC4_UNORM
		BC4_SNORM = 81,                                // DXGI_FORMAT_BC4_SNORM
		BC5_TYPELESS = 82,                             // DXGI_FORMAT_BC5_TYPELESS
		BC5_UNORM = 83,                                // DXGI_FORMAT_BC5_UNORM
		BC5_SNORM = 84,                                // DXGI_FORMAT_BC5_SNORM
		B5G6R5_UNORM = 85,                             // DXGI_FORMAT_B5G6R5_UNORM
		B5G5R5A1_UNORM = 86,                           // DXGI_FORMAT_B5G5R5A1_UNORM
		B8G8R8A8_UNORM = 87,                           // DXGI_FORMAT_B8G8R8A8_UNORM
		B8G8R8X8_UNORM = 88,                           // DXGI_FORMAT_B8G8R8X8_UNORM
		R10G10B10_XR_BIAS_A2_UNORM = 89,               // DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM
		B8G8R8A8_TYPELESS = 90,                        // DXGI_FORMAT_B8G8R8A8_TYPELESS
		B8G8R8A8_UNORM_SRGB = 91,                      // DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
		B8G8R8X8_TYPELESS = 92,                        // DXGI_FORMAT_B8G8R8X8_TYPELESS
		B8G8R8X8_UNORM_SRGB = 93,                      // DXGI_FORMAT_B8G8R8X8_UNORM_SRGB
		BC6H_TYPELESS = 94,                            // DXGI_FORMAT_BC6H_TYPELESS
		BC6H_UF16 = 95,                                // DXGI_FORMAT_BC6H_UF16
		BC6H_SF16 = 96,                                // DXGI_FORMAT_BC6H_SF16
		BC7_TYPELESS = 97,                             // DXGI_FORMAT_BC7_TYPELESS
		BC7_UNORM = 98,                                // DXGI_FORMAT_BC7_UNORM
		BC7_UNORM_SRGB = 99,                           // DXGI_FORMAT_BC7_UNORM_SRGB
		AYUV = 100,                                    // DXGI_FORMAT_AYUV
		Y410 = 101,                                    // DXGI_FORMAT_Y410
		Y416 = 102,                                    // DXGI_FORMAT_Y416
		NV12 = 103,                                    // DXGI_FORMAT_NV12
		P010 = 104,                                    // DXGI_FORMAT_P010
		P016 = 105,                                    // DXGI_FORMAT_P016
		_420_OPAQUE = 106,                             // DXGI_FORMAT_420_OPAQUE
		YUY2 = 107,                                    // DXGI_FORMAT_YUY2
		Y210 = 108,                                    // DXGI_FORMAT_Y210
		Y216 = 109,                                    // DXGI_FORMAT_Y216
		NV11 = 110,                                    // DXGI_FORMAT_NV11
		AI44 = 111,                                    // DXGI_FORMAT_AI44
		IA44 = 112,                                    // DXGI_FORMAT_IA44
		P8 = 113,                                      // DXGI_FORMAT_P8
		A8P8 = 114,                                    // DXGI_FORMAT_A8P8
		B4G4R4A4_UNORM = 115,                          // DXGI_FORMAT_B4G4R4A4_UNORM
		P208 = 130,                                    // DXGI_FORMAT_P208
		V208 = 131,                                    // DXGI_FORMAT_V208
		V408 = 132,                                    // DXGI_FORMAT_V408
		SAMPLER_FEEDBACK_MIN_MIP_OPAQUE = 189,         // DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE
		SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE = 190, // DXGI_FORMAT_SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE
		A4B4G4R4_UNORM = 191,                          // DXGI_FORMAT_A4B4G4R4_UNORM
		COUNT
	};

	inline static uint8_t getFormatSize(rend::Format format) {
		switch (format) {
		case rend::Format::R32G32B32A32_TYPELESS:
		case rend::Format::R32G32B32A32_FLOAT:
		case rend::Format::R32G32B32A32_UINT:
		case rend::Format::R32G32B32A32_SINT:
		case rend::Format::BC2_TYPELESS:
		case rend::Format::BC2_UNORM:
		case rend::Format::BC2_UNORM_SRGB:
		case rend::Format::BC3_TYPELESS:
		case rend::Format::BC3_UNORM:
		case rend::Format::BC3_UNORM_SRGB:
		case rend::Format::BC5_TYPELESS:
		case rend::Format::BC5_UNORM:
		case rend::Format::BC5_SNORM:
		case rend::Format::BC6H_TYPELESS:
		case rend::Format::BC6H_UF16:
		case rend::Format::BC6H_SF16:
		case rend::Format::BC7_TYPELESS:
		case rend::Format::BC7_UNORM:
		case rend::Format::BC7_UNORM_SRGB:
			return 16;
		case rend::Format::R32G32B32_TYPELESS:
		case rend::Format::R32G32B32_FLOAT:
		case rend::Format::R32G32B32_UINT:
		case rend::Format::R32G32B32_SINT:
			return 12;
		case rend::Format::R16G16B16A16_TYPELESS:
		case rend::Format::R16G16B16A16_FLOAT:
		case rend::Format::R16G16B16A16_UNORM:
		case rend::Format::R16G16B16A16_UINT:
		case rend::Format::R16G16B16A16_SNORM:
		case rend::Format::R16G16B16A16_SINT:
		case rend::Format::R32G32_TYPELESS:
		case rend::Format::R32G32_FLOAT:
		case rend::Format::R32G32_UINT:
		case rend::Format::R32G32_SINT:
		case rend::Format::R32G8X24_TYPELESS:
		case rend::Format::D32_FLOAT_S8X24_UINT:
		case rend::Format::R32_FLOAT_X8X24_TYPELESS:
		case rend::Format::X32_TYPELESS_G8X24_UINT:
		case rend::Format::Y416:
		case rend::Format::BC1_TYPELESS:
		case rend::Format::BC1_UNORM:
		case rend::Format::BC1_UNORM_SRGB:
		case rend::Format::BC4_TYPELESS:
		case rend::Format::BC4_UNORM:
		case rend::Format::BC4_SNORM:
			return 8;
		case rend::Format::R10G10B10A2_TYPELESS:
		case rend::Format::R10G10B10A2_UNORM:
		case rend::Format::R10G10B10A2_UINT:
		case rend::Format::R11G11B10_FLOAT:
		case rend::Format::R8G8B8A8_TYPELESS:
		case rend::Format::R8G8B8A8_UNORM:
		case rend::Format::R8G8B8A8_UNORM_SRGB:
		case rend::Format::R8G8B8A8_UINT:
		case rend::Format::R8G8B8A8_SNORM:
		case rend::Format::R8G8B8A8_SINT:
		case rend::Format::R32_TYPELESS:
		case rend::Format::D32_FLOAT:
		case rend::Format::R32_FLOAT:
		case rend::Format::R32_UINT:
		case rend::Format::R32_SINT:
		case rend::Format::R24G8_TYPELESS:
		case rend::Format::D24_UNORM_S8_UINT:
		case rend::Format::R24_UNORM_X8_TYPELESS:
		case rend::Format::X24_TYPELESS_G8_UINT:
		case rend::Format::R10G10B10_XR_BIAS_A2_UNORM:
		case rend::Format::B8G8R8A8_TYPELESS:
		case rend::Format::B8G8R8A8_UNORM:
		case rend::Format::B8G8R8A8_UNORM_SRGB:
		case rend::Format::B8G8R8X8_TYPELESS:
		case rend::Format::B8G8R8X8_UNORM:
		case rend::Format::B8G8R8X8_UNORM_SRGB:
		case rend::Format::R9G9B9E5_SHAREDEXP:
		case rend::Format::R8G8_B8G8_UNORM:
		case rend::Format::G8R8_G8B8_UNORM:
		case rend::Format::Y410:
		case rend::Format::AYUV:
		case rend::Format::YUY2:
		case rend::Format::Y210:
			return 4;
		case rend::Format::R16G16_TYPELESS:
		case rend::Format::R16G16_FLOAT:
		case rend::Format::R16G16_UNORM:
		case rend::Format::R16G16_UINT:
		case rend::Format::R16G16_SNORM:
		case rend::Format::R16G16_SINT:
		case rend::Format::R16_TYPELESS:
		case rend::Format::R16_FLOAT:
		case rend::Format::D16_UNORM:
		case rend::Format::R16_UNORM:
		case rend::Format::R16_UINT:
		case rend::Format::R16_SNORM:
		case rend::Format::R16_SINT:
		case rend::Format::R8G8_TYPELESS:
		case rend::Format::R8G8_UNORM:
		case rend::Format::R8G8_UINT:
		case rend::Format::R8G8_SNORM:
		case rend::Format::R8G8_SINT:
		case rend::Format::B5G6R5_UNORM:
		case rend::Format::B5G5R5A1_UNORM:
		case rend::Format::B4G4R4A4_UNORM:
		case rend::Format::A4B4G4R4_UNORM:
		case rend::Format::A8P8:
			return 2;
		case rend::Format::R8_TYPELESS:
		case rend::Format::R8_UNORM:
		case rend::Format::R8_UINT:
		case rend::Format::R8_SNORM:
		case rend::Format::R8_SINT:
		case rend::Format::A8_UNORM:
		case rend::Format::P8:
		case rend::Format::AI44:
		case rend::Format::IA44:
		case rend::Format::R1_UNORM:
			return 1;
		default:
			return 0;
		}
	}

	inline static const char* getFormatName(rend::Format format) {
		switch (format) {
		case rend::Format::Unknown:                                  return "Unknown";
		case rend::Format::R32G32B32A32_TYPELESS:                    return "R32G32B32A32_TYPELESS";
		case rend::Format::R32G32B32A32_FLOAT:                       return "R32G32B32A32_FLOAT";
		case rend::Format::R32G32B32A32_UINT:                        return "R32G32B32A32_UINT";
		case rend::Format::R32G32B32A32_SINT:                        return "R32G32B32A32_SINT";
		case rend::Format::R32G32B32_TYPELESS:                       return "R32G32B32_TYPELESS";
		case rend::Format::R32G32B32_FLOAT:                          return "R32G32B32_FLOAT";
		case rend::Format::R32G32B32_UINT:                           return "R32G32B32_UINT";
		case rend::Format::R32G32B32_SINT:                           return "R32G32B32_SINT";
		case rend::Format::R16G16B16A16_TYPELESS:                    return "R16G16B16A16_TYPELESS";
		case rend::Format::R16G16B16A16_FLOAT:                       return "R16G16B16A16_FLOAT";
		case rend::Format::R16G16B16A16_UNORM:                       return "R16G16B16A16_UNORM";
		case rend::Format::R16G16B16A16_UINT:                        return "R16G16B16A16_UINT";
		case rend::Format::R16G16B16A16_SNORM:                       return "R16G16B16A16_SNORM";
		case rend::Format::R16G16B16A16_SINT:                        return "R16G16B16A16_SINT";
		case rend::Format::R32G32_TYPELESS:                          return "R32G32_TYPELESS";
		case rend::Format::R32G32_FLOAT:                             return "R32G32_FLOAT";
		case rend::Format::R32G32_UINT:                              return "R32G32_UINT";
		case rend::Format::R32G32_SINT:                              return "R32G32_SINT";
		case rend::Format::R32G8X24_TYPELESS:                        return "R32G8X24_TYPELESS";
		case rend::Format::D32_FLOAT_S8X24_UINT:                     return "D32_FLOAT_S8X24_UINT";
		case rend::Format::R32_FLOAT_X8X24_TYPELESS:                 return "R32_FLOAT_X8X24_TYPELESS";
		case rend::Format::X32_TYPELESS_G8X24_UINT:                  return "X32_TYPELESS_G8X24_UINT";
		case rend::Format::R10G10B10A2_TYPELESS:                     return "R10G10B10A2_TYPELESS";
		case rend::Format::R10G10B10A2_UNORM:                        return "R10G10B10A2_UNORM";
		case rend::Format::R10G10B10A2_UINT:                         return "R10G10B10A2_UINT";
		case rend::Format::R11G11B10_FLOAT:                          return "R11G11B10_FLOAT";
		case rend::Format::R8G8B8A8_TYPELESS:                        return "R8G8B8A8_TYPELESS";
		case rend::Format::R8G8B8A8_UNORM:                           return "R8G8B8A8_UNORM";
		case rend::Format::R8G8B8A8_UNORM_SRGB:                      return "R8G8B8A8_UNORM_SRGB";
		case rend::Format::R8G8B8A8_UINT:                            return "R8G8B8A8_UINT";
		case rend::Format::R8G8B8A8_SNORM:                           return "R8G8B8A8_SNORM";
		case rend::Format::R8G8B8A8_SINT:                            return "R8G8B8A8_SINT";
		case rend::Format::R16G16_TYPELESS:                          return "R16G16_TYPELESS";
		case rend::Format::R16G16_FLOAT:                             return "R16G16_FLOAT";
		case rend::Format::R16G16_UNORM:                             return "R16G16_UNORM";
		case rend::Format::R16G16_UINT:                              return "R16G16_UINT";
		case rend::Format::R16G16_SNORM:                             return "R16G16_SNORM";
		case rend::Format::R16G16_SINT:                              return "R16G16_SINT";
		case rend::Format::R32_TYPELESS:                             return "R32_TYPELESS";
		case rend::Format::D32_FLOAT:                                return "D32_FLOAT";
		case rend::Format::R32_FLOAT:                                return "R32_FLOAT";
		case rend::Format::R32_UINT:                                 return "R32_UINT";
		case rend::Format::R32_SINT:                                 return "R32_SINT";
		case rend::Format::R24G8_TYPELESS:                           return "R24G8_TYPELESS";
		case rend::Format::D24_UNORM_S8_UINT:                        return "D24_UNORM_S8_UINT";
		case rend::Format::R24_UNORM_X8_TYPELESS:                    return "R24_UNORM_X8_TYPELESS";
		case rend::Format::X24_TYPELESS_G8_UINT:                     return "X24_TYPELESS_G8_UINT";
		case rend::Format::R8G8_TYPELESS:                            return "R8G8_TYPELESS";
		case rend::Format::R8G8_UNORM:                               return "R8G8_UNORM";
		case rend::Format::R8G8_UINT:                                return "R8G8_UINT";
		case rend::Format::R8G8_SNORM:                               return "R8G8_SNORM";
		case rend::Format::R8G8_SINT:                                return "R8G8_SINT";
		case rend::Format::R16_TYPELESS:                             return "R16_TYPELESS";
		case rend::Format::R16_FLOAT:                                return "R16_FLOAT";
		case rend::Format::D16_UNORM:                                return "D16_UNORM";
		case rend::Format::R16_UNORM:                                return "R16_UNORM";
		case rend::Format::R16_UINT:                                 return "R16_UINT";
		case rend::Format::R16_SNORM:                                return "R16_SNORM";
		case rend::Format::R16_SINT:                                 return "R16_SINT";
		case rend::Format::R8_TYPELESS:                              return "R8_TYPELESS";
		case rend::Format::R8_UNORM:                                 return "R8_UNORM";
		case rend::Format::R8_UINT:                                  return "R8_UINT";
		case rend::Format::R8_SNORM:                                 return "R8_SNORM";
		case rend::Format::R8_SINT:                                  return "R8_SINT";
		case rend::Format::A8_UNORM:                                 return "A8_UNORM";
		case rend::Format::R1_UNORM:                                 return "R1_UNORM";
		case rend::Format::R9G9B9E5_SHAREDEXP:                       return "R9G9B9E5_SHAREDEXP";
		case rend::Format::R8G8_B8G8_UNORM:                          return "R8G8_B8G8_UNORM";
		case rend::Format::G8R8_G8B8_UNORM:                          return "G8R8_G8B8_UNORM";
		case rend::Format::BC1_TYPELESS:                             return "BC1_TYPELESS";
		case rend::Format::BC1_UNORM:                                return "BC1_UNORM";
		case rend::Format::BC1_UNORM_SRGB:                           return "BC1_UNORM_SRGB";
		case rend::Format::BC2_TYPELESS:                             return "BC2_TYPELESS";
		case rend::Format::BC2_UNORM:                                return "BC2_UNORM";
		case rend::Format::BC2_UNORM_SRGB:                           return "BC2_UNORM_SRGB";
		case rend::Format::BC3_TYPELESS:                             return "BC3_TYPELESS";
		case rend::Format::BC3_UNORM:                                return "BC3_UNORM";
		case rend::Format::BC3_UNORM_SRGB:                           return "BC3_UNORM_SRGB";
		case rend::Format::BC4_TYPELESS:                             return "BC4_TYPELESS";
		case rend::Format::BC4_UNORM:                                return "BC4_UNORM";
		case rend::Format::BC4_SNORM:                                return "BC4_SNORM";
		case rend::Format::BC5_TYPELESS:                             return "BC5_TYPELESS";
		case rend::Format::BC5_UNORM:                                return "BC5_UNORM";
		case rend::Format::BC5_SNORM:                                return "BC5_SNORM";
		case rend::Format::B5G6R5_UNORM:                             return "B5G6R5_UNORM";
		case rend::Format::B5G5R5A1_UNORM:                           return "B5G5R5A1_UNORM";
		case rend::Format::B8G8R8A8_UNORM:                           return "B8G8R8A8_UNORM";
		case rend::Format::B8G8R8X8_UNORM:                           return "B8G8R8X8_UNORM";
		case rend::Format::R10G10B10_XR_BIAS_A2_UNORM:               return "R10G10B10_XR_BIAS_A2_UNORM";
		case rend::Format::B8G8R8A8_TYPELESS:                        return "B8G8R8A8_TYPELESS";
		case rend::Format::B8G8R8A8_UNORM_SRGB:                      return "B8G8R8A8_UNORM_SRGB";
		case rend::Format::B8G8R8X8_TYPELESS:                        return "B8G8R8X8_TYPELESS";
		case rend::Format::B8G8R8X8_UNORM_SRGB:                      return "B8G８R８X８_UNORM_SRGB";
		case rend::Format::BC6H_TYPELESS:                            return "BC6H_TYPELESS";
		case rend::Format::BC6H_UF16:                                return "BC6H_UF16";
		case rend::Format::BC6H_SF16:                                return "BC6H_SF16";
		case rend::Format::BC7_TYPELESS:                             return "BC7_TYPELESS";
		case rend::Format::BC7_UNORM:                                return "BC7_UNORM";
		case rend::Format::BC7_UNORM_SRGB:                           return "BC7_UNORM_SRGB";
		case rend::Format::AYUV:                                     return "AYUV";
		case rend::Format::Y410:                                     return "Y410";
		case rend::Format::Y416:                                     return "Y416";
		case rend::Format::NV12:                                     return "NV12";
		case rend::Format::P010:                                     return "P010";
		case rend::Format::P016:                                     return "P016";
		case rend::Format::_420_OPAQUE:                              return "420_OPAQUE";
		case rend::Format::YUY2:                                     return "YUY2";
		case rend::Format::Y210:                                     return "Y210";
		case rend::Format::Y216:                                     return "Y216";
		case rend::Format::NV11:                                     return "NV11";
		case rend::Format::AI44:                                     return "AI44";
		case rend::Format::IA44:                                     return "IA44";
		case rend::Format::P8:                                       return "P8";
		case rend::Format::A8P8:                                     return "A8P8";
		case rend::Format::B4G4R4A4_UNORM:                           return "B4G4R4A4_UNORM";
		case rend::Format::P208:                                     return "P208";
		case rend::Format::V208:                                     return "V208";
		case rend::Format::V408:                                     return "V408";
		case rend::Format::SAMPLER_FEEDBACK_MIN_MIP_OPAQUE:          return "SAMPLER_FEEDBACK_MIN_MIP_OPAQUE";
		case rend::Format::SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE:  return "SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE";
		case rend::Format::A4B4G4R4_UNORM:                           return "A4B4G4R4_UNORM";
		default: return "Unknown Format";
		}
	}
}