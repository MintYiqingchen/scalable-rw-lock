cmake_minimum_required(VERSION 2.8)
project(rwlock)

set(CMAKE_CXX_STANDARD 11)
MESSAGE(“MODEL_CHK_PATH=${MODEL_CHK_PATH}”)
# baseline 1
add_executable(pthreadRwlock pthreadRWLock.cpp)
TARGET_LINK_LIBRARIES(pthreadRwlock pthread)

# INCLUDE_DIRECTORIES("/home/mintyi/codework/model-checker/include")
IF(DEFINED MODEL_CHK_PATH)
    ADD_DEFINITIONS(-DWITH_CDS)
    INCLUDE_DIRECTORIES("${MODEL_CHK_PATH}/include")
    LINK_DIRECTORIES("${MODEL_CHK_PATH}")
ENDIF()
# baseline 2
add_executable(simpleRwlock simpleRWLock.cpp)
IF(DEFINED MODEL_CHK_PATH)
    TARGET_LINK_LIBRARIES(simpleRwlock "model")
ELSE()
    TARGET_LINK_LIBRARIES(simpleRwlock pthread)
ENDIF()