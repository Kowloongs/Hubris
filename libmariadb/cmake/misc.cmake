IF ("${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}.${CMAKE_PATCH_VERSION}" VERSION_LESS "2.8.7")
  FUNCTION(MESSAGE1 id out)
    MESSAGE(STATUS "${out}")
  ENDFUNCTION()
ELSE()
  FUNCTION(MESSAGE1 id out)
    STRING(MD5 hash "${out}")
    IF(NOT __msg1_${id} STREQUAL "${hash}")
      MESSAGE(STATUS "${out}")
    ENDIF()
    SET(__msg1_${id} ${hash} CACHE INTERNAL "")
  ENDFUNCTION()
ENDIF()
