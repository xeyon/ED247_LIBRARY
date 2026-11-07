add_executable(chatbot chatbot.cpp)

target_link_libraries(chatbot
  PRIVATE
  Ed247::ed247
  EXPAT::EXPAT
  libtime_tools
  libutils
)

install(TARGETS chatbot RUNTIME DESTINATION ${PLATFORM_INSTALL_SUBDIR}bin)
