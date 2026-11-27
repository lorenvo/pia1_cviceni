# #####################################################################
#  CMakeLists.txt - návod pro sestavení programu
# #####################################################################
#  Použití (ze složky projektu):
#    cmake -S . -B build        # jednou: připraví sestavení do složky build/
#    cmake --build build -j     # pokaždé: přeloží jen změněné soubory
#    ./build/segmentace         # spuštění
# #####################################################################

cmake_minimum_required(VERSION 3.16)
project(SegmentaceBunek CXX)

# --- standard C++17 (kvůli std::filesystem a inline konstantám) ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- výchozí režim Release = zapnuté optimalizace (rychlejší výpočet) ---
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# --- program a jeho zdrojové soubory ---
add_executable(segmentace
    src/main.cpp
    src/obrazek.cpp
    src/segmentace.cpp
    src/sousede.cpp
    src/simulace.cpp
    src/kresleni.cpp
    src/export.cpp
)

# --- kde hledat hlavičky (#include "...") ---
target_include_directories(segmentace PRIVATE src external/stb)

# --- varování překladače (pomáhají najít chyby) ---
target_compile_options(segmentace PRIVATE -Wall -Wextra)

# --- cesta ke složce projektu -> program najde data/ odkudkoli ---
target_compile_definitions(segmentace PRIVATE PROJEKT_DIR="${CMAKE_SOURCE_DIR}/")
