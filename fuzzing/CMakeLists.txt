#===================#
#     ds_fuzzer     #
#===================#

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=fuzzer")
add_executable(ds_fuzzer ds_fuzzer.cpp)
target_link_libraries(ds_fuzzer ${YDSH_STATIC})


#====================#
#     rpc_fuzzer     #
#====================#

include_directories(${CMAKE_SOURCE_DIR}/tools/json)
add_executable(rpc_fuzzer rpc_fuzzer.cpp)
target_link_libraries(rpc_fuzzer jsonrpc)


#====================#
#     lsp_fuzzer     #
#====================#

include_directories(${CMAKE_SOURCE_DIR}/tools/server)
add_executable(lsp_fuzzer lsp_fuzzer.cpp)
target_link_libraries(lsp_fuzzer ydshd_static)