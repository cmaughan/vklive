find_package(SDL2 REQUIRED)

macro(add_project_meta FILES_TO_INCLUDE)
if (NOT RESOURCE_FOLDER)
    set(RESOURCE_FOLDER ${CMAKE_CURRENT_BINARY_DIR}/../res)
endif()

if (NOT ICON_NAME)
    set(ICON_NAME AppIcon)
endif()

if (APPLE)
    set(ICON_FILE ${RESOURCE_FOLDER}/${ICON_NAME}.icns)
elseif (WIN32)
    set(ICON_FILE ${RESOURCE_FOLDER}/${ICON_NAME}.ico)
endif()

set(IMGUI_FILES
        ${RESOURCE_FOLDER}/Cousine-Regular.ttf
        ${RESOURCE_FOLDER}/imgui.ini)

# Get a list of all of the files in the runtree
file(GLOB_RECURSE RUN_TREE_RESOURCES "${REZONALITY_ROOT}/run_tree/*")

# Individually set the file's path properties.
foreach (FILE ${RUN_TREE_RESOURCES})

    # Get the relative path from the data-folder to the particular file.
    file(RELATIVE_PATH NEW_FILE ${REZONALITY_ROOT} ${FILE})

    # Get the relative path to the file.
    get_filename_component(NEW_FILE_PATH ${NEW_FILE} DIRECTORY)
    
    # Set it's location inside the app package (under Resources).
    set_property(SOURCE ${FILE} PROPERTY MACOSX_PACKAGE_LOCATION "Resources/${NEW_FILE_PATH}")
    set_property(SOURCE ${FILE} PROPERTY HEADER_FILE_ONLY TRUE)

    # Optional: Add the file to the 'Resources' folder group in Xcode.
    #           This also preserves folder structure.
    source_group("Resources/${FILE}" FILES "${FILE}")
endforeach ()

LIST(APPEND RESOURCE_DEPLOY_FILES ${RUN_TREE_RESOURCES})
if (WIN32)
    configure_file("${APP_ROOT}/cmake/windows_metafile.rc.in"
      "windows_metafile.rc"
    )

set(RES_FILES windows_metafile.rc ${APP_ROOT}/res/app.manifest)
endif()

if (APPLE)
    set_source_files_properties(${ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_source_files_properties(${IMGUI_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

    # Identify MacOS bundle
    set(MACOSX_BUNDLE_BUNDLE_NAME ${PROJECT_NAME})
    set(MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
    set(MACOSX_BUNDLE_LONG_VERSION_STRING ${PROJECT_VERSION})
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
    set(MACOSX_BUNDLE_COPYRIGHT ${COPYRIGHT})
    set(MACOSX_BUNDLE_GUI_IDENTIFIER ${IDENTIFIER})
    set(MACOSX_BUNDLE_ICON_FILE ${ICON_NAME})
endif()

if (APPLE)
    set(${FILES_TO_INCLUDE} ${ICON_FILE} ${RESOURCE_DEPLOY_FILES})
elseif (WIN32)
    set(${FILES_TO_INCLUDE} ${RES_FILES})
endif()

endmacro()

macro(init_os_bundle)
if (APPLE)
    set(OS_BUNDLE MACOSX_BUNDLE)
elseif (WIN32)
    set(OS_BUNDLE WIN32)
endif()
endmacro()

macro(fix_win_compiler)
if (MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE YES
        LINK_FLAGS "/ENTRY:mainCRTStartup"
    )
endif()
endmacro()

init_os_bundle()


