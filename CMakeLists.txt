cmake_minimum_required(VERSION 3.10)
project(radxa-penta-fan-ctrl VERSION 1.0.1 LANGUAGES C CXX)

# Set C and C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Threads REQUIRED)
find_library(GPIOD_LIBRARY gpiod REQUIRED)

# Third-party dependency: ssd1306 (pin to exact commit for reproducibility)
# Default: expect git submodule present at lib/ssd1306 (preferred for packaging, no network)
# Optional: allow fetching via FetchContent when submodule is absent or for developer convenience
option(RADXA_PENTA_USE_FETCHCONTENT "Fetch ssd1306 with CMake instead of using submodule" OFF)

if (EXISTS ${CMAKE_SOURCE_DIR}/lib/ssd1306/CMakeLists.txt AND NOT RADXA_PENTA_USE_FETCHCONTENT)
    add_subdirectory(lib/ssd1306)
else()
    include(FetchContent)
    FetchContent_Declare(
        ssd1306
        GIT_REPOSITORY https://github.com/lexus2k/ssd1306.git
        GIT_TAG 1c2c71043d981b756ec39129abc57440a59e5d3e # pinned
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(ssd1306)
endif()


# Source files
set(SOURCES
    src/main.c
    src/config.c
    src/fan.c
    src/thermal.c
    src/oled.c
    src/button.c
)

# Create executable
add_executable(radxa-penta-fan-ctrl ${SOURCES})

# Local includes (project headers) and system includes (third-party to suppress warnings)
target_include_directories(radxa-penta-fan-ctrl PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_include_directories(radxa-penta-fan-ctrl SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/ssd1306/src
)

# Apply strict warning flags only to our source files
target_compile_options(radxa-penta-fan-ctrl PRIVATE
    -Wall                   # Enable most common warnings
    -Wextra                 # Enable extra warnings
    -Wpedantic              # Strict ISO C/C++ compliance
    -Wformat=2              # Extra format string checks
    -Wformat-overflow=2     # Check for format overflow
    -Wformat-truncation=2   # Check for format truncation
    -Wnull-dereference      # Warn about null pointer dereferences
    -Wdouble-promotion      # Warn about implicit float to double promotion
    -Wshadow                # Warn when variable shadows another
    -Wconversion            # Warn on implicit type conversions
    -Wsign-conversion       # Warn on sign conversions
    -Wcast-qual             # Warn on casting away const
    -Wcast-align            # Warn on pointer cast alignment issues
    -Wstrict-overflow=5     # Warn about optimization assuming no overflow
    -Wundef                 # Warn on undefined macros in #if
    -Wwrite-strings         # Warn about writing to string literals
    -Wswitch-enum           # Warn about missing switch cases
    -Wunreachable-code      # Warn about unreachable code
    -Winit-self             # Warn about uninitialized self-initialization
    -Wstrict-prototypes     # Warn about missing prototypes (C only)
)

# Link libraries
target_link_libraries(radxa-penta-fan-ctrl
    ssd1306
    ${GPIOD_LIBRARY}
    Threads::Threads
    m
)

# Install target - FHS compliant paths
install(TARGETS radxa-penta-fan-ctrl DESTINATION bin)
# Install configuration under /etc/radxa-penta-fan-ctrl (absolute to avoid /usr/etc)
install(FILES radxa-penta-fan-ctrl.conf DESTINATION /etc/radxa-penta-fan-ctrl)
install(FILES radxa-penta-fan-ctrl.env DESTINATION /etc/radxa-penta-fan-ctrl)
install(FILES radxa-penta-fan-ctrl.service DESTINATION /lib/systemd/system)
