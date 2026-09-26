set(VC_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/volumetric_clouds_generated")
set(VC_HEADER "#pragma once\n\n#include <string_view>\n\nnamespace clouds::shaders\n{\n")
foreach (VC_SHADER IN ITEMS common depth trace reconstruct composite)
    set(VC_FILE "${CURRENT_MOD_DIR}/shaders/${VC_SHADER}.hlsl")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${VC_FILE}")
    file(READ "${VC_FILE}" VC_HEX HEX)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," VC_BYTES "${VC_HEX}")
    string(APPEND VC_HEADER "\tinline constexpr unsigned char k_${VC_SHADER}[] = {${VC_BYTES}0x0a};\n")
    string(APPEND VC_HEADER "\tinline const std::string_view ${VC_SHADER}{reinterpret_cast<const char*>(k_${VC_SHADER}), sizeof(k_${VC_SHADER})};\n\n")
endforeach ()
string(APPEND VC_HEADER "}\n")
file(CONFIGURE OUTPUT "${VC_GENERATED_DIR}/shaders_embedded.hpp" CONTENT "${VC_HEADER}" @ONLY)

file(GLOB VC_SOURCES CONFIGURE_DEPENDS "${CURRENT_MOD_DIR}/*.cpp" "${CURRENT_MOD_DIR}/*.hpp")
create_custom_roblox_mod(
        SOURCES ${VC_SOURCES}
        INCLUDE_DIRS ${VC_GENERATED_DIR}
)
