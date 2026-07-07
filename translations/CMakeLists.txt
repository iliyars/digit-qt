set(DIGITQT_TS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/DigitQt_ru.ts)

qt6_add_translation(DIGITQT_QM_FILES ${DIGITQT_TS_FILES})
add_custom_target(translations ALL DEPENDS ${DIGITQT_QM_FILES})
add_dependencies(DigitQt translations)

foreach(qm_file ${DIGITQT_QM_FILES})
    add_custom_command(TARGET translations POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:DigitQt>/translations"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${qm_file}"
                "$<TARGET_FILE_DIR:DigitQt>/translations/"
    )
endforeach()

# Dev-time convenience: `cmake --build build --target update_translations`
# re-scans src/ for tr(...) calls and merges any new/changed strings into
# the .ts files above, ready to hand to a translator in Qt Linguist.
find_program(QT_LUPDATE_EXECUTABLE NAMES lupdate lupdate6)
if(QT_LUPDATE_EXECUTABLE)
    add_custom_target(update_translations
        COMMAND ${QT_LUPDATE_EXECUTABLE} ${CMAKE_SOURCE_DIR}/src -ts ${DIGITQT_TS_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Scanning src/ for tr() calls and updating translations/*.ts"
    )
endif()


