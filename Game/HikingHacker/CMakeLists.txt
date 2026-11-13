find_package(SDL3 REQUIRED)
find_package(SDL3_image REQUIRED)
find_package(glm REQUIRED)
find_package(tinyxml2 REQUIRED)
find_package(SDL3_ttf REQUIRED)
add_executable (HikingHacker WIN32 "main.cpp"  "timer.h" "animation.h" "gameobject.h" "tmx.h" "tmx.cpp"  )

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET HikingHacker PROPERTY CXX_STANDARD 20)
endif()

target_link_libraries(HikingHacker PRIVATE SDL3::SDL3 SDL3_image::SDL3_image glm::glm tinyxml2::tinyxml2 SDL3_ttf::SDL3_ttf)