
PROJECT(xiaozhi-emu VERSION 1 LANGUAGES CXX)

add_executable(${PROJECT_NAME} 
  impl/paho_mqtt.cc
  impl/paho_mqtt.h
  impl/udp_client.cc
  impl/udp_client.h
  protocols/protocol.cc
  protocols/protocol.h
  protocols/mqtt_protocol.cc
  protocols/mqtt_protocol.h
  interface/mqtt.h
  interface/udp.h
  settings.cc
  settings.h
  application.cc
  application.h
  audio_codec.cc
  audio_codec.h
  sdl_audio_codec.cc
  sdl_audio_codec.h
  audio_processor.h
  sdl_audio_processor.cc
  sdl_audio_processor.h
  opus_wrapper.cc
  opus_wrapper.h
  background_task.cc
  background_task.h
  main.cc)


target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/interface)

target_link_libraries(${PROJECT_NAME} PRIVATE 
  fmt::fmt
  cjson
  httplib::httplib
  PahoMqttCpp::paho-mqttpp3-static
  SDL3::SDL3
  MbedTLS::mbedcrypto
  Opus::opus)
