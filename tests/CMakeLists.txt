add_executable(audio_tracker_tests
    test_main.cpp
    test_types.cpp
    test_stream.cpp
    test_stream_be.cpp
    test_memory_stream.cpp
    test_file_stream.cpp
    test_model.cpp
    test_xm_delta.cpp
    test_xm_pattern_codec.cpp
    test_xm_reader.cpp
    test_xm_writer.cpp
    test_xm_roundtrip.cpp
    test_mod_cell.cpp
    test_mod_reader.cpp
    test_mod_writer.cpp
)

target_link_libraries(audio_tracker_tests PRIVATE audio_tracker)
add_test(NAME audio_tracker_tests COMMAND audio_tracker_tests)
