set(SO "webserv${CMAKE_SHARED_LIBRARY_SUFFIX}")

add_library(${SO} MODULE
  webserv.cpp
  webserv_core.cpp
)

set_target_properties(${SO} PROPERTIES
  BUILD_WITH_INSTALL_RPATH ON
  FOLDER "Modules"
  INSTALL_RPATH_USE_LINK_PATH ON
  PREFIX ""
  SUFFIX ""
)

add_dependencies(${SO} ${PROGRAM_NAME})
if(HAVE_LOCALIZATION)
  add_dependencies(${SO} module_language)
endif()

# libcurl for outbound HTTP requests to the Django API.
find_package(CURL REQUIRED)
target_include_directories(${SO} PRIVATE ${CURL_INCLUDE_DIRS})
target_link_libraries(${SO} PRIVATE ${CURL_LIBRARIES})

# For Windows only, have the module link to the export library of Anope.
if(WIN32)
  target_link_libraries(${SO} PUBLIC ${PROGRAM_NAME})
  set_target_properties(${PROGRAM_NAME} PROPERTIES VERSION "${VERSION_DOTTED}")
elseif(APPLE)
  target_link_libraries(${SO} PUBLIC ${PROGRAM_NAME})
endif()

install(TARGETS ${SO}
  DESTINATION ${MODULE_DIR}
  LIBRARY)

install(FILES webserv.example.conf
  DESTINATION ${CONF_DIR}
)
