cmake_minimum_required(VERSION 3.2)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Define la variable de entorno VITASDK.")
  endif()
endif()

project(OpenCity C)
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "Open City Prototype")
set(VITA_TITLEID  "OPNC00001")
set(VITA_VERSION  "01.00")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -O2 -std=gnu99")

add_executable(${PROJECT_NAME} main.c)

target_link_libraries(${PROJECT_NAME}
  vitaGL
  mathneon
  m
  SceLibKernel_stub
  SceThreadmgr_stub
  SceAppMgr_stub
  SceAppUtil_stub
  SceCommonDialog_stub
  SceDisplay_stub
  SceGxm_stub
  SceSysmodule_stub
  SceCtrl_stub
  SceTouch_stub
  SceMotion_stub
  SceHid_stub
  SceShaccCg_stub
  SceKernelDmacmgr_stub
  ScePower_stub
)

vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME})
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
)
