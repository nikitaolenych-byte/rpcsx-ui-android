add_library(kernel INTERFACE)
target_include_directories(kernel INTERFACE include)

if (WITH_PS3)
    add_subdirectory(cellos)
endif()

if (WITH_PS4)
    add_subdirectory(orbis)
endif()
