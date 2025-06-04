
PROJECT(xiaozhi-emu VERSION 1 LANGUAGES CXX)

add_executable(${PROJECT_NAME} 
  porting/entry.cc
  porting/freertos/event_groups.cc
  porting/freertos/event_groups.h
  porting/freertos/task.cc
  porting/freertos/task.h
  porting/impl/ui_thread.cc
  porting/impl/ui_thread.h
  porting/nvs_flash.cc
  porting/nvs_flash.h
  porting/impl/opus_wrapper.cc
  porting/impl/opus_wrapper.h
  porting/impl/http_client.cc
  porting/impl/http_client.h
  porting/impl/paho_mqtt.cc
  porting/impl/paho_mqtt.h
  porting/impl/udp_client.cc
  porting/impl/udp_client.h
  porting/impl/board.cc
  porting/impl/fake_board.cc
  porting/impl/fake_board.h
  porting/impl/sdl_audio_codec.cc
  porting/impl/sdl_audio_codec.h
  porting/impl/sdl_audio_processor.cc
  porting/impl/sdl_audio_processor.h
  protocols/protocol.cc
  protocols/protocol.h
  protocols/mqtt_protocol.cc
  protocols/mqtt_protocol.h
  interface/mqtt.h
  interface/udp.h
  display/display.cc
  display/display.h
  settings.cc
  settings.h
  application.cc
  application.h
  audio_codec.cc
  audio_codec.h
  audio_processor.h
  background_task.cc
  background_task.h
  system_info.cc
  system_info.h
  ota.cc
  ota.h
  main.cc)


target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/porting
  ${CMAKE_CURRENT_SOURCE_DIR}/interface)

target_link_libraries(${PROJECT_NAME} PRIVATE 
  fmt::fmt
  cjson
  httplib::httplib
  PahoMqttCpp::paho-mqttpp3-static
  SDL3::SDL3
  MbedTLS::mbedcrypto
  Opus::opus)
